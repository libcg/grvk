#include "mantle_internal.h"

// Query and Synchronization Functions

GR_RESULT grCreateFence(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pFence);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate parameters

    VkFence vkFence = VK_NULL_HANDLE;

    const VkFenceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    VkResult res = vki.vkCreateFence(grDevice->device, &createInfo, NULL, &vkFence);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateFence failed (%d)\n", res);
        return getGrResult(res);
    }

    GrFence* grFence = malloc(sizeof(GrFence));
    *grFence = (GrFence) {
        .grObj = { GR_OBJ_TYPE_FENCE, grDevice },
        .fence = vkFence,
    };

    *pFence = (GR_FENCE)grFence;

    return GR_SUCCESS;
}

GR_RESULT grGetFenceStatus(
    GR_FENCE fence)
{
    LOGT("%p\n", fence);
    GrFence* grFence = (GrFence*)fence;

    if (fence == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grFence) != GR_OBJ_TYPE_FENCE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grFence);

    VkResult res = vki.vkGetFenceStatus(grDevice->device, grFence->fence);
    if (res != VK_SUCCESS && res != VK_NOT_READY) {
        LOGE("vkGetFenceStatus failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grWaitForFences(
    GR_DEVICE device,
    GR_UINT fenceCount,
    const GR_FENCE* pFences,
    GR_BOOL waitAll,
    GR_FLOAT timeout)
{
    LOGT("%p %u %p %d %g\n", device, fenceCount, pFences, waitAll, timeout);
    GrDevice* grDevice = (GrDevice*)device;
    uint64_t vkTimeout = (uint64_t)(timeout * 1000000000.0f); // Convert to nanoseconds

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (fenceCount == 0) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pFences == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    VkFence* vkFences = malloc(sizeof(VkFence) * fenceCount);
    for (int i = 0; i < fenceCount; i++) {
        GrFence* grFence = (GrFence*)pFences[i];

        if (grFence == NULL) {
            free(vkFences);
            return GR_ERROR_INVALID_HANDLE;
        }

        vkFences[i] = grFence->fence;
    }

    VkResult res = vki.vkWaitForFences(grDevice->device, fenceCount, vkFences, waitAll, vkTimeout);
    free(vkFences);

    if (res != VK_SUCCESS && res != VK_TIMEOUT) {
        LOGE("vkWaitForFences failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grCreateQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_CREATE_INFO* pCreateInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pSemaphore);
    GrDevice* grDevice = (GrDevice*)device;

    VkSemaphore vkSem = VK_NULL_HANDLE;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pSemaphore == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pCreateInfo->initialCount >= 32) {
        return GR_ERROR_INVALID_VALUE;
    }

    if (pCreateInfo->flags & GR_SEMAPHORE_CREATE_SHAREABLE) {
        // TODO add win32 semaphore support
        LOGW("unhandled shareable semaphore flag\n");
    }

    const VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    VkResult res = vki.vkCreateSemaphore(grDevice->device, &createInfo, NULL, &vkSem);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed (%d)\n", res);
        return getGrResult(res);
    }

    GrQueueSemaphore* grQueueSemaphore = malloc(sizeof(GrQueueSemaphore));
    *grQueueSemaphore = (GrQueueSemaphore) {
        .grObj = { GR_OBJ_TYPE_QUEUE_SEMAPHORE, grDevice },
        .semaphore = vkSem,
    };

    return GR_SUCCESS;
}

GR_RESULT grSignalQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    LOGT("%p %p\n", queue, semaphore);
    GrQueue* grQueue = (GrQueue*)queue;
    GrQueueSemaphore* grQueueSemaphore = (GrQueueSemaphore*)semaphore;

    if (grQueue == NULL || grQueueSemaphore == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 0,
        .pCommandBuffers = NULL,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &grQueueSemaphore->semaphore,
    };

    VkResult res = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grWaitQueueSemaphore(
    GR_QUEUE queue,
    GR_QUEUE_SEMAPHORE semaphore)
{
    LOGT("%p %p\n", queue, semaphore);
    GrQueue* grQueue = (GrQueue*)queue;
    GrQueueSemaphore* grQueueSemaphore = (GrQueueSemaphore*)semaphore;

    if (grQueue == NULL || grQueueSemaphore == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &grQueueSemaphore->semaphore,
        .pWaitDstStageMask = &stageMask,
        .commandBufferCount = 0,
        .pCommandBuffers = NULL,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    VkResult res = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grCreateEvent(
    GR_DEVICE device,
    const GR_EVENT_CREATE_INFO* pCreateInfo,
    GR_EVENT* pEvent)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pEvent);
    GrDevice* grDevice = (GrDevice*)device;

    VkEvent vkEvent = VK_NULL_HANDLE;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pEvent == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const VkEventCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    VkResult res = vki.vkCreateEvent(grDevice->device, &createInfo, NULL, &vkEvent);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateEvent failed (%d)\n", res);
        return getGrResult(res);
    }

    GrEvent* grEvent = malloc(sizeof(GrEvent));
    *grEvent = (GrEvent) {
        .grObj = { GR_OBJ_TYPE_EVENT, grDevice },
        .event = vkEvent,
    };

    *pEvent = (GR_EVENT)grEvent;
    return GR_SUCCESS;
}

GR_RESULT grGetEventStatus(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;

    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grEvent) != GR_OBJ_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grEvent);

    VkResult res = vki.vkGetEventStatus(grDevice->device, grEvent->event);
    if (res != VK_EVENT_SET && res != VK_EVENT_SET) {
        LOGE("vkGetEventStatus failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grSetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;

    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grEvent) != GR_OBJ_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grEvent);

    VkResult res = vki.vkSetEvent(grDevice->device, grEvent->event);
    if (res != VK_SUCCESS) {
        LOGE("vkSetEvent failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT grResetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;

    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grEvent) != GR_OBJ_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grEvent);

    VkResult res = vki.vkResetEvent(grDevice->device, grEvent->event);
    if (res != VK_SUCCESS) {
        LOGE("vkResetEvent failed (%d)\n", res);
    }

    return getGrResult(res);
}
