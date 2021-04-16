#include "mantle_internal.h"

static CRITICAL_SECTION mMemRefMutex;
static bool mMemRefMutexInit = false;

static unsigned getVkQueueFamilyIndex(
    GrDevice* grDevice,
    GR_QUEUE_TYPE queueType)
{
    switch (queueType) {
    case GR_QUEUE_UNIVERSAL:
        return grDevice->universalQueueIndex;
    case GR_QUEUE_COMPUTE:
        return grDevice->computeQueueIndex;
    }

    LOGE("invalid queue type %d\n", queueType);
    return INVALID_QUEUE_INDEX;
}

static void prepareImages(
    const GrQueue* grQueue,
    unsigned imageCount,
    GrImage** images,
    GR_IMAGE_STATE newState)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);
    GR_IMAGE_STATE_TRANSITION* stateTransitions =
        malloc(imageCount * sizeof(GR_IMAGE_STATE_TRANSITION));

    for (unsigned i = 0; i < imageCount; i++) {
        stateTransitions[i] = (GR_IMAGE_STATE_TRANSITION) {
            .image = (GR_IMAGE)images[i],
            .oldState = GR_IMAGE_STATE_UNINITIALIZED,
            .newState = newState,
            .subresourceRange = {
                .aspect = GR_IMAGE_ASPECT_COLOR,
                .baseMipLevel = 0,
                .mipLevels = GR_LAST_MIP_OR_SLICE,
                .baseArraySlice = 0,
                .arraySize = GR_LAST_MIP_OR_SLICE,
            },
        };
    }

    const GR_CMD_BUFFER_CREATE_INFO cmdBufferCreateInfo = {
        .queueType = GR_QUEUE_UNIVERSAL,
        .flags = 0,
    };

    // TODO create cmd buffer in advance
    GR_CMD_BUFFER cmdBuffer = GR_NULL_HANDLE;
    grCreateCommandBuffer((GR_DEVICE)grDevice, &cmdBufferCreateInfo, &cmdBuffer);

    grBeginCommandBuffer(cmdBuffer, GR_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT);
    grCmdPrepareImages(cmdBuffer, imageCount, stateTransitions);
    grEndCommandBuffer(cmdBuffer);

    grQueueSubmit((GR_QUEUE)grQueue, 1, &cmdBuffer, 0, NULL, GR_NULL_HANDLE);
    grQueueWaitIdle((GR_QUEUE)grQueue); // TODO use a fence, or better, a semaphore...

    grDestroyObject(cmdBuffer);
    free(stateTransitions);
}

static void checkMemoryReferences(
    const GrQueue* grQueue,
    unsigned memRefCount,
    const GR_MEMORY_REF* memRefs)
{
    unsigned imageCount = 0;
    GrImage** images = NULL;

    // Check if image references need implicit data transfer state transition
    for (unsigned i = 0; i < memRefCount; i++) {
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)memRefs[i].mem;

        EnterCriticalSection(&grGpuMemory->boundObjectsMutex);
        for (unsigned j = 0; j < grGpuMemory->boundObjectCount; j++) {
            GrObject* grBoundObject = grGpuMemory->boundObjects[j];

            if (grBoundObject->grObjType == GR_OBJ_TYPE_IMAGE) {
                GrImage* grImage = (GrImage*)grBoundObject;

                if (grImage->needInitialDataTransferState) {
                    imageCount++;
                    images = realloc(images, imageCount * sizeof(GrImage*));
                    images[imageCount - 1] = grImage;

                    grImage->needInitialDataTransferState = false;
                }
            }
        }
        LeaveCriticalSection(&grGpuMemory->boundObjectsMutex);
    }

    // Perform data transfer state transition
    if (imageCount > 0) {
        prepareImages(grQueue, imageCount, images, GR_IMAGE_STATE_DATA_TRANSFER);
    }

    free(images);
}

// Queue Functions

GR_RESULT grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    LOGT("%p 0x%X %u %p\n", device, queueType, queueId, pQueue);
    GrDevice* grDevice = (GrDevice*)device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    unsigned queueIndex = getVkQueueFamilyIndex(grDevice, queueType);
    if (queueIndex == INVALID_QUEUE_INDEX) {
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    VKD.vkGetDeviceQueue(grDevice->device, queueIndex, queueId, &vkQueue);

    // FIXME technically, this object should be created in advance so it doesn't get duplicated
    // when the application requests it multiple times
    GrQueue* grQueue = malloc(sizeof(GrQueue));
    *grQueue = (GrQueue) {
        .grObj = { GR_OBJ_TYPE_QUEUE, grDevice },
        .queue = vkQueue,
        .queueIndex = queueIndex,
    };

    if (!mMemRefMutexInit) {
        InitializeCriticalSectionAndSpinCount(&mMemRefMutex, 0);
        mMemRefMutexInit = true;
    }

    *pQueue = (GR_QUEUE)grQueue;
    return GR_SUCCESS;
}

GR_RESULT grQueueSubmit(
    GR_QUEUE queue,
    GR_UINT cmdBufferCount,
    const GR_CMD_BUFFER* pCmdBuffers,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs,
    GR_FENCE fence)
{
    LOGT("%p %u %p %u %p %p\n", queue, cmdBufferCount, pCmdBuffers, memRefCount, pMemRefs, fence);
    GrQueue* grQueue = (GrQueue*)queue;
    GrFence* grFence = (GrFence*)fence;
    VkFence vkFence = VK_NULL_HANDLE;
    VkResult res;

    // TODO validate args

    GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

    EnterCriticalSection(&mMemRefMutex);
    checkMemoryReferences(grQueue, memRefCount, pMemRefs);
    LeaveCriticalSection(&mMemRefMutex);

    if (grFence != NULL) {
        vkFence = grFence->fence;

        res = VKD.vkResetFences(grDevice->device, 1, &vkFence);
        if (res != VK_SUCCESS) {
            LOGE("vkResetFences failed (%d)\n", res);
            return getGrResult(res);
        }

        grFence->submitted = true;
    }

    VkCommandBuffer* vkCommandBuffers = malloc(sizeof(VkCommandBuffer) * cmdBufferCount);
    for (unsigned i = 0; i < cmdBufferCount; i++) {
        GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)pCmdBuffers[i];

        grCmdBuffer->submitFence = grFence;
        vkCommandBuffers[i] = grCmdBuffer->commandBuffer;
    }

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = cmdBufferCount,
        .pCommandBuffers = vkCommandBuffers,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    res = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, vkFence);
    free(vkCommandBuffers);

    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grQueueWaitIdle(
    GR_QUEUE queue)
{
    LOGT("%p\n", queue);
    GrQueue* grQueue = (GrQueue*)queue;
    GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = VKD.vkQueueWaitIdle(grQueue->queue);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueWaitIdle failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grDeviceWaitIdle(
    GR_DEVICE device)
{
    LOGT("%p\n", device);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = VKD.vkDeviceWaitIdle(grDevice->device);
    if (res != VK_SUCCESS) {
        LOGE("vkDeviceWaitIdle failed (%d)\n", res);
    }

    return getGrResult(res);
}
