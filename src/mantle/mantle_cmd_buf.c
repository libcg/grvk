#include "mantle_internal.h"
#include "amdilc.h"

static VkFramebuffer getVkFramebuffer(
    const GrDevice* grDevice,
    VkRenderPass renderPass,
    unsigned attachmentCount,
    const VkImageView* attachments,
    VkExtent3D extent)
{
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .width = extent.width,
        .height = extent.height,
        .layers = extent.depth,
    };

    if (VKD.vkCreateFramebuffer(grDevice->device, &framebufferCreateInfo, NULL,
                                &framebuffer) != VK_SUCCESS) {
        LOGE("vkCreateFramebuffer failed\n");
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}

static const DescriptorSetSlot* getDescriptorSetSlot(
    const GrDescriptorSet* grDescriptorSet,
    unsigned slotOffset,
    const GR_DESCRIPTOR_SET_MAPPING* mapping,
    uint32_t bindingIndex)
{
    for (unsigned i = 0; i < mapping->descriptorCount; i++) {
        const GR_DESCRIPTOR_SLOT_INFO* slotInfo = &mapping->pDescriptorInfo[i];
        const DescriptorSetSlot* slot = &grDescriptorSet->slots[slotOffset + i];

        if (slotInfo->slotObjectType == GR_SLOT_UNUSED) {
            continue;
        } else if (slotInfo->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
            if (slot->type != SLOT_TYPE_NESTED) {
                LOGE("unexpected slot type %d (should be nested)\n", slot->type);
                assert(false);
            }

            return getDescriptorSetSlot(slot->nested.nextSet, slot->nested.slotOffset,
                                        slotInfo->pNextLevelSet, bindingIndex);
        }

        uint32_t slotBinding = slotInfo->shaderEntityIndex;
        if (slotInfo->slotObjectType != GR_SLOT_SHADER_SAMPLER) {
            slotBinding += ILC_BASE_RESOURCE_ID;
        }

        if (slotBinding == bindingIndex) {
            return slot;
        }
    }

    return NULL;
}

static void updateVkDescriptorSet(
    const GrDevice* grDevice,
    VkDescriptorSet vkDescriptorSet,
    unsigned slotOffset,
    const GR_PIPELINE_SHADER* shaderInfo,
    const GrDescriptorSet* grDescriptorSet,
    VkBufferView dynamicBufferView)
{
    const GrShader* grShader = (GrShader*)shaderInfo->shader;
    const GR_DYNAMIC_MEMORY_VIEW_SLOT_INFO* dynamicMapping = &shaderInfo->dynamicMemoryViewMapping;
    const DescriptorSetSlot dynamicMemoryViewSlot = {
        .type = SLOT_TYPE_MEMORY_VIEW,
        .memoryView.vkBufferView = dynamicBufferView,
    };

    if (grShader == NULL) {
        // Nothing to update
        return;
    }

    for (unsigned i = 0; i < grShader->bindingCount; i++) {
        const IlcBinding* binding = &grShader->bindings[i];
        const DescriptorSetSlot* slot;

        if (dynamicMapping->slotObjectType != GR_SLOT_UNUSED &&
            (binding->index == (ILC_BASE_RESOURCE_ID + dynamicMapping->shaderEntityIndex))) {
            slot = &dynamicMemoryViewSlot;
        } else {
            slot = getDescriptorSetSlot(grDescriptorSet, slotOffset,
                                        &shaderInfo->descriptorSetMapping[0], binding->index);
        }

        if (slot == NULL) {
            LOGE("can't find slot for binding %d\n", binding->index);
            assert(false);
        }

        VkDescriptorImageInfo imageInfo;
        VkWriteDescriptorSet writeDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = vkDescriptorSet,
            .dstBinding = binding->index,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = binding->descriptorType,
            .pImageInfo = NULL, // Set below
            .pBufferInfo = NULL, // Set below
            .pTexelBufferView = NULL, // Set below
        };

        if (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) {
            if (slot->type != SLOT_TYPE_SAMPLER) {
                LOGE("unexpected slot type %d for descriptor type %d\n",
                     slot->type, binding->descriptorType);
                assert(false);
            }

            imageInfo = (VkDescriptorImageInfo) {
                .sampler = slot->sampler.vkSampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            writeDescriptorSet.pImageInfo = &imageInfo;
        } else if (binding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
            if (slot->type != SLOT_TYPE_MEMORY_VIEW) {
                LOGE("unexpected slot type %d for descriptor type %d\n",
                     slot->type, binding->descriptorType);
                assert(false);
            }

            writeDescriptorSet.pTexelBufferView = &slot->memoryView.vkBufferView;
        } else {
            LOGE("unhandled descriptor type %d\n", binding->descriptorType);
            assert(false);
        }

        // TODO batch
        VKD.vkUpdateDescriptorSets(grDevice->device, 1, &writeDescriptorSet, 0, NULL);
    }
}

