#include "mantle_internal.h"
#include "amdilc.h"

#define SETS_PER_POOL   (2048)

typedef enum _DirtyFlags {
    FLAG_DIRTY_DESCRIPTOR_SET       = 1u << 0,
    FLAG_DIRTY_RENDER_PASS          = 1u << 1,
    FLAG_DIRTY_PIPELINE             = 1u << 2,
    FLAG_DIRTY_DYNAMIC_OFFSET       = 1u << 3,
} DirtyFlags;

static VkDescriptorPool getVkDescriptorPool(
    const GrDevice* grDevice)
{
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // TODO rebalance
    const VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                   SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            SETS_PER_POOL },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    SETS_PER_POOL },
    };

    const VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = SETS_PER_POOL,
        .poolSizeCount = COUNT_OF(poolSizes),
        .pPoolSizes = poolSizes,
    };

    VkResult res = VKD.vkCreateDescriptorPool(grDevice->device, &createInfo, NULL, &descriptorPool);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool failed (%d)\n", res);
        assert(false);
    }

    return descriptorPool;
}

static void updateVkDescriptorSet(
    const GrDevice* grDevice,
    const GrCmdBuffer* grCmdBuffer,
    const BindPoint* bindPoint,
    const GrDescriptorSet* grDescriptorSet,
    unsigned slotOffset,
    unsigned updateTemplateSlotCount,
    const UpdateTemplateSlot* updateTemplateSlots,
    VkPipelineLayout pipelineLayout)
{
    for (unsigned i = 0; i < updateTemplateSlotCount; i++) {
        const UpdateTemplateSlot* templateSlot = &updateTemplateSlots[i];
        const DescriptorSetSlot* slot;

        if (templateSlot->isDynamic) {
            slot = &bindPoint->dynamicMemoryView;
        } else {
            slot = &grDescriptorSet->slots[slotOffset];

            for (unsigned j = 0; j < templateSlot->pathDepth; j++) {
                slot = &slot[templateSlot->path[j]];
                slot = &slot->nested.nextSet->slots[slot->nested.slotOffset];
            }
        }

        VKD.vkUpdateDescriptorSetWithTemplate(grDevice->device, bindPoint->descriptorSet,
                                              templateSlot->updateTemplate, (void*)slot);

        // Pass buffer strides down to the shader
        for (unsigned j = 0; j < templateSlot->strideCount; j++) {
            VKD.vkCmdPushConstants(grCmdBuffer->commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   templateSlot->strideOffsets[j], sizeof(uint32_t),
                                   &slot[templateSlot->strideSlotIndexes[j]].buffer.stride);
        }
    }
}

static void grCmdBufferBeginRenderPass(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->isRendering) {
        return;
    }

    const VkRenderingInfoKHR renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .renderArea = (VkRect2D) {
            .offset = { 0, 0 },
            .extent = { grCmdBuffer->minExtent.width, grCmdBuffer->minExtent.height },
        },
        .layerCount = grCmdBuffer->minExtent.depth,
        .viewMask = 0,
        .colorAttachmentCount = GR_MAX_COLOR_TARGETS,
        .pColorAttachments = grCmdBuffer->colorAttachments,
        .pDepthAttachment = grCmdBuffer->hasDepth ? &grCmdBuffer->depthAttachment : NULL,
        .pStencilAttachment = grCmdBuffer->hasStencil ? &grCmdBuffer->stencilAttachment : NULL,
    };

    VKD.vkCmdBeginRenderingKHR(grCmdBuffer->commandBuffer, &renderingInfo);
    grCmdBuffer->isRendering = true;
}

void grCmdBufferEndRenderPass(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (!grCmdBuffer->isRendering) {
        return;
    }

    VKD.vkCmdEndRenderingKHR(grCmdBuffer->commandBuffer);
    grCmdBuffer->isRendering = false;
}

