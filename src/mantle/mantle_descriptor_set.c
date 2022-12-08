#include "mantle_internal.h"

inline static void releaseSlot(
    const GrDevice* grDevice,
    DescriptorSetSlot* slot)
{
    if (slot->type == SLOT_TYPE_BUFFER && slot->buffer.bufferView != VK_NULL_HANDLE) {
        VKD.vkDestroyBufferView(grDevice->device, slot->buffer.bufferView, NULL);
    }
}

// Descriptor Set Functions

GR_RESULT GR_STDCALL grCreateDescriptorSet(
    GR_DEVICE device,
    const GR_DESCRIPTOR_SET_CREATE_INFO* pCreateInfo,
    GR_DESCRIPTOR_SET* pDescriptorSet)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pDescriptorSet);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pDescriptorSet == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    VkBuffer vkBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = 0;
    VkDeviceAddress bufferAddress = 0ull;
    void* descriptorBufferPtr = NULL;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkResult vkRes = VK_SUCCESS;
    if (grDevice->descriptorBufferSupported) {
        bufferSize = grDevice->maxMutableDescriptorSize * pCreateInfo->slots * DESCRIPTORS_PER_SLOT;
        uint32_t queueFamilyIndices[2];
        uint32_t queueFamilyIndexCount = 0;
        if (grDevice->grUniversalQueue) {
            queueFamilyIndexCount++;
            queueFamilyIndices[0] = grDevice->grUniversalQueue->queueFamilyIndex;
        }
        if (grDevice->grComputeQueue) {
            queueFamilyIndexCount++;
            queueFamilyIndices[queueFamilyIndexCount - 1] = grDevice->grComputeQueue->queueFamilyIndex;
        }
        const VkBufferCreateInfo bufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .size = bufferSize,
            .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = queueFamilyIndexCount <= 1 ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = queueFamilyIndexCount <= 1 ? 0 : queueFamilyIndexCount,
            .pQueueFamilyIndices = queueFamilyIndexCount <= 1 ? NULL : queueFamilyIndices,
        };

        vkRes = VKD.vkCreateBuffer(grDevice->device, &bufferCreateInfo, NULL, &vkBuffer);
        if (vkRes != VK_SUCCESS) {
            LOGE("vkCreateBuffer failed (%d)\n", vkRes);
            goto bail;
        }
        if (quirkHas(QUIRK_DESCRIPTOR_SET_USE_DEDICATED_ALLOCATION)) {
            VkMemoryRequirements memReqs;
            VKD.vkGetBufferMemoryRequirements(grDevice->device, vkBuffer, &memReqs);

            const VkMemoryAllocateFlagsInfo flagsInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                .pNext = NULL,
                .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                .deviceMask = 0,
            };
            VkMemoryAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = &flagsInfo,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = 0xFFFF,
            };
            // exclude host non-visible memory types
            for (unsigned i = 0; i < grDevice->memoryProperties.memoryTypeCount; ++i) {
                if (!(memReqs.memoryTypeBits & (1 << i))) {
                    continue;
                }
                if (!(grDevice->memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                    continue;
                }

                allocateInfo.memoryTypeIndex = i;
                vkRes = VKD.vkAllocateMemory(grDevice->device, &allocateInfo, NULL, &bufferMemory);
                if (vkRes == VK_SUCCESS) {
                    break;
                }
            }

            if (vkRes != VK_SUCCESS) {
                LOGE("failed to allocate memory for descriptor buffer (%d)\n", vkRes);
                goto bail;
            } else if (bufferMemory == VK_NULL_HANDLE) {
                LOGE("failed to select memory memory for descriptor buffer\n");
                goto bail;
            }

            vkRes = VKD.vkBindBufferMemory(grDevice->device, vkBuffer, bufferMemory, 0);
            if (vkRes != VK_SUCCESS) {
                LOGE("Buffer binding failed (%d)\n", vkRes);
                goto bail;
            }
            VkBufferDeviceAddressInfo vkBufferAddressInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = NULL,
                .buffer = vkBuffer,
            };

            bufferAddress = VKD.vkGetBufferDeviceAddress(grDevice->device, &vkBufferAddressInfo);

            vkRes = VKD.vkMapMemory(grDevice->device, bufferMemory,
                                    0, VK_WHOLE_SIZE, 0, &descriptorBufferPtr);
            if (vkRes != VK_SUCCESS) {
                LOGE("vkMapMemory failed (%d)\n", vkRes);
                goto bail;
            }

            memset(descriptorBufferPtr, 0, bufferSize);
        }
    } else {
        const VkDescriptorType descriptorTypes[] = {
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_SAMPLER
        };
        const VkMutableDescriptorTypeListEXT mutableTypeList = {
            .descriptorTypeCount = COUNT_OF(descriptorTypes),
            .pDescriptorTypes = descriptorTypes,
        };
        const VkMutableDescriptorTypeCreateInfoEXT mutableTypeInfo = {
            .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,
            .pNext = NULL,
            .mutableDescriptorTypeListCount = 1,
            .pMutableDescriptorTypeLists = &mutableTypeList,
        };
        const VkDescriptorPoolSize poolSize = {
            .type = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
            .descriptorCount = DESCRIPTORS_PER_SLOT * pCreateInfo->slots,
        };

        const VkDescriptorPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = &mutableTypeInfo,
            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };

        vkRes = VKD.vkCreateDescriptorPool(grDevice->device, &poolCreateInfo, NULL, &descriptorPool);
        if (vkRes != VK_SUCCESS) {
            LOGE("vkCreateDescriptorPool failed (%d)\n", vkRes);
            goto bail;
        }

        const VkDescriptorSetVariableDescriptorCountAllocateInfo descriptorCountInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext = NULL,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &poolSize.descriptorCount,
        };
        const VkDescriptorSetAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &descriptorCountInfo,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &grDevice->defaultDescriptorSetLayout,
        };

        vkRes = VKD.vkAllocateDescriptorSets(grDevice->device, &allocateInfo, &descriptorSet);
        if (vkRes != VK_SUCCESS) {
            LOGE("vkAllocateDescriptorSets failed (%d)\n", vkRes);
            goto bail;
        }
    }

    GrDescriptorSet* grDescriptorSet = malloc(sizeof(GrDescriptorSet));
    if (grDescriptorSet == NULL) {
        return GR_ERROR_OUT_OF_MEMORY;
    }

    *grDescriptorSet = (GrDescriptorSet) {
        .grObj = { GR_OBJ_TYPE_DESCRIPTOR_SET, grDevice },
        .slotCount = pCreateInfo->slots,
        .slots = calloc(pCreateInfo->slots, sizeof(DescriptorSetSlot)),
        .descriptorLock = SRWLOCK_INIT,
        .descriptorPool = descriptorPool,
        .descriptorSet = descriptorSet,
        .descriptorBufferPtr = descriptorBufferPtr,
        .descriptorBuffer = vkBuffer,
        .descriptorBufferMemory = bufferMemory,
        .descriptorBufferSize = bufferSize,
        .descriptorBufferMemoryOffset = 0ull,
        .descriptorBufferAddress = bufferAddress,
    };

    *pDescriptorSet = (GR_DESCRIPTOR_SET)grDescriptorSet;
    return GR_SUCCESS;