static void initCmdBufferResources(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrPipeline* grPipeline = grCmdBuffer->grPipeline;
    VkPipelineBindPoint bindPoint = getVkPipelineBindPoint(GR_PIPELINE_BIND_POINT_GRAPHICS);

    for (unsigned i = 0; i < MAX_STAGE_COUNT; i++) {
        updateVkDescriptorSet(grDevice, grPipeline->descriptorSets[i], grCmdBuffer->slotOffset,
                              &grPipeline->shaderInfos[i], grCmdBuffer->grDescriptorSet,
                              grCmdBuffer->dynamicBufferView);
    }

    VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer, bindPoint, grPipeline->pipeline);
    VKD.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, bindPoint, grPipeline->pipelineLayout,
                                0, MAX_STAGE_COUNT, grPipeline->descriptorSets, 0, NULL);

    // TODO track
    VkFramebuffer framebuffer =
        getVkFramebuffer(grDevice, grPipeline->renderPass,
                         grCmdBuffer->attachmentCount, grCmdBuffer->attachments,
                         grCmdBuffer->minExtent);

    const VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = grPipeline->renderPass,
        .framebuffer = framebuffer,
        .renderArea = (VkRect2D) {
            .offset = { 0, 0 },
            .extent = { grCmdBuffer->minExtent.width, grCmdBuffer->minExtent.height },
        },
        .clearValueCount = 0,
        .pClearValues = NULL,
    };

    if (grCmdBuffer->hasActiveRenderPass) {
        VKD.vkCmdEndRenderPass(grCmdBuffer->commandBuffer);
    }
    VKD.vkCmdBeginRenderPass(grCmdBuffer->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    grCmdBuffer->hasActiveRenderPass = true;

    grCmdBuffer->isDirty = false;
}



// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pipeline);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrPipeline* grPipeline = (GrPipeline*)pipeline;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        LOGW("unsupported bind point 0x%x\n", pipelineBindPoint);
    }

    grCmdBuffer->grPipeline = grPipeline;
    grCmdBuffer->isDirty = true;
}