static void grCmdBufferUpdateDescriptorSet(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint vkBindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];
    GrPipeline* grPipeline = bindPoint->grPipeline;
    VkResult vkRes;

    for (unsigned i = 0; i < 2; i++) {
        if (grCmdBuffer->descriptorPoolIndex < grCmdBuffer->descriptorPoolCount) {
            const VkDescriptorSetAllocateInfo descSetAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = NULL,
                .descriptorPool = grCmdBuffer->descriptorPools[grCmdBuffer->descriptorPoolIndex],
                .descriptorSetCount = 1,
                .pSetLayouts = &grPipeline->descriptorSetLayout,
            };

            vkRes = VKD.vkAllocateDescriptorSets(grDevice->device, &descSetAllocateInfo,
                                                 &bindPoint->descriptorSet);
            if (vkRes == VK_SUCCESS) {
                break;
            } else if (vkRes != VK_ERROR_OUT_OF_POOL_MEMORY) {
                LOGE("vkAllocateDescriptorSets failed (%d)\n", vkRes);
                break;
            } else if (i > 0) {
                LOGE("descriptor set allocation failed with a new pool\n");
                assert(false);
            } else {
                // Use the next pool
                grCmdBuffer->descriptorPoolIndex++;
            }
        }

        if (grCmdBuffer->descriptorPoolIndex == grCmdBuffer->descriptorPoolCount) {
            // Need to allocate a new pool
            VkDescriptorPool descriptorPool = getVkDescriptorPool(grDevice);

            // Track descriptor pool
            grCmdBuffer->descriptorPoolCount++;
            grCmdBuffer->descriptorPools = realloc(grCmdBuffer->descriptorPools,
                                                   grCmdBuffer->descriptorPoolCount *
                                                   sizeof(VkDescriptorPool));
            grCmdBuffer->descriptorPools[grCmdBuffer->descriptorPoolCount - 1] = descriptorPool;
        }
    }

    for (unsigned i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        updateVkDescriptorSet(grDevice, grCmdBuffer, bindPoint,
                              bindPoint->grDescriptorSets[i], bindPoint->slotOffsets[i],
                              grPipeline->updateTemplateSlotCounts[i],
                              grPipeline->updateTemplateSlots[i],
                              grPipeline->pipelineLayout);
    }
}

static void grCmdBufferBindDescriptorSet(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint vkBindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];
    const GrPipeline* grPipeline = bindPoint->grPipeline;

    const VkDescriptorSet descriptorSets[] = {
        bindPoint->descriptorSet,
        grCmdBuffer->atomicCounterSet,
    };

    uint32_t dynamicOffsets[MAX_STAGE_COUNT];

    for (unsigned i = 0; i < grPipeline->dynamicOffsetCount; i++) {
        dynamicOffsets[i] = bindPoint->dynamicOffset;
    }

    VKD.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, vkBindPoint, grPipeline->pipelineLayout,
                                0, COUNT_OF(descriptorSets), descriptorSets,
                                grPipeline->dynamicOffsetCount, dynamicOffsets);
}

static void grCmdBufferUpdateResources(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint vkBindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];
    GrPipeline* grPipeline = bindPoint->grPipeline;
    uint32_t dirtyFlags = bindPoint->dirtyFlags;

    if (dirtyFlags & FLAG_DIRTY_DESCRIPTOR_SET) {
        grCmdBufferUpdateDescriptorSet(grCmdBuffer, vkBindPoint);
    }

    if (dirtyFlags & (FLAG_DIRTY_DESCRIPTOR_SET | FLAG_DIRTY_DYNAMIC_OFFSET)) {
        grCmdBufferBindDescriptorSet(grCmdBuffer, vkBindPoint);
    }

    if (dirtyFlags & FLAG_DIRTY_RENDER_PASS) {
        grCmdBufferEndRenderPass(grCmdBuffer);
    }

    if (dirtyFlags & FLAG_DIRTY_PIPELINE) {
        VkPipeline vkPipeline = grPipelineFindOrCreateVkPipeline(grPipeline,
                                                                 grCmdBuffer->grColorBlendState,
                                                                 grCmdBuffer->grMsaaState,
                                                                 grCmdBuffer->grRasterState);

        VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer, vkBindPoint, vkPipeline);
    }

    bindPoint->dirtyFlags = 0;
}

// Command Buffer Building Functions

GR_VOID GR_STDCALL grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pipeline);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrPipeline* grPipeline = (GrPipeline*)pipeline;
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];

    if (grPipeline == bindPoint->grPipeline) {
        return;
    }

    bindPoint->grPipeline = grPipeline;

    if (vkBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SET | FLAG_DIRTY_PIPELINE;
    } else {
        // Pipeline creation isn't deferred for compute, bind now
        VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer, vkBindPoint,
                              grPipelineFindOrCreateVkPipeline(grPipeline, NULL, NULL, NULL));

        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SET;
    }
}

