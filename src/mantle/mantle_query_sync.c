#include "mantle_internal.h"

// Query and Synchronization Functions

GR_RESULT grCreateFence(
    GR_DEVICE device,
    const GR_FENCE_CREATE_INFO* pCreateInfo,
    GR_FENCE* pFence)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pFence);
    GrDevice* grDevice = (GrDevice*)device;

    VkFence vkFence = VK_NULL_HANDLE;

    const VkFenceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    if (vki.vkCreateFence(grDevice->device, &createInfo, NULL, &vkFence) != VK_SUCCESS) {
        LOGE("vkCreateFence failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrFence* grFence = malloc(sizeof(GrFence));
    *grFence = (GrFence) {
        .sType = GR_STRUCT_TYPE_FENCE,
        .device = grDevice->device,
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
    } else if (grFence->sType != GR_STRUCT_TYPE_FENCE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = vki.vkGetFenceStatus(grFence->device, grFence->fence);

    if (res == VK_NOT_READY) {
        return GR_NOT_READY;
    } else if (res != VK_SUCCESS) {
        LOGE("vkGetFenceStatus failed %d\n", res);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
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

    if (res == VK_TIMEOUT) {
        return GR_TIMEOUT;
    } else if (res != VK_SUCCESS) {
        LOGE("vkWaitForFences failed %d\n", res);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
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

    if (vki.vkCreateSemaphore(grDevice->device, &createInfo, NULL, &vkSem) != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrQueueSemaphore* grQueueSemaphore = malloc(sizeof(GrQueueSemaphore));
    *grQueueSemaphore = (GrQueueSemaphore) {
        .sType = GR_STRUCT_TYPE_QUEUE_SEMAPHORE,
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
    } else if (grQueue->sType != GR_STRUCT_TYPE_QUEUE) {
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

    if (vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
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
    } else if (grQueue->sType != GR_STRUCT_TYPE_QUEUE) {
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

    if (vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pEvent == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const VkEventCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    if (vki.vkCreateEvent(grDevice->device, &createInfo, NULL, &vkEvent) != VK_SUCCESS) {
        LOGE("vkCreateEvent failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrEvent* grEvent = malloc(sizeof(GrEvent));
    *grEvent = (GrEvent) {
        .sType = GR_STRUCT_TYPE_EVENT,
        .device = grDevice->device,
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
    } else if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = vki.vkGetEventStatus(grEvent->device, grEvent->event);

    if (res == VK_EVENT_SET) {
        return GR_EVENT_SET;
    } else if (res == VK_EVENT_RESET) {
        return GR_EVENT_RESET;
    } else {
        LOGE("vkGetEventStatus failed %d\n", res);
        return GR_ERROR_OUT_OF_MEMORY;
    }
}

GR_RESULT grSetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;

    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    if (vki.vkSetEvent(grEvent->device, grEvent->event) != VK_SUCCESS) {
        LOGE("vkSetEvent failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

GR_RESULT grResetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;

    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    if (vki.vkResetEvent(grEvent->device, grEvent->event) != VK_SUCCESS) {
        LOGE("vkResetEvent failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}