GR_VOID grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, stateBindPoint, state);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrViewportStateObject* viewportState = (GrViewportStateObject*)state;
    GrRasterStateObject* rasterState = (GrRasterStateObject*)state;
    GrDepthStencilStateObject* depthStencilState = (GrDepthStencilStateObject*)state;
    GrColorBlendStateObject* colorBlendState = (GrColorBlendStateObject*)state;

    switch ((GR_STATE_BIND_POINT)stateBindPoint) {
    case GR_STATE_BIND_VIEWPORT:
        VKD.vkCmdSetViewportWithCountEXT(grCmdBuffer->commandBuffer,
                                         viewportState->viewportCount, viewportState->viewports);
        VKD.vkCmdSetScissorWithCountEXT(grCmdBuffer->commandBuffer, viewportState->scissorCount,
                                        viewportState->scissors);
        break;
    case GR_STATE_BIND_RASTER:
        VKD.vkCmdSetCullModeEXT(grCmdBuffer->commandBuffer, rasterState->cullMode);
        VKD.vkCmdSetFrontFaceEXT(grCmdBuffer->commandBuffer, rasterState->frontFace);
        VKD.vkCmdSetDepthBias(grCmdBuffer->commandBuffer, rasterState->depthBiasConstantFactor,
                              rasterState->depthBiasClamp, rasterState->depthBiasSlopeFactor);
        break;
    case GR_STATE_BIND_DEPTH_STENCIL:
        VKD.vkCmdSetDepthTestEnableEXT(grCmdBuffer->commandBuffer,
                                       depthStencilState->depthTestEnable);
        VKD.vkCmdSetDepthWriteEnableEXT(grCmdBuffer->commandBuffer,
                                        depthStencilState->depthWriteEnable);
        VKD.vkCmdSetDepthCompareOpEXT(grCmdBuffer->commandBuffer,
                                      depthStencilState->depthCompareOp);
        VKD.vkCmdSetDepthBoundsTestEnableEXT(grCmdBuffer->commandBuffer,
                                             depthStencilState->depthBoundsTestEnable);
        VKD.vkCmdSetStencilTestEnableEXT(grCmdBuffer->commandBuffer,
                                         depthStencilState->stencilTestEnable);
        VKD.vkCmdSetStencilOpEXT(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                 depthStencilState->front.failOp,
                                 depthStencilState->front.passOp,
                                 depthStencilState->front.depthFailOp,
                                 depthStencilState->front.compareOp);
        VKD.vkCmdSetStencilCompareMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                       depthStencilState->front.compareMask);
        VKD.vkCmdSetStencilWriteMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.writeMask);
        VKD.vkCmdSetStencilReference(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.reference);
        VKD.vkCmdSetStencilOpEXT(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                 depthStencilState->back.failOp,
                                 depthStencilState->back.passOp,
                                 depthStencilState->back.depthFailOp,
                                 depthStencilState->back.compareOp);
        VKD.vkCmdSetStencilCompareMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                       depthStencilState->back.compareMask);
        VKD.vkCmdSetStencilWriteMask(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.writeMask);
        VKD.vkCmdSetStencilReference(grCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.reference);
        VKD.vkCmdSetDepthBounds(grCmdBuffer->commandBuffer,
                                depthStencilState->minDepthBounds, depthStencilState->maxDepthBounds);
        break;
    case GR_STATE_BIND_COLOR_BLEND:
        VKD.vkCmdSetBlendConstants(grCmdBuffer->commandBuffer, colorBlendState->blendConstants);
        break;
    case GR_STATE_BIND_MSAA:
        // TODO
        break;
    }
}

GR_VOID grCmdBindDescriptorSet(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT index,
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT slotOffset)
{
    LOGT("%p 0x%X %u %p %u\n", cmdBuffer, pipelineBindPoint, index,  descriptorSet, slotOffset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        LOGW("unsupported bind point 0x%x\n", pipelineBindPoint);
    } else if (index != 0) {
        LOGW("unsupported index %u\n", index);
    }

    grCmdBuffer->grDescriptorSet = grDescriptorSet;
    grCmdBuffer->slotOffset = slotOffset;
    grCmdBuffer->isDirty = true;
}

GR_VOID grCmdBindDynamicMemoryView(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pMemView);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrGpuMemory* grGpuMemory = (GrGpuMemory*)pMemView->mem;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        LOGW("unsupported bind point 0x%x\n", pipelineBindPoint);
    }

    if (grCmdBuffer->dynamicBufferView != VK_NULL_HANDLE) {
        VKD.vkDestroyBufferView(grDevice->device, grCmdBuffer->dynamicBufferView, NULL);
    }

    // FIXME what is pMemView->state for?
    const VkBufferViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .buffer = grGpuMemory->buffer,
        .format = pMemView->format.channelFormat == GR_CH_FMT_UNDEFINED ?
                  VK_FORMAT_R32G32B32A32_SFLOAT : getVkFormat(pMemView->format),
        .offset = pMemView->offset,
        .range = pMemView->range,
    };

    VkResult vkRes = VKD.vkCreateBufferView(grDevice->device, &createInfo, NULL,
                                            &grCmdBuffer->dynamicBufferView);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateBufferView failed (%d)\n", vkRes);
    }
}