GR_VOID GR_STDCALL grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, stateBindPoint, state);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[VK_PIPELINE_BIND_POINT_GRAPHICS];

    // TODO compare objects instead of just pointers

    switch ((GR_STATE_BIND_POINT)stateBindPoint) {
    case GR_STATE_BIND_VIEWPORT: {
        GrViewportStateObject* viewportState = (GrViewportStateObject*)state;
        if (viewportState == grCmdBuffer->grViewportState) {
            break;
        }

        VKD.vkCmdSetViewportWithCountEXT(grCmdBuffer->commandBuffer,
                                         viewportState->viewportCount, viewportState->viewports);
        VKD.vkCmdSetScissorWithCountEXT(grCmdBuffer->commandBuffer, viewportState->scissorCount,
                                        viewportState->scissors);

        grCmdBuffer->grViewportState = viewportState;
    }   break;
    case GR_STATE_BIND_RASTER: {
        GrRasterStateObject* rasterState = (GrRasterStateObject*)state;
        if (rasterState == grCmdBuffer->grRasterState) {
            break;
        }

        VKD.vkCmdSetCullModeEXT(grCmdBuffer->commandBuffer, rasterState->cullMode);
        VKD.vkCmdSetFrontFaceEXT(grCmdBuffer->commandBuffer, rasterState->frontFace);
        VKD.vkCmdSetDepthBias(grCmdBuffer->commandBuffer, rasterState->depthBiasConstantFactor,
                              rasterState->depthBiasClamp, rasterState->depthBiasSlopeFactor);

        if (grCmdBuffer->grRasterState == NULL ||
            rasterState->polygonMode != grCmdBuffer->grRasterState->polygonMode) {
            bindPoint->dirtyFlags |= FLAG_DIRTY_PIPELINE;
        }

        grCmdBuffer->grRasterState = rasterState;
    }   break;
    case GR_STATE_BIND_DEPTH_STENCIL: {
        GrDepthStencilStateObject* depthStencilState = (GrDepthStencilStateObject*)state;
        if (depthStencilState == grCmdBuffer->grDepthStencilState) {
            break;
        }

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
        VKD.vkCmdSetDepthBounds(grCmdBuffer->commandBuffer, depthStencilState->minDepthBounds,
                                                            depthStencilState->maxDepthBounds);

        grCmdBuffer->grDepthStencilState = depthStencilState;
    }   break;
    case GR_STATE_BIND_COLOR_BLEND: {
        GrColorBlendStateObject* colorBlendState = (GrColorBlendStateObject*)state;
        if (colorBlendState == grCmdBuffer->grColorBlendState) {
            break;
        }

        VKD.vkCmdSetBlendConstants(grCmdBuffer->commandBuffer, colorBlendState->blendConstants);

        if (grCmdBuffer->grColorBlendState == NULL ||
            memcmp(colorBlendState->states, grCmdBuffer->grColorBlendState->states,
                   sizeof(colorBlendState->states)) != 0) {
            bindPoint->dirtyFlags |= FLAG_DIRTY_PIPELINE;
        }

        grCmdBuffer->grColorBlendState = colorBlendState;
    }   break;
    case GR_STATE_BIND_MSAA: {
        GrMsaaStateObject* msaaState = (GrMsaaStateObject*)state;
        if (msaaState == grCmdBuffer->grMsaaState) {
            break;
        }

        if (grCmdBuffer->grMsaaState == NULL ||
            msaaState->sampleCountFlags != grCmdBuffer->grMsaaState->sampleCountFlags ||
            msaaState->sampleMask != grCmdBuffer->grMsaaState->sampleMask) {
            bindPoint->dirtyFlags |= FLAG_DIRTY_PIPELINE;
        }

        grCmdBuffer->grMsaaState = msaaState;
    }   break;
    }
}

GR_VOID GR_STDCALL grCmdBindDescriptorSet(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT index,
    GR_DESCRIPTOR_SET descriptorSet,
    GR_UINT slotOffset)
{
    LOGT("%p 0x%X %u %p %u\n", cmdBuffer, pipelineBindPoint, index,  descriptorSet, slotOffset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDescriptorSet* grDescriptorSet = (GrDescriptorSet*)descriptorSet;
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];

    if (grDescriptorSet != bindPoint->grDescriptorSets[index] ||
        slotOffset != bindPoint->slotOffsets[index]) {
        bindPoint->grDescriptorSets[index] = grDescriptorSet;
        bindPoint->slotOffsets[index] = slotOffset;
        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SET;
    }
}

