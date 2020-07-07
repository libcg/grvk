#include "mantle_internal.h"

// Queue Functions

GR_RESULT grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    VkDevice vkDevice = ((GrvkDevice*)device)->device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    vkGetDeviceQueue(vkDevice, getVkQueueFamilyIndex(queueType), queueId, &vkQueue);

    GrvkQueue* grvkQueue = malloc(sizeof(GrvkQueue));
    *grvkQueue = (GrvkQueue) {
        .sType = GRVK_STRUCT_TYPE_QUEUE,
        .queue = vkQueue,
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
    VkQueue vkQueue = ((GrvkQueue*)queue)->queue;
    VkFence vkFence = VK_NULL_HANDLE;

    if ((GrvkFence*)fence != NULL) {
        vkFence = ((GrvkFence*)fence)->fence;
    }

    VkCommandBuffer* vkCommandBuffers = malloc(sizeof(VkCommandBuffer) * cmdBufferCount);
    for (int i = 0; i < cmdBufferCount; i++) {
        vkCommandBuffers[i] = ((GrvkCmdBuffer*)pCmdBuffers[i])->commandBuffer;
    }

    VkSubmitInfo submitInfo = {
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

    if (vkQueueSubmit(vkQueue, 1, &submitInfo, vkFence) != VK_SUCCESS) {
        printf("%s: vkQueueSubmit failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    free(vkCommandBuffers);

    return GR_SUCCESS;
}
