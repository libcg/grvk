#include "mantle_internal.h"

// Query and Synchronization Functions

GR_RESULT GR_STDCALL grCreateQueryPool(
    GR_DEVICE device,
    const GR_QUERY_POOL_CREATE_INFO* pCreateInfo,
    GR_QUERY_POOL* pQueryPool)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pQueryPool);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pQueryPool == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pCreateInfo->slots == 0) {
        return GR_ERROR_INVALID_VALUE;
    }

    VkQueryPool vkQueryPool = VK_NULL_HANDLE;

    const VkQueryPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queryType = getVkQueryType(pCreateInfo->queryType),
        .queryCount = pCreateInfo->slots,
        .pipelineStatistics =
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
            VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,
    };

    VkResult res = VKD.vkCreateQueryPool(grDevice->device, &createInfo, NULL, &vkQueryPool);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateQueryPool failed (%d)\n", res);
        return getGrResult(res);
    }

    GrQueryPool *grQueryPool = malloc(sizeof(GrQueryPool));
    *grQueryPool = (GrQueryPool) {
        .grObj = { GR_OBJ_TYPE_QUERY_POOL, grDevice },
        .queryPool = vkQueryPool,
        .queryType = createInfo.queryType,
    };

    *pQueryPool = (GR_QUERY_POOL)grQueryPool;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetQueryPoolResults(
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p %u %u %p %p\n", queryPool, startQuery, queryCount, pDataSize, pData);
    GrQueryPool* grQueryPool = (GrQueryPool*)queryPool;

    if (grQueryPool == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueryPool) != GR_OBJ_TYPE_QUERY_POOL) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueryPool);
    unsigned elemSize = grQueryPool->queryType == VK_QUERY_TYPE_OCCLUSION ?
                        sizeof(uint64_t) : sizeof(GR_PIPELINE_STATISTICS_DATA);

    if (pData == NULL) {
        *pDataSize = queryCount * elemSize;
        return GR_SUCCESS;
    } else if (*pDataSize < queryCount * elemSize) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    VkResult vkRes = VKD.vkGetQueryPoolResults(grDevice->device, grQueryPool->queryPool,
                                               startQuery, queryCount, *pDataSize, pData,
                                               elemSize, VK_QUERY_RESULT_64_BIT);
    if (vkRes != VK_SUCCESS && vkRes != VK_NOT_READY) {
        LOGE("vkGetQueryPoolResults failed (%d)\n", vkRes);
    } else if (vkRes == VK_SUCCESS && grQueryPool->queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
        GR_PIPELINE_STATISTICS_DATA* stats = (GR_PIPELINE_STATISTICS_DATA*)pData;

        // Reorder results to match Mantle (see VkQueryPipelineStatisticFlags)
        for (unsigned i = 0; i < queryCount; i++) {
            const uint64_t* vkStat = &((uint64_t*)pData)[11 * i];

            stats[i] = (GR_PIPELINE_STATISTICS_DATA) {
                .psInvocations  = vkStat[7],
                .cPrimitives    = vkStat[6],
                .cInvocations   = vkStat[5],
                .vsInvocations  = vkStat[2],
                .gsInvocations  = vkStat[3],
                .gsPrimitives   = vkStat[4],
                .iaPrimitives   = vkStat[1],
                .iaVertices     = vkStat[0],
                .hsInvocations  = vkStat[8],
                .dsInvocations  = vkStat[9],
                .csInvocations  = vkStat[10],
            };
        }
    }

    return getGrResult(vkRes);
}

GR_RESULT GR_STDCALL grCreateFence(
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

    VkResult res = VKD.vkCreateFence(grDevice->device, &createInfo, NULL, &vkFence);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateFence failed (%d)\n", res);
        return getGrResult(res);
    }

    GrFence* grFence = malloc(sizeof(GrFence));
    *grFence = (GrFence) {
        .grObj = { GR_OBJ_TYPE_FENCE, grDevice },
        .fence = vkFence,
        .submitted = false,
    };

    *pFence = (GR_FENCE)grFence;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetFenceStatus(
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

    if (!grFence->submitted) {
        return GR_ERROR_UNAVAILABLE;
    }

    VkResult res = VKD.vkGetFenceStatus(grDevice->device, grFence->fence);
    if (res != VK_SUCCESS && res != VK_NOT_READY) {
        LOGE("vkGetFenceStatus failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grWaitForFences(
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

    STACK_ARRAY(VkFence, vkFences, 1024, fenceCount);

    for (unsigned i = 0; i < fenceCount; i++) {
        GrFence* grFence = (GrFence*)pFences[i];

        if (grFence == NULL) {
            free(vkFences);
            return GR_ERROR_INVALID_HANDLE;
        } else if (!grFence->submitted) {
            free(vkFences);
            return GR_ERROR_UNAVAILABLE;
        }

        vkFences[i] = grFence->fence;
    }

    VkResult res = VKD.vkWaitForFences(grDevice->device, fenceCount, vkFences, waitAll, vkTimeout);

    STACK_ARRAY_FINISH(vkFences);

    if (res != VK_SUCCESS && res != VK_TIMEOUT) {
        LOGE("vkWaitForFences failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grCreateQueueSemaphore(
    GR_DEVICE device,
    const GR_QUEUE_SEMAPHORE_CREATE_INFO* pCreateInfo,
    GR_QUEUE_SEMAPHORE* pSemaphore)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pSemaphore);
    GrDevice* grDevice = (GrDevice*)device;

    VkSemaphore vkSemaphore = VK_NULL_HANDLE;

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

    VkResult res = VKD.vkCreateSemaphore(grDevice->device, &createInfo, NULL, &vkSemaphore);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed (%d)\n", res);
        return getGrResult(res);
    }

    GrQueueSemaphore* grQueueSemaphore = malloc(sizeof(GrQueueSemaphore));
    *grQueueSemaphore = (GrQueueSemaphore) {
        .grObj = { GR_OBJ_TYPE_QUEUE_SEMAPHORE, grDevice },
        .semaphore = vkSemaphore,
        .currentCount = pCreateInfo->initialCount,
    };

    *pSemaphore = (GR_QUEUE_SEMAPHORE)grQueueSemaphore;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grSignalQueueSemaphore(
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

    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

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

    AcquireSRWLockExclusive(&grQueue->queueLock);
    VkResult res = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grWaitQueueSemaphore(
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

    if (grQueueSemaphore->currentCount > 0) {
        // Mantle spec: "At creation time, an application can specify an initial semaphore count
        // that is equivalent to signaling the semaphore that many times."
        grQueueSemaphore->currentCount--;
        return GR_SUCCESS;
    }

    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

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

    AcquireSRWLockExclusive(&grQueue->queueLock);
    VkResult res = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grCreateEvent(
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

    VkResult res = VKD.vkCreateEvent(grDevice->device, &createInfo, NULL, &vkEvent);
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

GR_RESULT GR_STDCALL grGetEventStatus(
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

    VkResult res = VKD.vkGetEventStatus(grDevice->device, grEvent->event);
    if (res != VK_EVENT_RESET && res != VK_EVENT_SET) {
        LOGE("vkGetEventStatus failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grSetEvent(
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

    VkResult res = VKD.vkSetEvent(grDevice->device, grEvent->event);
    if (res != VK_SUCCESS) {
        LOGE("vkSetEvent failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grResetEvent(
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

    VkResult res = VKD.vkResetEvent(grDevice->device, grEvent->event);
    if (res != VK_SUCCESS) {
        LOGE("vkResetEvent failed (%d)\n", res);
    }

    return getGrResult(res);
}