GR_VOID GR_STDCALL grCmdBindDynamicMemoryView(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pMemView);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrGpuMemory* grGpuMemory = (GrGpuMemory*)pMemView->mem;
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];

    // FIXME what is pMemView->state for?

    if (pMemView->offset != bindPoint->dynamicOffset) {
        bindPoint->dynamicOffset = pMemView->offset;

        bindPoint->dirtyFlags |= FLAG_DIRTY_DYNAMIC_OFFSET;
    }

    if (grGpuMemory->buffer != bindPoint->dynamicMemoryView.buffer.bufferInfo.buffer ||
        pMemView->range != bindPoint->dynamicMemoryView.buffer.bufferInfo.range ||
        pMemView->stride != bindPoint->dynamicMemoryView.buffer.stride) {
        bindPoint->dynamicMemoryView = (DescriptorSetSlot) {
            .type = SLOT_TYPE_BUFFER,
            .buffer = {
                .bufferView = VK_NULL_HANDLE,
                .bufferInfo = {
                    .buffer = grGpuMemory->buffer,
                    .offset = 0,
                    .range = pMemView->range,
                },
                .stride = pMemView->stride,
            },
        };

        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SET;
    }
}

GR_VOID GR_STDCALL grCmdBindIndexData(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset,
    GR_ENUM indexType)
{
    LOGT("%p %p %u 0x%X\n", cmdBuffer, mem, offset, indexType);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    VKD.vkCmdBindIndexBuffer(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset,
                             getVkIndexType(indexType));
}

GR_VOID GR_STDCALL grCmdPrepareMemoryRegions(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_MEMORY_STATE_TRANSITION* pStateTransitions)
{
    LOGT("%p %u %p\n", cmdBuffer, transitionCount, pStateTransitions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkBufferMemoryBarrier, barriers, 128, transitionCount);
    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    for (unsigned i = 0; i < transitionCount; i++) {
        const GR_MEMORY_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)stateTransition->mem;

        barriers[i] = (VkBufferMemoryBarrier) {
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

        srcStageMask |= getVkPipelineStageFlagsMemory(stateTransition->oldState);
        dstStageMask |= getVkPipelineStageFlagsMemory(stateTransition->newState);
    }

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer, srcStageMask, dstStageMask,
                             0, 0, NULL, transitionCount, barriers, 0, NULL);

    STACK_ARRAY_FINISH(barriers);
}

GR_VOID GR_STDCALL grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    LOGT("%p %u %p %p\n", cmdBuffer, colorTargetCount, pColorTargets, pDepthTarget);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[VK_PIPELINE_BIND_POINT_GRAPHICS];

    VkRenderingAttachmentInfoKHR colorAttachments[GR_MAX_COLOR_TARGETS];
    bool hasDepth = false;
    bool hasStencil = false;
    VkRenderingAttachmentInfoKHR depthAttachment;
    VkRenderingAttachmentInfoKHR stencilAttachment;
    VkExtent3D minExtent = { UINT32_MAX, UINT32_MAX, UINT32_MAX };

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GrColorTargetView* grColorTargetView =
            i < colorTargetCount ? (GrColorTargetView*)pColorTargets[i].view : NULL;

        colorAttachments[i] = (VkRenderingAttachmentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .pNext = NULL,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {{{ 0 }}},
        };

        if (grColorTargetView != NULL &&
            pColorTargets[i].colorTargetState != GR_IMAGE_STATE_UNINITIALIZED) {
            colorAttachments[i].imageView = grColorTargetView->imageView;
            colorAttachments[i].imageLayout = getVkImageLayout(pColorTargets[i].colorTargetState);

            minExtent.width = MIN(minExtent.width, grColorTargetView->extent.width);
            minExtent.height = MIN(minExtent.height, grColorTargetView->extent.height);
            minExtent.depth = MIN(minExtent.depth, grColorTargetView->extent.depth);
        }
    }

    if (pDepthTarget != NULL && pDepthTarget->view != NULL) {
        const GrDepthStencilView* grDepthStencilView = (GrDepthStencilView*)pDepthTarget->view;

        if (pDepthTarget->depthState != GR_IMAGE_STATE_UNINITIALIZED &&
            (grDepthStencilView->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0) {
            hasDepth = true;
            depthAttachment = (VkRenderingAttachmentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .pNext = NULL,
                .imageView = grDepthStencilView->imageView,
                .imageLayout = getVkImageLayout(pDepthTarget->depthState),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{ 0 }}},
            };
        }
        if (pDepthTarget->stencilState != GR_IMAGE_STATE_UNINITIALIZED &&
            (grDepthStencilView->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) {
            hasStencil = true;
            stencilAttachment = (VkRenderingAttachmentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .pNext = NULL,
                .imageView = grDepthStencilView->imageView,
                .imageLayout = getVkImageLayout(pDepthTarget->stencilState),
                .resolveMode = VK_RESOLVE_MODE_NONE,
                .resolveImageView = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{ 0 }}},
            };
        }

        if (hasDepth || hasStencil) {
            minExtent.width = MIN(minExtent.width, grDepthStencilView->extent.width);
            minExtent.height = MIN(minExtent.height, grDepthStencilView->extent.height);
            minExtent.depth = MIN(minExtent.depth, grDepthStencilView->extent.depth);
        }
    }

    if (memcmp(colorAttachments, grCmdBuffer->colorAttachments, GR_MAX_COLOR_TARGETS * sizeof(colorAttachments[0])) ||
        hasDepth != grCmdBuffer->hasDepth || hasStencil != grCmdBuffer->hasStencil ||
        (hasDepth && memcmp(&depthAttachment, &grCmdBuffer->depthAttachment, sizeof(depthAttachment))) ||
        (hasStencil && memcmp(&stencilAttachment, &grCmdBuffer->stencilAttachment, sizeof(stencilAttachment))) ||
        memcmp(&minExtent, &grCmdBuffer->minExtent, sizeof(minExtent))) {
        // Targets have changed
        memcpy(grCmdBuffer->colorAttachments, colorAttachments, GR_MAX_COLOR_TARGETS * sizeof(colorAttachments[0]));
        grCmdBuffer->hasDepth = hasDepth;
        grCmdBuffer->hasStencil = hasStencil;
        grCmdBuffer->depthAttachment = depthAttachment;
        grCmdBuffer->stencilAttachment = stencilAttachment;
        grCmdBuffer->minExtent = minExtent;

        bindPoint->dirtyFlags |= FLAG_DIRTY_RENDER_PASS;
    }
}