GR_VOID grCmdBindIndexData(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset,
    GR_ENUM indexType)
{
    LOGT("%p %p %u 0x%X\n", cmdBuffer, mem, offset, indexType);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    VKD.vkCmdBindIndexBuffer(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset,
                             getVkIndexType(indexType));
}

GR_VOID grCmdPrepareMemoryRegions(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_MEMORY_STATE_TRANSITION* pStateTransitions)
{
    LOGT("%p %u %p\n", cmdBuffer, transitionCount, pStateTransitions);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    for (int i = 0; i < transitionCount; i++) {
        const GR_MEMORY_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)stateTransition->mem;

        grGpuMemoryBindBuffer(grGpuMemory);

        const VkBufferMemoryBarrier bufferMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsMemory(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsMemory(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = grGpuMemory->buffer,
            .offset = stateTransition->offset,
            .size = stateTransition->regionSize > 0 ? stateTransition->regionSize : VK_WHOLE_SIZE,
        };

        // TODO batch
        VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 0, 0, NULL, 1, &bufferMemoryBarrier, 0, NULL);
    }
}

// FIXME what are target states for?
// TODO handle depth target
GR_VOID grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    LOGT("%p %u %p %p\n", cmdBuffer, colorTargetCount, pColorTargets, pDepthTarget);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (pDepthTarget != NULL && pDepthTarget->view != GR_NULL_HANDLE) {
        LOGW("unhandled depth target\n");
    }

    // Find minimum extent
    grCmdBuffer->minExtent = (VkExtent3D) { UINT32_MAX, UINT32_MAX, UINT32_MAX };
    for (unsigned i = 0; i < colorTargetCount; i++) {
        const GrColorTargetView* grColorTargetView = (GrColorTargetView*)pColorTargets[i].view;

        if (grColorTargetView != NULL) {
            grCmdBuffer->minExtent.width = MIN(grCmdBuffer->minExtent.width,
                                               grColorTargetView->extent.width);
            grCmdBuffer->minExtent.height = MIN(grCmdBuffer->minExtent.height,
                                                grColorTargetView->extent.height);
            grCmdBuffer->minExtent.depth = MIN(grCmdBuffer->minExtent.depth,
                                               grColorTargetView->extent.depth);
        }
    }

    // Copy attachments
    grCmdBuffer->attachmentCount = 0;

    for (unsigned i = 0; i < colorTargetCount; i++) {
        const GrColorTargetView* grColorTargetView = (GrColorTargetView*)pColorTargets[i].view;

        if (grColorTargetView != NULL) {
            grCmdBuffer->attachments[grCmdBuffer->attachmentCount] = grColorTargetView->imageView;
            grCmdBuffer->attachmentCount++;
        }
    }
}