bail:
    VKD.vkDestroyDescriptorPool(grDevice->device, descriptorPool, NULL);
    VKD.vkDestroyBuffer(grDevice->device, vkBuffer, NULL);
    VKD.vkFreeMemory(grDevice->device, bufferMemory, NULL);
    return getGrResult(vkRes);
}

GR_VOID GR_STDCALL grBeginDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    GrDevice* grDevice = GET_OBJ_DEVICE(descriptorSet);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    if (grDevice->descriptorBufferSupported && grDescriptorSet->descriptorBufferPtr == NULL) {
        LOGE("memory is not mapped for descriptor buffer");
    }
}

GR_VOID GR_STDCALL grEndDescriptorSetUpdate(
    GR_DESCRIPTOR_SET descriptorSet)
{
    LOGT("%p\n", descriptorSet);

    // No-op
}

GR_VOID GR_STDCALL grAttachSamplerDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_SAMPLER* pSamplers)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pSamplers);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        AcquireSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }

    if (grDevice->descriptorBufferSupported && grDevice->descriptorBufferAllowPreparedSampler) {
        for (unsigned i = 0; i < slotCount; i++) {
            const GrSampler* grSampler = (GrSampler*)pSamplers[i];
            memcpy(grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLER)) * grDevice->maxMutableDescriptorSize,
                   &grSampler->descriptor,
                   grDevice->descriptorBufferProps.samplerDescriptorSize);
        }
    } else if (grDevice->descriptorBufferSupported) {
        for (unsigned i = 0; i < slotCount; i++) {
            const GrSampler* grSampler = (GrSampler*)pSamplers[i];

            VkDescriptorGetInfoEXT descriptorInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .pNext = NULL,
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .data = {
                    .pSampler = &grSampler->sampler
                }
            };

            VKD.vkGetDescriptorEXT(
                grDevice->device,
                &descriptorInfo,
                grDevice->descriptorBufferProps.samplerDescriptorSize,
                grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLER)) * grDevice->maxMutableDescriptorSize);
        }
    } else {
        STACK_ARRAY(VkWriteDescriptorSet, writeDescriptors, 128, slotCount);
        unsigned descriptorWriteCount = 0;

        for (unsigned i = 0; i < slotCount; i++) {
            DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
            const GrSampler* grSampler = (GrSampler*)pSamplers[i];

            releaseSlot(grDevice, slot);

            *slot = (DescriptorSetSlot) {
                .type = SLOT_TYPE_IMAGE,
                .image = {
                    .imageInfo = {
                        .sampler = grSampler->sampler,
                        .imageView = VK_NULL_HANDLE,
                        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    },
                },
            };

            writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = grDescriptorSet->descriptorSet,
                .dstBinding = 0,
                .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLER),
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .pImageInfo = &slot->image.imageInfo,
                .pBufferInfo = NULL,
                .pTexelBufferView = NULL,
            };
        }

        VKD.vkUpdateDescriptorSets(grDevice->device, descriptorWriteCount, writeDescriptors, 0, NULL);
        STACK_ARRAY_FINISH(writeDescriptors);
    }

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        ReleaseSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }
}