GR_VOID GR_STDCALL grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    LOGT("%p %u %p\n", cmdBuffer, transitionCount, pStateTransitions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkImageMemoryBarrier, barriers, 128, transitionCount);
    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    for (unsigned i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrImage* grImage = (GrImage*)stateTransition->image;

        barriers[i] = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsImage(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsImage(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = grImage->image,
            .subresourceRange = getVkImageSubresourceRange(stateTransition->subresourceRange,
                                                           grImage->multiplyCubeLayers),
        };

        srcStageMask |= getVkPipelineStageFlagsImage(stateTransition->oldState);
        dstStageMask |= getVkPipelineStageFlagsImage(stateTransition->newState);
    }

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer, srcStageMask, dstStageMask,
                             0, 0, NULL, 0, NULL, transitionCount, barriers);

    STACK_ARRAY_FINISH(barriers);
}

GR_VOID GR_STDCALL grCmdDraw(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT firstVertex,
    GR_UINT vertexCount,
    GR_UINT firstInstance,
    GR_UINT instanceCount)
{
    LOGT("%p %u %u %u %u\n", cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

#ifndef TESS
    if (grCmdBuffer->bindPoints[0].grPipeline->hasTessellation) {
        // Skip draw
        return;
    }
#endif

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDraw(grCmdBuffer->commandBuffer,
                  vertexCount, instanceCount, firstVertex, firstInstance);
}

GR_VOID GR_STDCALL grCmdDrawIndexed(
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

#ifndef TESS
    if (grCmdBuffer->bindPoints[0].grPipeline->hasTessellation) {
        // Skip draw
        return;
    }
#endif

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndexed(grCmdBuffer->commandBuffer,
                         indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

GR_VOID GR_STDCALL grCmdDrawIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %u\n", cmdBuffer, mem, offset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

#ifndef TESS
    if (grCmdBuffer->bindPoints[0].grPipeline->hasTessellation) {
        // Skip draw
        return;
    }
#endif

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndirect(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset, 1, 0);
}

GR_VOID GR_STDCALL grCmdDrawIndexedIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %u\n", cmdBuffer, mem, offset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

#ifndef TESS
    if (grCmdBuffer->bindPoints[0].grPipeline->hasTessellation) {
        // Skip draw
        return;
    }
#endif

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndexedIndirect(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset, 1, 0);
}

GR_VOID GR_STDCALL grCmdDispatch(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT x,
    GR_UINT y,
    GR_UINT z)
{
    LOGT("%p %u %u %u\n", cmdBuffer, x, y, z);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdDispatch(grCmdBuffer->commandBuffer, x, y, z);
}

GR_VOID GR_STDCALL grCmdDispatchIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %u\n", cmdBuffer, mem, offset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    grCmdBufferUpdateResources(grCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdDispatchIndirect(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset);
}