GR_VOID grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    LOGT("%p %u %p\n", cmdBuffer, transitionCount, pStateTransitions);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    for (int i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrImage* grImage = (GrImage*)stateTransition->image;

        const VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsImage(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsImage(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = grImage->image,
            .subresourceRange = getVkImageSubresourceRange(stateTransition->subresourceRange),
        };

        // TODO batch
        VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}

GR_VOID grCmdDraw(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstVertex,
    GR_UINT vertexCount,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    LOGT("%p %u %u %u %u\n", cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->isDirty) {
        initCmdBufferResources(grCmdBuffer);
    }

    VKD.vkCmdDraw(grCmdBuffer->commandBuffer,
                  vertexCount, instanceCount, firstVertex, firstInstance);
}

GR_VOID grCmdDrawIndexed(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstIndex,
    GR_UINT indexCount,
    GR_INT vertexOffset,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    LOGT("%p %u %u %d %u %u\n",
         cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->isDirty) {
        initCmdBufferResources(grCmdBuffer);
    }

    VKD.vkCmdDrawIndexed(grCmdBuffer->commandBuffer,
                         indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

GR_VOID grCmdCopyMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcMem, destMem, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grSrcGpuMemory = (GrGpuMemory*)srcMem;
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grGpuMemoryBindBuffer(grSrcGpuMemory);
    grGpuMemoryBindBuffer(grDstGpuMemory);

    VkBufferCopy* vkRegions = malloc(regionCount * sizeof(VkBufferCopy));
    for (unsigned i = 0; i < regionCount; i++) {
        const GR_MEMORY_COPY* region = &pRegions[i];

        vkRegions[i] = (VkBufferCopy) {
            .srcOffset = region->srcOffset,
            .dstOffset = region->destOffset,
            .size = region->copySize,
        };
    }

    VKD.vkCmdCopyBuffer(grCmdBuffer->commandBuffer, grSrcGpuMemory->buffer, grDstGpuMemory->buffer,
                        regionCount, vkRegions);

    free(vkRegions);
}

GR_VOID grCmdCopyImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcImage, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grSrcImage = (GrImage*)srcImage;
    GrImage* grDstImage = (GrImage*)destImage;

    VkImageCopy* vkRegions = malloc(regionCount * sizeof(VkImageCopy));
    for (unsigned i = 0; i < regionCount; i++) {
        const GR_IMAGE_COPY* region = &pRegions[i];

        vkRegions[i] = (VkImageCopy) {
            .srcSubresource = getVkImageSubresourceLayers(region->srcSubresource),
            .srcOffset = { region->srcOffset.x, region->srcOffset.y, region->srcOffset.z },
            .dstSubresource = getVkImageSubresourceLayers(region->destSubresource),
            .dstOffset = { region->destOffset.x, region->destOffset.y, region->destOffset.z },
            .extent = { region->extent.width, region->extent.height, region->extent.depth },
        };
    }

    VKD.vkCmdCopyImage(grCmdBuffer->commandBuffer,
                       grSrcImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                       grDstImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                       regionCount, vkRegions);

    free(vkRegions);
}

GR_VOID grCmdCopyMemoryToImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcMem, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grSrcGpuMemory = (GrGpuMemory*)srcMem;
    GrImage* grDstImage = (GrImage*)destImage;

    grGpuMemoryBindBuffer(grSrcGpuMemory);

    VkBufferImageCopy* vkRegions = malloc(regionCount * sizeof(VkBufferImageCopy));
    for (unsigned i = 0; i < regionCount; i++) {
        const GR_MEMORY_IMAGE_COPY* region = &pRegions[i];

        vkRegions[i] = (VkBufferImageCopy) {
            .bufferOffset = region->memOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = getVkImageSubresourceLayers(region->imageSubresource),
            .imageOffset = { region->imageOffset.x, region->imageOffset.y, region->imageOffset.z },
            .imageExtent = {
                region->imageExtent.width,
                region->imageExtent.height,
                region->imageExtent.depth,
            },
        };
    }

    VKD.vkCmdCopyBufferToImage(grCmdBuffer->commandBuffer, grSrcGpuMemory->buffer,
                               grDstImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                               regionCount, vkRegions);

    free(vkRegions);
}

GR_VOID grCmdClearColorImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGT("%p %p %g %g %g %g %u %p\n",
         cmdBuffer, image, color[0], color[1], color[2], color[3], rangeCount, pRanges);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    const VkClearColorValue vkColor = {
        .float32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i]);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}

GR_VOID grCmdClearColorImageRaw(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_UINT32 color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGT("%p %p %u %u %u %u %u %p\n",
         cmdBuffer, image, color[0], color[1], color[2], color[3], rangeCount, pRanges);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i]);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}

GR_VOID grCmdSetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    LOGT("%p %p\n", cmdBuffer, event);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrEvent* grEvent = (GrEvent*)event;

    VKD.vkCmdSetEvent(grCmdBuffer->commandBuffer, grEvent->event,
                      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

GR_VOID grCmdResetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    LOGT("%p %p\n", cmdBuffer, event);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrEvent* grEvent = (GrEvent*)event;

    VKD.vkCmdResetEvent(grCmdBuffer->commandBuffer, grEvent->event,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}
