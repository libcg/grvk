#include "mantle_internal.h"
#include "mantle_object.h"

// Generic API Object Management functions

GR_RESULT GR_STDCALL grDestroyObject(
    GR_OBJECT object)
{
    LOGT("%p\n", object);
    GrObject* grObject = (GrObject*)object;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grObject);

    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }

    switch (grObject->grObjType) {
    case GR_OBJ_TYPE_COMMAND_BUFFER: {
        GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)grObject;

        VKD.vkDestroyCommandPool(grDevice->device, grCmdBuffer->commandPool, NULL);
        VKD.vkDestroyQueryPool(grDevice->device, grCmdBuffer->timestampQueryPool, NULL);
        for (unsigned i = 0; i < grCmdBuffer->descriptorPoolCount; i++) {
            VKD.vkDestroyDescriptorPool(grDevice->device, grCmdBuffer->descriptorPools[i], NULL);
        }
        free(grCmdBuffer->descriptorPools);
    }   break;
    case GR_OBJ_TYPE_COLOR_BLEND_STATE_OBJECT:
        // Nothing to do
        break;
    case GR_OBJ_TYPE_COLOR_TARGET_VIEW: {
        GrColorTargetView* grColorTargetView = (GrColorTargetView*)grObject;

        VKD.vkDestroyImageView(grDevice->device, grColorTargetView->imageView, NULL);
    }   break;
    case GR_OBJ_TYPE_DEPTH_STENCIL_STATE_OBJECT:
        // Nothing to do
        break;
    case GR_OBJ_TYPE_DEPTH_STENCIL_VIEW: {
        GrDepthStencilView* grDepthStencilView = (GrDepthStencilView*)grObject;

        VKD.vkDestroyImageView(grDevice->device, grDepthStencilView->imageView, NULL);
    }   break;
    case GR_OBJ_TYPE_DESCRIPTOR_SET: {
        GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)grObject;

        grClearDescriptorSetSlots(grDescriptorSet, 0, grDescriptorSet->slotCount);
        free(grDescriptorSet->slots);
    }   break;
    case GR_OBJ_TYPE_EVENT: {
        GrEvent* grEvent = (GrEvent*)grObject;

        VKD.vkDestroyEvent(grDevice->device, grEvent->event, NULL);
    }   break;
    case GR_OBJ_TYPE_FENCE: {
        GrFence* grFence = (GrFence*)grObject;

        VKD.vkDestroyFence(grDevice->device, grFence->fence, NULL);
    }   break;
    case GR_OBJ_TYPE_IMAGE: {
        GrImage* grImage = (GrImage*)grObject;

        if (grImage->image != VK_NULL_HANDLE) {
            VKD.vkDestroyImage(grDevice->device, grImage->image, NULL);
        }
        if (grImage->buffer != VK_NULL_HANDLE) {
            VKD.vkDestroyBuffer(grDevice->device, grImage->buffer, NULL);
        }
        if (grImage->imageAllocation != VK_NULL_HANDLE) {
            vmaFreeMemory(grDevice->allocator, grImage->imageAllocation);
        }
        if (grImage->bufferAllocation != VK_NULL_HANDLE) {
            vmaFreeMemory(grDevice->allocator, grImage->bufferAllocation);
        }

        grQueueRemoveInitialImage(grImage);
        grWsiDestroyImage(grImage);
    }   break;
    case GR_OBJ_TYPE_IMAGE_VIEW: {
        GrImageView* grImageView = (GrImageView*)grObject;

        VKD.vkDestroyImageView(grDevice->device, grImageView->imageView, NULL);
    }   break;
    case GR_OBJ_TYPE_MSAA_STATE_OBJECT:
        // Nothing to do
        break;
    case GR_OBJ_TYPE_PIPELINE:
        // TODO
        break;
    case GR_OBJ_TYPE_QUEUE_SEMAPHORE: {
        GrQueueSemaphore* grQueueSemaphore = (GrQueueSemaphore*)grObject;

        VKD.vkDestroySemaphore(grDevice->device, grQueueSemaphore->semaphore, NULL);
    }   break;
    case GR_OBJ_TYPE_RASTER_STATE_OBJECT:
        // Nothing to do
        break;
    case GR_OBJ_TYPE_SAMPLER: {
        GrSampler* grSampler = (GrSampler*)grObject;

        VKD.vkDestroySampler(grDevice->device, grSampler->sampler, NULL);
    }   break;
    case GR_OBJ_TYPE_SHADER:
        // FIXME actually destroy it?
        return GR_SUCCESS;
    case GR_OBJ_TYPE_QUERY_POOL: {
        GrQueryPool* grQueryPool = (GrQueryPool*)grObject;

        VKD.vkDestroyQueryPool(grDevice->device, grQueryPool->queryPool, NULL);
    }   break;
    case GR_OBJ_TYPE_VIEWPORT_STATE_OBJECT: {
        GrViewportStateObject* grViewportStateObject = (GrViewportStateObject*)grObject;

        free(grViewportStateObject->viewports);
        free(grViewportStateObject->scissors);
    }   break;
    case GR_OBJ_TYPE_WSI_WIN_DISPLAY:
        // Nothing to do
        break;
    default:
        LOGW("unsupported object type %u\n", grObject->grObjType);
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    free(grObject);
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetObjectInfo(
    GR_BASE_OBJECT object,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X %p %p\n", object, infoType, pDataSize, pData);
    GrBaseObject* grBaseObject = (GrBaseObject*)object;

    if (grBaseObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    GrObjectType objType = GET_OBJ_TYPE(grBaseObject);
    unsigned expectedSize = 0;

    switch (infoType) {
    case GR_INFO_TYPE_MEMORY_REQUIREMENTS: {
        GR_MEMORY_REQUIREMENTS* grMemReqs = (GR_MEMORY_REQUIREMENTS*)pData;
        VkMemoryRequirements memReqs;

        expectedSize = sizeof(GR_MEMORY_REQUIREMENTS);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_MEMORY_REQUIREMENTS, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        switch (objType) {
        case GR_OBJ_TYPE_IMAGE: {
            GrImage* grImage = (GrImage*)grBaseObject;
            GrDevice* grDevice = GET_OBJ_DEVICE(grBaseObject);

            if (grImage->buffer != VK_NULL_HANDLE) {
                VKD.vkGetBufferMemoryRequirements(grDevice->device, grImage->buffer, &memReqs);
            } else {
                VKD.vkGetImageMemoryRequirements(grDevice->device, grImage->image, &memReqs);
            }

            *grMemReqs = getGrMemoryRequirements(grDevice, memReqs);
        }   break;
        case GR_OBJ_TYPE_BORDER_COLOR_PALETTE:
        case GR_OBJ_TYPE_COLOR_TARGET_VIEW:
        case GR_OBJ_TYPE_DEPTH_STENCIL_VIEW:
        case GR_OBJ_TYPE_DESCRIPTOR_SET:
        case GR_OBJ_TYPE_EVENT:
        case GR_OBJ_TYPE_FENCE:
        case GR_OBJ_TYPE_IMAGE_VIEW:
        case GR_OBJ_TYPE_PIPELINE:
        case GR_OBJ_TYPE_QUERY_POOL:
        case GR_OBJ_TYPE_QUEUE_SEMAPHORE:
            // Mantle spec: "Not all objects have memory requirements, in which case it is valid
            // for the requirements structure to return zero size and alignment, and no heaps."
            *grMemReqs = (GR_MEMORY_REQUIREMENTS) {
                // No actual allocation will be done with a heap count of 0. See grAllocMemory.
                .size = quirkHas(QUIRK_NON_ZERO_MEM_REQ) ? 4096 : 0,
                .alignment = quirkHas(QUIRK_NON_ZERO_MEM_REQ) ? 4 : 0,
                .heapCount = 0,
            };
            break;
        default:
            LOGW("unsupported type %d for info type 0x%X\n", objType, infoType);
            return GR_ERROR_INVALID_VALUE;
        }
    }   break;
    case GR_WSI_WIN_INFO_TYPE_QUEUE_PROPERTIES: {
        GR_WSI_WIN_QUEUE_PROPERTIES* grQueueProps = (GR_WSI_WIN_QUEUE_PROPERTIES*)pData;
        const GrQueue* grQueue = (GrQueue*)grBaseObject;
        const GrDevice* grDevice = GET_OBJ_DEVICE(grBaseObject);

        expectedSize = sizeof(GR_WSI_WIN_QUEUE_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_WSI_WIN_QUEUE_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        // FIXME check queue capability
        // TODO present from compute
        *grQueueProps = (GR_WSI_WIN_QUEUE_PROPERTIES) {
            .presentSupport = grQueue == grDevice->grUniversalQueue ?
                              GR_WSI_WIN_FULLSCREEN_PRESENT_SUPPORTED |
                              GR_WSI_WIN_WINDOWED_PRESENT_SUPPORTED : 0,
        };
    }   break;
    case GR_WSI_WIN_INFO_TYPE_DISPLAY_PROPERTIES: {
        GR_WSI_WIN_DISPLAY_PROPERTIES* grDisplayProps = (GR_WSI_WIN_DISPLAY_PROPERTIES*)pData;
        const GrWsiWinDisplay* grWsiWinDisplay = (GrWsiWinDisplay*)grBaseObject;

        expectedSize = sizeof(GR_WSI_WIN_DISPLAY_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_WSI_WIN_DISPLAY_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        MONITORINFOEX monitorInfo = { .cbSize = sizeof(MONITORINFOEX) };
        GetMonitorInfo(grWsiWinDisplay->hMonitor, (MONITORINFO*)&monitorInfo);

        *grDisplayProps = (GR_WSI_WIN_DISPLAY_PROPERTIES) {
            .hMonitor = grWsiWinDisplay->hMonitor,
            .displayName = "", // Initialized below
            .desktopCoordinates = {
                .offset = {
                    monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.top,
                },
                .extent = {
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                },
            },
            .rotation = GR_WSI_WIN_ROTATION_ANGLE_0, // FIXME
        };
        strncpy(grDisplayProps->displayName, monitorInfo.szDevice, GR_MAX_DEVICE_NAME_LEN);
    }   break;
    default:
        LOGW("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    assert(expectedSize != 0);
    *pDataSize = expectedSize;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grBindObjectMemory(
    GR_OBJECT object,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %llu\n", object, mem, offset);
    GrObject* grObject = (GrObject*)object;
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;
    VkResult vkRes = VK_SUCCESS;

    if (grObject == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }

    if (grGpuMemory != NULL) {
        // Bind memory
        GrObjectType objType = GET_OBJ_TYPE(grObject);

        switch (objType) {
        case GR_OBJ_TYPE_IMAGE: {
            GrImage* grImage = (GrImage*)grObject;
            GrDevice* grDevice = GET_OBJ_DEVICE(grObject);

            if (!grDevice->virtualizeMemory) {
                if (grImage->image != VK_NULL_HANDLE) {
                    vkRes = VKD.vkBindImageMemory(grDevice->device, grImage->image,
                                                grGpuMemory->deviceMemory, offset);
                } else {
                    vkRes = VKD.vkBindBufferMemory(grDevice->device, grImage->buffer,
                                                grGpuMemory->deviceMemory, offset);
                }
            } else {
                const GrvkMemoryHeap *heap = &grDevice->heaps.heaps[grGpuMemory->heapIndex];
                bool hostVisible = (heap->vkPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
                bool deviceLocal = (heap->vkPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

                VmaAllocationCreateInfo allocationInfo = {
                    .flags = 0,
                    .usage = 0,
                    .requiredFlags = heap->vkPropertyFlags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    .preferredFlags = deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0,
                    .memoryTypeBits = 0,
                    .pool = VK_NULL_HANDLE,
                    .pUserData = NULL,
                    .priority = 1.0f
                };
                if (!hostVisible) {
                    if (grImage->image != VK_NULL_HANDLE)
                    {
                        vkRes = vmaAllocateMemoryForImage(grDevice->allocator, grImage->image,
                                                        &allocationInfo, &grImage->imageAllocation, NULL);
                        if (vkRes == VK_SUCCESS) {
                            vkRes = vmaBindImageMemory(grDevice->allocator, grImage->imageAllocation,
                                                            grImage->image);
                        }
                        if (grImage->buffer != VK_NULL_HANDLE) {
                            // The image is not host visible, so we don't need the buffer
                            VKD.vkDestroyBuffer(grDevice->device, grImage->buffer, NULL);
                            grImage->buffer = VK_NULL_HANDLE;
                        }
                    } else {
                        vkRes = vmaAllocateMemoryForBuffer(grDevice->allocator, grImage->buffer,
                                                        &allocationInfo, &grImage->bufferAllocation, NULL);
                        if (vkRes == VK_SUCCESS) {
                            vkRes = vmaBindBufferMemory(grDevice->allocator, grImage->bufferAllocation,
                                                            grImage->buffer);
                        }
                    }
                } else {
                    if (grImage->buffer != VK_NULL_HANDLE)
                    {
                        vkRes = vmaBindBufferMemory2(grDevice->allocator, grGpuMemory->allocation,
                                                        offset, grImage->buffer, NULL);

                        if (vkRes == VK_SUCCESS && grImage->image != VK_NULL_HANDLE)
                        {
                            allocationInfo.requiredFlags &= ~(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

                            vkRes = vmaAllocateMemoryForImage(grDevice->allocator, grImage->image,
                                                            &allocationInfo, &grImage->imageAllocation, NULL);

                            if (vkRes == VK_SUCCESS) {
                                vkRes = vmaBindImageMemory(grDevice->allocator, grImage->imageAllocation,
                                                               grImage->image);
                            }
                        }
                    } else {
                        vkRes = vmaBindImageMemory2(grDevice->allocator, grGpuMemory->allocation,
                                                        offset, grImage->image, NULL);
                    }
                }
            }
        }   break;
        case GR_OBJ_TYPE_BORDER_COLOR_PALETTE:
        case GR_OBJ_TYPE_COLOR_TARGET_VIEW:
        case GR_OBJ_TYPE_DEPTH_STENCIL_VIEW:
        case GR_OBJ_TYPE_DESCRIPTOR_SET:
        case GR_OBJ_TYPE_EVENT:
        case GR_OBJ_TYPE_FENCE:
        case GR_OBJ_TYPE_IMAGE_VIEW:
        case GR_OBJ_TYPE_PIPELINE:
        case GR_OBJ_TYPE_QUERY_POOL:
        case GR_OBJ_TYPE_QUEUE_SEMAPHORE:
            // Nothing to do
            break;
        default:
            LOGW("unsupported object type %d\n", objType);
            return GR_ERROR_UNAVAILABLE;
        }

        if (vkRes != VK_SUCCESS) {
            LOGW("binding failed (%d)\n", objType);
        }
    }

    grObject->grGpuMemory = grGpuMemory;

    return getGrResult(vkRes);
}
