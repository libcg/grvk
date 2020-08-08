#include "mantle_internal.h"

static uint32_t getVkQueueFamilyIndex(
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
    GrDevice* grDevice = (GrDevice*)device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    uint32_t queueIndex = getVkQueueFamilyIndex(grDevice, queueType);
    if (queueIndex == INVALID_QUEUE_INDEX) {
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    vki.vkGetDeviceQueue(grDevice->device, queueIndex, queueId, &vkQueue);

    GrQueue* grQueue = malloc(sizeof(GrQueue));
    *grQueue = (GrQueue) {
        .sType = GR_STRUCT_TYPE_QUEUE,
        .grDevice = grDevice,
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
    GrQueue* grQueue = (GrQueue*)queue;
    GrFence* grFence = (GrFence*)fence;
    VkResult res;
    VkFence vkFence = VK_NULL_HANDLE;

    if (grFence != NULL) {
        vkFence = grFence->fence;

        if (vki.vkResetFences(grQueue->grDevice->device, 1, &vkFence) != VK_SUCCESS) {
            LOGE("vkResetFences failed\n");
            return GR_ERROR_OUT_OF_MEMORY;
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
        LOGE("vkQueueSubmit failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}