GR_VOID GR_STDCALL grAttachImageViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_IMAGE_VIEW_ATTACH_INFO* pImageViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pImageViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        AcquireSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }

    if (grDevice->descriptorBufferSupported && grDevice->descriptorBufferAllowPreparedImageView) {
        for (unsigned i = 0; i < slotCount; i++) {
            const GR_IMAGE_VIEW_ATTACH_INFO* info = &pImageViews[i];
            const GrImageView* grImageView = (GrImageView*)info->view;
            memcpy(grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) * grDevice->maxMutableDescriptorSize,
                   &grImageView->sampledDescriptor,
                   grDevice->descriptorBufferProps.sampledImageDescriptorSize
                );

            if (grImageView->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
                memcpy(
                    grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)) * grDevice->maxMutableDescriptorSize,
                    &grImageView->storageDescriptor,
                    grDevice->descriptorBufferProps.storageImageDescriptorSize);
            }
        }
    } else if (grDevice->descriptorBufferSupported) {
        for (unsigned i = 0; i < slotCount; i++) {
            const GR_IMAGE_VIEW_ATTACH_INFO* info = &pImageViews[i];
            const GrImageView* grImageView = (GrImageView*)info->view;

            VkDescriptorImageInfo imageInfo = {
                .sampler = VK_NULL_HANDLE,
                .imageView = grImageView->imageView,
                .imageLayout = getVkImageLayout(info->state),
            };

            VkDescriptorGetInfoEXT descriptorInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .pNext = NULL,
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .data = {
                    .pSampledImage = &imageInfo,
                }
            };
            VKD.vkGetDescriptorEXT(
                grDevice->device,
                &descriptorInfo,
                grDevice->descriptorBufferProps.sampledImageDescriptorSize,
                grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)) * grDevice->maxMutableDescriptorSize);

            if (grImageView->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
                descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptorInfo.data.pStorageImage = &imageInfo;

                VKD.vkGetDescriptorEXT(
                    grDevice->device,
                    &descriptorInfo,
                    grDevice->descriptorBufferProps.storageImageDescriptorSize,
                    grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)) * grDevice->maxMutableDescriptorSize);
            }
        }
    } else {
        STACK_ARRAY(VkWriteDescriptorSet, writeDescriptors, 128, slotCount * 2);
        unsigned descriptorWriteCount = 0;

        for (unsigned i = 0; i < slotCount; i++) {
            DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
            const GR_IMAGE_VIEW_ATTACH_INFO* info = &pImageViews[i];
            const GrImageView* grImageView = (GrImageView*)info->view;

            releaseSlot(grDevice, slot);

            *slot = (DescriptorSetSlot) {
                .type = SLOT_TYPE_IMAGE,
                    .image = {
                        .imageInfo = {
                            .sampler = VK_NULL_HANDLE,
                            .imageView = grImageView->imageView,
                            .imageLayout = getVkImageLayout(info->state),
                        },
                    },
            };

            if (grImageView->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
                writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = NULL,
                    .dstSet = grDescriptorSet->descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .pImageInfo = &slot->image.imageInfo,
                    .pBufferInfo = NULL,
                    .pTexelBufferView = NULL,
                };
            }
            writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = grDescriptorSet->descriptorSet,
                .dstBinding = 0,
                .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo = &slot->image.imageInfo,
                .pBufferInfo = NULL,
                .pTexelBufferView = NULL,
            };
        }

        VKD.vkUpdateDescriptorSets(grDevice->device, descriptorWriteCount, writeDescriptors, 0, NULL);
        STACK_ARRAY_FINISH(writeDescriptors);
    }

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        ReleaseSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }
}