GR_VOID GR_STDCALL grCmdCopyMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcMem, destMem, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grSrcGpuMemory = (GrGpuMemory*)srcMem;
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkBufferCopy, vkRegions, 128, regionCount);

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

    STACK_ARRAY_FINISH(vkRegions);
}

GR_VOID GR_STDCALL grCmdCopyImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcImage, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grSrcImage = (GrImage*)srcImage;
    GrImage* grDstImage = (GrImage*)destImage;
    unsigned srcTileSize = getVkFormatTileSize(grSrcImage->format);
    unsigned dstTileSize = getVkFormatTileSize(grDstImage->format);
    unsigned extentTileSize = srcTileSize > dstTileSize ? dstTileSize : srcTileSize;

    if (quirkHas(QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS)) {
        srcTileSize = 1;
        dstTileSize = 1;
        extentTileSize = 1;
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkImageCopy, vkRegions, 128, regionCount);

    for (unsigned i = 0; i < regionCount; i++) {
        const GR_IMAGE_COPY* region = &pRegions[i];

        vkRegions[i] = (VkImageCopy) {
            .srcSubresource = getVkImageSubresourceLayers(region->srcSubresource),
            .srcOffset = {
                region->srcOffset.x * srcTileSize,
                region->srcOffset.y * srcTileSize,
                region->srcOffset.z,
            },
            .dstSubresource = getVkImageSubresourceLayers(region->destSubresource),
            .dstOffset = {
                region->destOffset.x * dstTileSize,
                region->destOffset.y * dstTileSize,
                region->destOffset.z,
            },
            .extent = {
                region->extent.width * extentTileSize,
                region->extent.height * extentTileSize,
                region->extent.depth,
            },
        };
    }

    VKD.vkCmdCopyImage(grCmdBuffer->commandBuffer,
                       grSrcImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                       grDstImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                       regionCount, vkRegions);

    STACK_ARRAY_FINISH(vkRegions);
}

GR_VOID GR_STDCALL grCmdCopyMemoryToImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcMem, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grSrcGpuMemory = (GrGpuMemory*)srcMem;
    GrImage* grDstImage = (GrImage*)destImage;
    unsigned dstTileSize = getVkFormatTileSize(grDstImage->format);

    if (quirkHas(QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS)) {
        dstTileSize = 1;
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkBufferImageCopy, vkRegions, 128, regionCount);

    for (unsigned i = 0; i < regionCount; i++) {
        const GR_MEMORY_IMAGE_COPY* region = &pRegions[i];

        vkRegions[i] = (VkBufferImageCopy) {
            .bufferOffset = region->memOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = getVkImageSubresourceLayers(region->imageSubresource),
            .imageOffset = {
                region->imageOffset.x * dstTileSize,
                region->imageOffset.y * dstTileSize,
                region->imageOffset.z
            },
            .imageExtent = {
                MIN(region->imageExtent.width * dstTileSize,
                    MIP(grDstImage->extent.width, region->imageSubresource.mipLevel)),
                MIN(region->imageExtent.height * dstTileSize,
                    MIP(grDstImage->extent.height, region->imageSubresource.mipLevel)),
                region->imageExtent.depth,
            },
        };
    }

    VKD.vkCmdCopyBufferToImage(grCmdBuffer->commandBuffer,
                               grSrcGpuMemory->buffer, grDstImage->image,
                               getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                               regionCount, vkRegions);

    STACK_ARRAY_FINISH(vkRegions);
}

GR_VOID GR_STDCALL grCmdCopyImageToMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcImage, destMem, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grSrcImage = (GrImage*)srcImage;
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;
    unsigned srcTileSize = getVkFormatTileSize(grSrcImage->format);

    if (quirkHas(QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS)) {
        srcTileSize = 1;
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    STACK_ARRAY(VkBufferImageCopy, vkRegions, 128, regionCount);

    for (unsigned i = 0; i < regionCount; i++) {
        const GR_MEMORY_IMAGE_COPY* region = &pRegions[i];

        vkRegions[i] = (VkBufferImageCopy) {
            .bufferOffset = region->memOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = getVkImageSubresourceLayers(region->imageSubresource),
            .imageOffset = {
                region->imageOffset.x * srcTileSize,
                region->imageOffset.y * srcTileSize,
                region->imageOffset.z
            },
            .imageExtent = {
                MIN(region->imageExtent.width * srcTileSize,
                    MIP(grSrcImage->extent.width, region->imageSubresource.mipLevel)),
                MIN(region->imageExtent.height * srcTileSize,
                    MIP(grSrcImage->extent.height, region->imageSubresource.mipLevel)),
                region->imageExtent.depth,
            },
        };
    }

    VKD.vkCmdCopyImageToBuffer(grCmdBuffer->commandBuffer, grSrcImage->image,
                               getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                               grDstGpuMemory->buffer, regionCount, vkRegions);

    STACK_ARRAY_FINISH(vkRegions);
}

