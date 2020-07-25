#include "mantle_internal.h"

static uint32_t getVkQueueFamilyIndex(
    GrvkDevice* grvkDevice,
    GR_QUEUE_TYPE queueType)
{
    switch (queueType) {
    case GR_QUEUE_UNIVERSAL:
        return grvkDevice->universalQueueIndex;
    case GR_QUEUE_COMPUTE:
        return grvkDevice->computeQueueIndex;
    }

    printf("%s: invalid queue type %d\n", __func__, queueType);
    return INVALID_QUEUE_INDEX;
}

// Queue Functions

GR_RESULT grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    uint32_t queueIndex = getVkQueueFamilyIndex(grvkDevice, queueType);
    if (queueIndex == INVALID_QUEUE_INDEX) {
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    vki.vkGetDeviceQueue(grvkDevice->device, queueIndex, queueId, &vkQueue);

    GrvkQueue* grvkQueue = malloc(sizeof(GrvkQueue));
    *grvkQueue = (GrvkQueue) {
        .sType = GRVK_STRUCT_TYPE_QUEUE,
        .grvkDevice = grvkDevice,
        .queue = vkQueue,
        .queueIndex = queueIndex,
    };

    *pQueue = (GR_QUEUE)grvkQueue;
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
    GrvkQueue* grvkQueue = (GrvkQueue*)queue;
    GrvkFence* grvkFence = (GrvkFence*)fence;
    VkResult res;
    VkFence vkFence = VK_NULL_HANDLE;

    if (grvkFence != NULL) {
        vkFence = grvkFence->fence;

        if (vki.vkResetFences(grvkQueue->grvkDevice->device, 1, &vkFence) != VK_SUCCESS) {
            printf("%s: vkResetFences failed\n", __func__);
            return GR_ERROR_OUT_OF_MEMORY;
        }
    }

    VkCommandBuffer* vkCommandBuffers = malloc(sizeof(VkCommandBuffer) * cmdBufferCount);
    for (int i = 0; i < cmdBufferCount; i++) {
        vkCommandBuffers[i] = ((GrvkCmdBuffer*)pCmdBuffers[i])->commandBuffer;
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

    res = vki.vkQueueSubmit(grvkQueue->queue, 1, &submitInfo, vkFence);
    free(vkCommandBuffers);

    if (res != VK_SUCCESS) {
        printf("%s: vkQueueSubmit failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}