GR_VOID GR_STDCALL grAttachMemoryViewDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemViews)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pMemViews);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);
    VkResult vkRes;

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        AcquireSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }

    if (grDevice->descriptorBufferSupported) {
        for (unsigned i = 0; i < slotCount; i++) {
            DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
            const GR_MEMORY_VIEW_ATTACH_INFO* info = &pMemViews[i];
            GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;
            VkFormat vkFormat = getVkFormat(info->format);
            VkBufferView vkBufferView = VK_NULL_HANDLE;

            VkDescriptorAddressInfoEXT bufferInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                .pNext = NULL,
                .address = grGpuMemory->address + info->offset,
                .range = info->range,
                .format = vkFormat
            };

            if (vkFormat != VK_FORMAT_UNDEFINED) {
                VkDescriptorGetInfoEXT descriptorInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                    .pNext = NULL,
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                    .data = {
                        .pUniformTexelBuffer = &bufferInfo
                    }
                };

                VKD.vkGetDescriptorEXT(
                    grDevice->device,
                    &descriptorInfo,
                    grDevice->descriptorBufferProps.uniformTexelBufferDescriptorSize,
                    grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)) * grDevice->maxMutableDescriptorSize);

                descriptorInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                descriptorInfo.data.pStorageTexelBuffer = &bufferInfo;

                VKD.vkGetDescriptorEXT(
                    grDevice->device,
                    &descriptorInfo,
                    grDevice->descriptorBufferProps.storageTexelBufferDescriptorSize,
                    grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)) * grDevice->maxMutableDescriptorSize);

            }

            VkDescriptorGetInfoEXT descriptorInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .pNext = NULL,
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .data = {
                    .pStorageBuffer = &bufferInfo
                }
            };

            VKD.vkGetDescriptorEXT(
                grDevice->device,
                &descriptorInfo,
                grDevice->descriptorBufferProps.storageBufferDescriptorSize,
                grDescriptorSet->descriptorBufferPtr + ((startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) * grDevice->maxMutableDescriptorSize);

            *slot = (DescriptorSetSlot) {
                .type = SLOT_TYPE_BUFFER,
                .buffer = {
                    .bufferView = vkBufferView,
                    .bufferInfo = {
                        .buffer = grGpuMemory->buffer,
                        .offset = info->offset,
                        .range = info->range,
                    },
                    .stride = info->stride,
                },
            };
        }
    } else {
        STACK_ARRAY(VkWriteDescriptorSet, writeDescriptors, 128, slotCount * 3);
        unsigned descriptorWriteCount = 0;

        for (unsigned i = 0; i < slotCount; i++) {
            DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
            const GR_MEMORY_VIEW_ATTACH_INFO* info = &pMemViews[i];
            GrGpuMemory* grGpuMemory = (GrGpuMemory*)info->mem;
            VkFormat vkFormat = getVkFormat(info->format);
            VkBufferView vkBufferView = VK_NULL_HANDLE;

            releaseSlot(grDevice, slot);

            if (vkFormat != VK_FORMAT_UNDEFINED) {
                // Create buffer view for typed buffers
                const VkBufferViewCreateInfo createInfo = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0,
                    .buffer = grGpuMemory->buffer,
                    .format = vkFormat,
                    .offset = info->offset,
                    .range = info->range,
                };

                vkRes = VKD.vkCreateBufferView(grDevice->device, &createInfo, NULL, &vkBufferView);
                if (vkRes != VK_SUCCESS) {
                    LOGE("vkCreateBufferView failed (%d)\n", vkRes);
                }

                writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = NULL,
                    .dstSet = grDescriptorSet->descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER),
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                    .pImageInfo = NULL,
                    .pBufferInfo = NULL,
                    .pTexelBufferView = &slot->buffer.bufferView,
                };
                writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = NULL,
                    .dstSet = grDescriptorSet->descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER),
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                    .pImageInfo = NULL,
                    .pBufferInfo = NULL,
                    .pTexelBufferView = &slot->buffer.bufferView,
                };
            }
            writeDescriptors[descriptorWriteCount++] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = grDescriptorSet->descriptorSet,
                .dstBinding = 0,
                .dstArrayElement = (startSlot + i) * DESCRIPTORS_PER_SLOT + getDescriptorOffset(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pImageInfo = NULL,
                .pBufferInfo = &slot->buffer.bufferInfo,
                .pTexelBufferView = NULL,
            };
            *slot = (DescriptorSetSlot) {
                .type = SLOT_TYPE_BUFFER,
                .buffer = {
                    .bufferView = vkBufferView,
                    .bufferInfo = {
                        .buffer = grGpuMemory->buffer,
                        .offset = info->offset,
                        .range = info->range,
                    },
                    .stride = info->stride,
                },
            };
        }

        VKD.vkUpdateDescriptorSets(grDevice->device, descriptorWriteCount, writeDescriptors, 0, NULL);
        STACK_ARRAY_FINISH(writeDescriptors);
    }

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        ReleaseSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }
}

