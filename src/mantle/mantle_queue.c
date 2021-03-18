#include "mantle_internal.h"

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

    vki.vkGetDeviceQueue(grDevice->device, queueIndex, queueId, &vkQueue);

    // FIXME technically, this object should be created in advance so it doesn't get duplicated
    // when the application requests it multiple times
    GrQueue* grQueue = malloc(sizeof(GrQueue));
    *grQueue = (GrQueue) {
        .grObj = { GR_OBJ_TYPE_QUEUE, grDevice },
        .queue = vkQueue,
        .queueIndex = queueIndex,
    };

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

    if (grFence != NULL) {
        GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

        vkFence = grFence->fence;

        res = vki.vkResetFences(grDevice->device, 1, &vkFence);
        if (res != VK_SUCCESS) {
            LOGE("vkResetFences failed (%d)\n", res);
            return getGrResult(res);
        }
    }

    VkCommandBuffer* vkCommandBuffers = malloc(sizeof(VkCommandBuffer) * cmdBufferCount);
    for (int i = 0; i < cmdBufferCount; i++) {
        vkCommandBuffers[i] = ((GrCmdBuffer*)pCmdBuffers[i])->commandBuffer;
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

    res = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, vkFence);
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

    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = vki.vkQueueWaitIdle(grQueue->queue);
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

    VkResult res = vki.vkDeviceWaitIdle(grDevice->device);
    if (res != VK_SUCCESS) {
        LOGE("vkDeviceWaitIdle failed (%d)\n", res);
    }

    return getGrResult(res);
}
