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
    VkResult res;

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
        if (grFence == NULL || grFence->sType != GR_STRUCT_TYPE_FENCE) {
            free(vkFences);
            return GR_ERROR_INVALID_HANDLE;
        }

        vkFences[i] = grFence->fence;
    }

    res = vki.vkWaitForFences(grDevice->device, fenceCount, vkFences, waitAll, vkTimeout);
    free(vkFences);

    if (res == VK_SUCCESS) {
        return GR_SUCCESS;
    } else if (res == VK_TIMEOUT) {
        return GR_TIMEOUT;
    } else {
        LOGE("vkWaitForFences failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }
}


GR_RESULT grGetFenceStatus(
    GR_FENCE fence)
{
    LOGT("%p\n", fence);
    GrFence* grFence = (GrFence*)fence;
    if (fence == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grFence->sType != GR_STRUCT_TYPE_FENCE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkGetFenceStatus(grFence->device, grFence->fence);
    switch (res) {
    case VK_NOT_READY:
        return GR_NOT_READY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        return GR_ERROR_UNKNOWN;
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
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (pCreateInfo == NULL || pEvent == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    const VkEventCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VkEvent event = VK_NULL_HANDLE;
    VkResult res = vki.vkCreateEvent(grDevice->device, &createInfo, NULL, &event);

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

    GrEvent* grEvent = malloc(sizeof(GrEvent));
    *grEvent = (GrEvent) {
        .sType = GR_STRUCT_TYPE_EVENT,
        .device = grDevice->device,
        .event = event,
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
    }
    if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkGetEventStatus(grEvent->device, grEvent->event);
    switch (res) {
    case VK_EVENT_SET:
        return GR_EVENT_SET;
    case VK_EVENT_RESET:
        return GR_EVENT_RESET;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    default:
        break;
    }
    return GR_UNSUPPORTED;
}

GR_RESULT grSetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;
    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkSetEvent(grEvent->device, grEvent->event);
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

GR_RESULT grResetEvent(
    GR_EVENT event)
{
    LOGT("%p\n", event);
    GrEvent* grEvent = (GrEvent*)event;
    if (grEvent == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grEvent->sType != GR_STRUCT_TYPE_EVENT) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkResult res = vki.vkResetEvent(grEvent->device, grEvent->event);
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