GR_VOID GR_STDCALL grAttachNestedDescriptors(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount,
    const GR_DESCRIPTOR_SET_ATTACH_INFO* pNestedDescriptorSets)
{
    LOGT("%p %u %u %p\n", descriptorSet, startSlot, slotCount, pNestedDescriptorSets);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        AcquireSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }

    for (unsigned i = 0; i < slotCount; i++) {
        DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];
        const GR_DESCRIPTOR_SET_ATTACH_INFO* info = &pNestedDescriptorSets[i];

        releaseSlot(grDevice, slot);

        *slot = (DescriptorSetSlot) {
            .type = SLOT_TYPE_NESTED,
            .nested = {
                .nextSet = (GrDescriptorSet*)info->descriptorSet,
                .slotOffset = info->slotOffset,
            },
        };
    }
    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        ReleaseSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }
}

GR_VOID GR_STDCALL grClearDescriptorSetSlots(
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT startSlot,
    GR_UINT slotCount)
{
    LOGT("%p %u %u\n", descriptorSet, startSlot, slotCount);
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grDescriptorSet);

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        AcquireSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }

    if (grDevice->descriptorBufferSupported) {
        memset(grDescriptorSet->descriptorBufferPtr + (startSlot * DESCRIPTORS_PER_SLOT * grDevice->maxMutableDescriptorSize), 0, grDevice->maxMutableDescriptorSize * slotCount * DESCRIPTORS_PER_SLOT);
        memset(&grDescriptorSet->slots[startSlot], 0, sizeof(DescriptorSetSlot) * slotCount);
    } else {
        for (unsigned i = 0; i < slotCount; i++) {
            DescriptorSetSlot* slot = &grDescriptorSet->slots[startSlot + i];

            releaseSlot(grDevice, slot);

            slot->type = SLOT_TYPE_NONE;
        }
    }

    if (quirkHas(QUIRK_DESCRIPTOR_SET_INTERNAL_SYNCHRONIZED)) {
        ReleaseSRWLockExclusive(&grDescriptorSet->descriptorLock);
    }
}