GR_VOID GR_STDCALL grCmdUpdateMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE dataSize,
    const GR_UINT32* pData)
{
    LOGT("%p %p %u %u %p\n", cmdBuffer, destMem, destOffset, dataSize, pData);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdUpdateBuffer(grCmdBuffer->commandBuffer, grDstGpuMemory->buffer, destOffset,
                          dataSize, pData);
}

GR_VOID GR_STDCALL grCmdFillMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE fillSize,
    GR_UINT32 data)
{
    LOGT("%p %p %u %u %u\n", cmdBuffer, destMem, destOffset, fillSize, data);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdFillBuffer(grCmdBuffer->commandBuffer, grDstGpuMemory->buffer, destOffset,
                        fillSize, data);
}

GR_VOID GR_STDCALL grCmdClearColorImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGT("%p %p %g %g %g %g %u %p\n",
         cmdBuffer, image, color[0], color[1], color[2], color[3], rangeCount, pRanges);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    grCmdBufferEndRenderPass(grCmdBuffer);

    const VkClearColorValue vkColor = {
        .float32 = { color[0], color[1], color[2], color[3] },
    };

    STACK_ARRAY(VkImageSubresourceRange, vkRanges, 128, rangeCount);

    for (unsigned i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->multiplyCubeLayers);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    STACK_ARRAY_FINISH(vkRanges);
}

GR_VOID GR_STDCALL grCmdClearColorImageRaw(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_UINT32 color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGT("%p %p %u %u %u %u %u %p\n",
         cmdBuffer, image, color[0], color[1], color[2], color[3], rangeCount, pRanges);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    grCmdBufferEndRenderPass(grCmdBuffer);

    GR_IMAGE_STATE imageState = quirkHas(QUIRK_IMAGE_DATA_TRANSFER_STATE_FOR_RAW_CLEAR) ?
                                GR_IMAGE_STATE_DATA_TRANSFER : GR_IMAGE_STATE_CLEAR;
    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    STACK_ARRAY(VkImageSubresourceRange, vkRanges, 128, rangeCount);

    for (unsigned i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->multiplyCubeLayers);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(imageState), &vkColor, rangeCount, vkRanges);

    STACK_ARRAY_FINISH(vkRanges);
}

GR_VOID GR_STDCALL grCmdClearDepthStencil(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    GR_FLOAT depth,
    GR_UINT8 stencil,
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    LOGT("%p %p %g %u %u %p\n", cmdBuffer, image, depth, stencil, rangeCount, pRanges);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    grCmdBufferEndRenderPass(grCmdBuffer);

    const VkClearDepthStencilValue depthStencilValue = {
        .depth = depth,
        .stencil = stencil,
    };

    STACK_ARRAY(VkImageSubresourceRange, vkRanges, 128, rangeCount);

    for (unsigned i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->multiplyCubeLayers);
    }

    VKD.vkCmdClearDepthStencilImage(grCmdBuffer->commandBuffer, grImage->image,
                                    getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                                    &depthStencilValue, rangeCount, vkRanges);

    STACK_ARRAY_FINISH(vkRanges);
}

