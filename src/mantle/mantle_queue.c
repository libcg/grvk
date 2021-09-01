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

static void prepareImagesForDataTransfer(
    const GrQueue* grQueue,
    unsigned imageCount,
    GrImage** images)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;
    VkImageMemoryBarrier* barriers = malloc(imageCount * sizeof(VkImageMemoryBarrier));

    // TODO create command pool and buffer in advance
    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = grQueue->queueIndex,
    };

    VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &vkCommandPool);

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, &vkCommandBuffer);

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    for (unsigned i = 0; i < imageCount; i++) {
        barriers[i] = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = getVkAccessFlagsImage(GR_IMAGE_STATE_DATA_TRANSFER),
            .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
            .newLayout = getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[i]->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, // TODO handle depth-stencil
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
    }

    VKD.vkBeginCommandBuffer(vkCommandBuffer, &beginInfo);
    VKD.vkCmdPipelineBarrier(vkCommandBuffer,
                             VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, 0, NULL, 0, NULL, imageCount, barriers);
    VKD.vkEndCommandBuffer(vkCommandBuffer);

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &vkCommandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    VKD.vkQueueWaitIdle(grQueue->queue); // TODO chain with a semaphore

    VKD.vkDestroyCommandPool(grDevice->device, vkCommandPool, NULL);
    free(barriers);
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
        prepareImagesForDataTransfer(grQueue, imageCount, images);
    }

    free(images);
}

// Queue Functions

GR_RESULT GR_STDCALL grGetDeviceQueue(
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
        LOGE("invalid index %d for queue type %d\n", queueIndex, queueType);
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
        .globalMemRefCount = 0,
        .globalMemRefs = NULL,
    };

    if (!mMemRefMutexInit) {
        InitializeCriticalSectionAndSpinCount(&mMemRefMutex, 0);
        mMemRefMutexInit = true;
    }

    *pQueue = (GR_QUEUE)grQueue;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grQueueSubmit(
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
    checkMemoryReferences(grQueue, grQueue->globalMemRefCount, grQueue->globalMemRefs);
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

GR_RESULT GR_STDCALL grQueueWaitIdle(
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

GR_RESULT GR_STDCALL grDeviceWaitIdle(
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

GR_RESULT GR_STDCALL grQueueSetGlobalMemReferences(
    GR_QUEUE queue,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs)
{
    LOGT("%p %u %p\n", queue, memRefCount, pMemRefs);
    GrQueue* grQueue = (GrQueue*)queue;

    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (memRefCount > 0 && pMemRefs == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    EnterCriticalSection(&mMemRefMutex);
    grQueue->globalMemRefCount = memRefCount;
    grQueue->globalMemRefs = (GR_MEMORY_REF*)pMemRefs;
    LeaveCriticalSection(&mMemRefMutex);

    return GR_SUCCESS;
}
