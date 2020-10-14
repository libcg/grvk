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
    LOGT("%p 0x%X %u %p\n", device, queueType, queueId, pQueue);
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
    LOGT("%p %u %p %u %p %p\n", queue, cmdBufferCount, pCmdBuffers, memRefCount, pMemRefs, fence);
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

GR_RESULT grQueueWaitIdle(
    GR_QUEUE queue)
{
    LOGT("%p\n", queue);
    GrQueue* grQueue = (GrQueue*)queue;
    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grQueue->sType != GR_STRUCT_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkQueueWaitIdle(grQueue->queue);
    switch (res) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        return GR_SUCCESS;
    }
    return GR_ERROR_UNKNOWN;
}

GR_RESULT grDeviceWaitIdle(
    GR_DEVICE device)
{
    LOGT("%p\n", device);
    GrDevice* grDevice = (GrDevice*)device;
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkDeviceWaitIdle(grDevice->device);
    switch (res) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        return GR_SUCCESS;
    }
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_CREATE_INFO* pCreateInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pSemaphore);
    GrDevice* grDevice = (GrDevice*)device;
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pSemaphore == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    //TODO: add win32 semaphore support
    if (pCreateInfo->flags & GR_SEMAPHORE_CREATE_SHAREABLE) {
        return GR_UNSUPPORTED;
    }
    const VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VkSemaphore sem = VK_NULL_HANDLE;
    GrSemaphore* grSemaphore = malloc(sizeof(GrSemaphore));
    VkResult res = vki.vkCreateSemaphore(grDevice->device, &createInfo, NULL, &sem);
    switch (res) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        break;
    }
    *grSemaphore = (GrSemaphore) {
        .sType = GR_STRUCT_TYPE_QUEUE_SEMAPHORE,
        .semaphore = sem,
    };
    return GR_SUCCESS;
}

GR_RESULT grSignalQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    LOGT("%p %p\n", queue, semaphore);
    GrQueue* grQueue = (GrQueue*)queue;
    GrSemaphore* grSemaphore = (GrSemaphore*)semaphore;
    if (grQueue == NULL || grSemaphore == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grQueue->sType != GR_STRUCT_TYPE_QUEUE || grSemaphore->sType != GR_STRUCT_TYPE_QUEUE_SEMAPHORE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    const VkSemaphore signalSemaphore = grSemaphore->semaphore;
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 0,
        .pCommandBuffers = NULL,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signalSemaphore,
    };

    VkResult res = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    switch (res) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        break;
    }
    return GR_SUCCESS;
}

GR_RESULT grWaitQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    LOGT("%p %p\n", queue, semaphore);
    GrQueue* grQueue = (GrQueue*)queue;
    GrSemaphore* grSemaphore = (GrSemaphore*)semaphore;
    if (grQueue == NULL || grSemaphore == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grQueue->sType != GR_STRUCT_TYPE_QUEUE || grSemaphore->sType != GR_STRUCT_TYPE_QUEUE_SEMAPHORE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    const VkSemaphore waitSemaphore = grSemaphore->semaphore;
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 0,
        .pCommandBuffers = NULL,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    VkResult res = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    switch (res) {
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        break;
    }
    return GR_SUCCESS;
}