GR_VOID GR_STDCALL grCmdSetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    LOGT("%p %p\n", cmdBuffer, event);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrEvent* grEvent = (GrEvent*)event;

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdSetEvent(grCmdBuffer->commandBuffer, grEvent->event,
                      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

GR_VOID GR_STDCALL grCmdResetEvent(
    GR_CMD_BUFFER cmdBuffer,
    GR_EVENT event)
{
    LOGT("%p %p\n", cmdBuffer, event);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrEvent* grEvent = (GrEvent*)event;

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdResetEvent(grCmdBuffer->commandBuffer, grEvent->event,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

GR_VOID GR_STDCALL grCmdBeginQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_FLAGS flags)
{
    LOGT("%p %p %u 0x%X\n", cmdBuffer, queryPool, slot, flags);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrQueryPool* grQueryPool = (GrQueryPool*)queryPool;

    VKD.vkCmdBeginQuery(grCmdBuffer->commandBuffer, grQueryPool->queryPool, slot,
                        flags & GR_QUERY_IMPRECISE_DATA ? 0 : VK_QUERY_CONTROL_PRECISE_BIT);
}

GR_VOID GR_STDCALL grCmdEndQuery(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot)
{
    LOGT("%p %p %u\n", cmdBuffer, queryPool, slot);
    const GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrQueryPool* grQueryPool = (GrQueryPool*)queryPool;

    VKD.vkCmdEndQuery(grCmdBuffer->commandBuffer, grQueryPool->queryPool, slot);
}

GR_VOID GR_STDCALL grCmdResetQueryPool(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount)
{
    LOGT("%p %p %u %u\n", cmdBuffer, queryPool, startQuery, queryCount);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrQueryPool* grQueryPool = (GrQueryPool*)queryPool;

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdResetQueryPool(grCmdBuffer->commandBuffer, grQueryPool->queryPool,
                            startQuery, queryCount);
}

GR_VOID GR_STDCALL grCmdWriteTimestamp(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM timestampType,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    LOGT("%p 0x%X %p %u\n", cmdBuffer, timestampType, destMem, destOffset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)destMem;

    VkPipelineStageFlags stageFlags = 0;
    if (timestampType == GR_TIMESTAMP_TOP) {
        stageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } else if (timestampType == GR_TIMESTAMP_BOTTOM) {
        stageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        LOGE("unhandled timestamp type 0x%X\n", timestampType);
        assert(false);
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdResetQueryPool(grCmdBuffer->commandBuffer, grCmdBuffer->timestampQueryPool, 0, 1);

    VKD.vkCmdWriteTimestamp(grCmdBuffer->commandBuffer, stageFlags,
                            grCmdBuffer->timestampQueryPool, 0);

    VKD.vkCmdCopyQueryPoolResults(grCmdBuffer->commandBuffer, grCmdBuffer->timestampQueryPool,
                                  0, 1, grGpuMemory->buffer, destOffset, sizeof(uint64_t),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

GR_VOID GR_STDCALL grCmdInitAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    const GR_UINT32* pData)
{
    LOGT("%p 0x%X %u %u %p\n", cmdBuffer, pipelineBindPoint, startCounter, counterCount, pData);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferEndRenderPass(grCmdBuffer);

    VkDeviceSize offset = startCounter * sizeof(uint32_t);
    VkDeviceSize size = counterCount * sizeof(uint32_t);

    const VkBufferMemoryBarrier preUpdateBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = grCmdBuffer->atomicCounterBuffer,
        .offset = offset,
        .size = size,
    };

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 1, &preUpdateBarrier, 0, NULL);

    VKD.vkCmdUpdateBuffer(grCmdBuffer->commandBuffer, grCmdBuffer->atomicCounterBuffer,
                          offset, size, pData);

    const VkBufferMemoryBarrier postUpdateBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                         VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT |
                         VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = grCmdBuffer->atomicCounterBuffer,
        .offset = offset,
        .size = size,
    };

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
                             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 1, &postUpdateBarrier, 0, NULL);
}

GR_VOID GR_STDCALL grCmdSaveAtomicCounters(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_UINT startCounter,
    GR_UINT counterCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset)
{
    LOGT("%p 0x%X %u %u %p %u\n",
         cmdBuffer, pipelineBindPoint, startCounter, counterCount, destMem, destOffset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grCmdBufferEndRenderPass(grCmdBuffer);

    const VkBufferCopy bufferCopy = {
        .srcOffset = startCounter * sizeof(uint32_t),
        .dstOffset = destOffset,
        .size = counterCount * sizeof(uint32_t),
    };

    const VkBufferMemoryBarrier preCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
                         VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT |
                         VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = grCmdBuffer->atomicCounterBuffer,
        .offset = bufferCopy.srcOffset,
        .size = bufferCopy.size,
    };

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
                             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 1, &preCopyBarrier, 0, NULL);

    VKD.vkCmdCopyBuffer(grCmdBuffer->commandBuffer, grCmdBuffer->atomicCounterBuffer,
                        grDstGpuMemory->buffer, 1, &bufferCopy);

    const VkBufferMemoryBarrier postCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                         VK_ACCESS_SHADER_WRITE_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT |
                         VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = grCmdBuffer->atomicCounterBuffer,
        .offset = bufferCopy.srcOffset,
        .size = bufferCopy.size,
    };

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
                             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, NULL, 1, &postCopyBarrier, 0, NULL);
}
