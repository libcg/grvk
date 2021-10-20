#include "mantle_internal.h"
#include "amdilc.h"

#define SETS_PER_POOL   (2048)

typedef enum _DirtyFlags {
    FLAG_DIRTY_DESCRIPTOR_SETS      = 1u << 0,
    FLAG_DIRTY_FRAMEBUFFER          = 1u << 1,
    FLAG_DIRTY_PIPELINE             = 1u << 2,
    FLAG_DIRTY_DYNAMIC_OFFSET       = 1u << 3,
} DirtyFlags;

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
            if (slot->type == SLOT_TYPE_NONE) {
                continue;
            } else if (slot->type != SLOT_TYPE_NESTED) {
                LOGE("unexpected slot type %d (should be nested)\n", slot->type);
                assert(false);
            }

            const DescriptorSetSlot* nestedSlot =
                getDescriptorSetSlot(slot->nested.nextSet, slot->nested.slotOffset,
                                     slotInfo->pNextLevelSet, bindingIndex);
            if (nestedSlot != NULL) {
                return nestedSlot;
            } else {
                continue;
            }
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
    GrCmdBuffer* grCmdBuffer,
    VkDescriptorSet vkDescriptorSet,
    VkPipelineLayout vkPipelineLayout,
    unsigned slotOffset,
    const GR_PIPELINE_SHADER* shaderInfo,
    const GrDescriptorSet* grDescriptorSet,
    const DescriptorSetSlot* dynamicMemoryView)
{
    const GrShader* grShader = (GrShader*)shaderInfo->shader;
    const GR_DYNAMIC_MEMORY_VIEW_SLOT_INFO* dynamicMapping = &shaderInfo->dynamicMemoryViewMapping;

    if (grShader == NULL) {
        // Nothing to update
        return;
    }

    VkDescriptorImageInfo* imageInfos = malloc(grShader->bindingCount *
                                               sizeof(VkDescriptorImageInfo));
    VkDescriptorBufferInfo* bufferInfos = malloc(grShader->bindingCount *
                                                 sizeof(VkDescriptorBufferInfo));
    VkWriteDescriptorSet* writes = malloc(grShader->bindingCount * sizeof(VkWriteDescriptorSet));

    for (unsigned i = 0; i < grShader->bindingCount; i++) {
        const IlcBinding* binding = &grShader->bindings[i];
        const DescriptorSetSlot* slot;

        if (dynamicMapping->slotObjectType != GR_SLOT_UNUSED &&
            (binding->index == (ILC_BASE_RESOURCE_ID + dynamicMapping->shaderEntityIndex))) {
            slot = dynamicMemoryView;
        } else {
            slot = getDescriptorSetSlot(grDescriptorSet, slotOffset,
                                        &shaderInfo->descriptorSetMapping[0], binding->index);
        }

        if (slot == NULL) {
            LOGE("can't find slot for binding %d\n", binding->index);
            assert(false);
        }

        writes[i] = (VkWriteDescriptorSet) {
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

            imageInfos[i] = (VkDescriptorImageInfo) {
                .sampler = slot->sampler.vkSampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            writes[i].pImageInfo = &imageInfos[i];
        } else if (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                   binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            if (slot->type != SLOT_TYPE_IMAGE_VIEW) {
                LOGE("unexpected slot type %d for descriptor type %d\n",
                     slot->type, binding->descriptorType);
                assert(false);
            }

            imageInfos[i] = (VkDescriptorImageInfo) {
                .sampler = VK_NULL_HANDLE,
                .imageView = slot->imageView.vkImageView,
                .imageLayout = slot->imageView.vkImageLayout,
            };
            writes[i].pImageInfo = &imageInfos[i];
        } else if (binding->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                   binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
            if (slot->type != SLOT_TYPE_MEMORY_VIEW) {
                LOGE("unexpected slot type %d for descriptor type %d\n",
                     slot->type, binding->descriptorType);
                assert(false);
            }

            writes[i].pTexelBufferView = &slot->memoryView.vkBufferView;
        } else if (binding->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            if (slot->type != SLOT_TYPE_MEMORY_VIEW) {
                LOGE("unexpected slot type %d for descriptor type %d\n",
                     slot->type, binding->descriptorType);
                assert(false);
            }

            bufferInfos[i] = (VkDescriptorBufferInfo) {
                .buffer = slot->memoryView.vkBuffer,
                .offset = slot->memoryView.offset,
                .range = slot->memoryView.range,
            };
            writes[i].pBufferInfo = &bufferInfos[i];

            if (slot == dynamicMemoryView) {
                // Buffer offset is passed during descriptor set binding
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                bufferInfos[i].offset = 0;
            }

            if (binding->strideIndex >= 0) {
                // Pass memory view stride through push constants
                uint32_t stride = slot->memoryView.stride;

                VKD.vkCmdPushConstants(grCmdBuffer->commandBuffer, vkPipelineLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT,
                                       binding->strideIndex * sizeof(uint32_t),
                                       sizeof(uint32_t), &stride);
            }
        } else {
            LOGE("unhandled descriptor type %d\n", binding->descriptorType);
            assert(false);
        }
    }

    VKD.vkUpdateDescriptorSets(grDevice->device, grShader->bindingCount, writes, 0, NULL);

    free(imageInfos);
    free(bufferInfos);
    free(writes);
}

static void grCmdBufferBeginRenderPass(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrPipeline* grPipeline =
        grCmdBuffer->bindPoints[VK_PIPELINE_BIND_POINT_GRAPHICS].grPipeline;

    if (grCmdBuffer->hasActiveRenderPass) {
        return;
    }

    VkRenderPass renderPass = grPipeline->renderPasses[grCmdBuffer->grMsaaState->renderPassIndex];

    const VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = renderPass,
        .framebuffer = grCmdBuffer->framebuffer,
        .renderArea = (VkRect2D) {
            .offset = { 0, 0 },
            .extent = { grCmdBuffer->minExtent.width, grCmdBuffer->minExtent.height },
        },
        .clearValueCount = 0,
        .pClearValues = NULL,
    };

    VKD.vkCmdBeginRenderPass(grCmdBuffer->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    grCmdBuffer->hasActiveRenderPass = true;
}

void grCmdBufferEndRenderPass(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (!grCmdBuffer->hasActiveRenderPass) {
        return;
    }

    VKD.vkCmdEndRenderPass(grCmdBuffer->commandBuffer);
    grCmdBuffer->hasActiveRenderPass = false;
}

static void grCmdBufferUpdateDescriptorSets(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint vkBindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];
    GrPipeline* grPipeline = bindPoint->grPipeline;
    VkResult vkRes;

    for (unsigned i = 0; i < 2; i++) {
        if (bindPoint->descriptorPool != VK_NULL_HANDLE) {
            const VkDescriptorSetAllocateInfo descSetAllocateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = NULL,
                .descriptorPool = bindPoint->descriptorPool,
                .descriptorSetCount = grPipeline->stageCount, // TODO optimize
                .pSetLayouts = grPipeline->descriptorSetLayouts,
            };

            vkRes = VKD.vkAllocateDescriptorSets(grDevice->device, &descSetAllocateInfo,
                                                 bindPoint->descriptorSets);
            if (vkRes == VK_SUCCESS) {
                break;
            } else if (vkRes != VK_ERROR_OUT_OF_POOL_MEMORY) {
                LOGE("vkAllocateDescriptorSets failed (%d)\n", vkRes);
                break;
            } else if (i > 0) {
                LOGE("descriptor set allocation failed with a new pool\n");
                assert(false);
            }
        }

        // Need a new pool
        bindPoint->descriptorPool = getVkDescriptorPool(grDevice);

        // Track descriptor pool
        grCmdBuffer->descriptorPoolCount++;
        grCmdBuffer->descriptorPools = realloc(grCmdBuffer->descriptorPools,
                                               grCmdBuffer->descriptorPoolCount *
                                               sizeof(VkDescriptorPool));
        grCmdBuffer->descriptorPools[grCmdBuffer->descriptorPoolCount - 1] =
            bindPoint->descriptorPool;
    }

    for (unsigned i = 0; i < grPipeline->stageCount; i++) {
        updateVkDescriptorSet(grDevice, grCmdBuffer, bindPoint->descriptorSets[i],
                              grPipeline->pipelineLayout, bindPoint->slotOffset,
                              &grPipeline->shaderInfos[i], bindPoint->grDescriptorSet,
                              &bindPoint->dynamicMemoryView);
    }
}

static void grCmdBufferBindDescriptorSets(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint vkBindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const BindPoint* bindPoint = &grCmdBuffer->bindPoints[vkBindPoint];
    const GrPipeline* grPipeline = bindPoint->grPipeline;

    uint32_t dynamicOffset = bindPoint->dynamicMemoryView.memoryView.offset;
    uint32_t dynamicOffsets[MAX_STAGE_COUNT];

    for (unsigned i = 0; i < grPipeline->dynamicOffsetCount; i++) {
        dynamicOffsets[i] = dynamicOffset;
    }

    VKD.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, vkBindPoint, grPipeline->pipelineLayout,
                                0, grPipeline->stageCount, bindPoint->descriptorSets,
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

    if (dirtyFlags & FLAG_DIRTY_DESCRIPTOR_SETS) {
        grCmdBufferUpdateDescriptorSets(grCmdBuffer, vkBindPoint);
    }

    if (dirtyFlags & (FLAG_DIRTY_DESCRIPTOR_SETS | FLAG_DIRTY_DYNAMIC_OFFSET)) {
        grCmdBufferBindDescriptorSets(grCmdBuffer, vkBindPoint);
    }

    if (dirtyFlags & FLAG_DIRTY_FRAMEBUFFER) {
        grCmdBufferEndRenderPass(grCmdBuffer);

        VkRenderPass renderPass =
            grPipeline->renderPasses[grCmdBuffer->grMsaaState->renderPassIndex];

        grCmdBuffer->framebuffer = getVkFramebuffer(grDevice, renderPass,
                                                    grCmdBuffer->attachmentCount,
                                                    grCmdBuffer->attachments,
                                                    grCmdBuffer->minExtent);

        // Track framebuffer
        grCmdBuffer->framebufferCount++;
        grCmdBuffer->framebuffers =
            realloc(grCmdBuffer->framebuffers,
                    grCmdBuffer->framebufferCount * sizeof(VkFramebuffer));
        grCmdBuffer->framebuffers[grCmdBuffer->framebufferCount - 1] = grCmdBuffer->framebuffer;
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
        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SETS |
                                 FLAG_DIRTY_FRAMEBUFFER |
                                 FLAG_DIRTY_PIPELINE;
    } else {
        // Pipeline creation isn't deferred for compute, bind now
        VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer, vkBindPoint,
                              grPipelineFindOrCreateVkPipeline(grPipeline, NULL, NULL, NULL));

        bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SETS;
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
        grCmdBuffer->grRasterState = rasterState;
        bindPoint->dirtyFlags |= FLAG_DIRTY_PIPELINE;
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
        VKD.vkCmdSetDepthBounds(grCmdBuffer->commandBuffer,
                                depthStencilState->minDepthBounds, depthStencilState->maxDepthBounds);
        grCmdBuffer->grDepthStencilState = depthStencilState;
    }   break;
    case GR_STATE_BIND_COLOR_BLEND: {
        GrColorBlendStateObject* colorBlendState = (GrColorBlendStateObject*)state;
        if (colorBlendState == grCmdBuffer->grColorBlendState) {
            break;
        }

        VKD.vkCmdSetBlendConstants(grCmdBuffer->commandBuffer, colorBlendState->blendConstants);
        grCmdBuffer->grColorBlendState = colorBlendState;
        bindPoint->dirtyFlags |= FLAG_DIRTY_PIPELINE;
    }   break;
    case GR_STATE_BIND_MSAA: {
        GrMsaaStateObject* msaaState = (GrMsaaStateObject*)state;
        if (msaaState == grCmdBuffer->grMsaaState) {
            break;
        }

        grCmdBuffer->grMsaaState = msaaState;
        bindPoint->dirtyFlags |= FLAG_DIRTY_FRAMEBUFFER | FLAG_DIRTY_PIPELINE;
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

    if (index != 0) {
        LOGW("unsupported index %u\n", index);
    }

    if (grDescriptorSet == bindPoint->grDescriptorSet && slotOffset == bindPoint->slotOffset) {
        return;
    }

    bindPoint->grDescriptorSet = grDescriptorSet;
    bindPoint->slotOffset = slotOffset;
    bindPoint->dirtyFlags |= FLAG_DIRTY_DESCRIPTOR_SETS;
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
    DescriptorSetSlot* dynamicMemoryView = &bindPoint->dynamicMemoryView;

    // FIXME what is pMemView->state for?

    bool dirtySlot = grGpuMemory->buffer != dynamicMemoryView->memoryView.vkBuffer ||
                     pMemView->range != dynamicMemoryView->memoryView.range ||
                     pMemView->stride != dynamicMemoryView->memoryView.stride;
    bool dirtyOffset = pMemView->offset != dynamicMemoryView->memoryView.offset;

    if (!dirtySlot && !dirtyOffset) {
        return;
    }

    *dynamicMemoryView = (DescriptorSetSlot) {
        .type = SLOT_TYPE_MEMORY_VIEW,
        .memoryView = {
            .vkBufferView = VK_NULL_HANDLE,
            .vkBuffer = grGpuMemory->buffer,
            .offset = pMemView->offset,
            .range = pMemView->range,
            .stride = pMemView->stride,
        },
    };

    bindPoint->dirtyFlags |= (dirtySlot ? FLAG_DIRTY_DESCRIPTOR_SETS : 0) |
                             (dirtyOffset ? FLAG_DIRTY_DYNAMIC_OFFSET : 0);
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

    VkBufferMemoryBarrier* barriers = malloc(transitionCount * sizeof(VkBufferMemoryBarrier));
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
    }

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, transitionCount, barriers, 0, NULL);
    free(barriers);
}

// FIXME what are target states for?
GR_VOID GR_STDCALL grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    LOGT("%p %u %p %p\n", cmdBuffer, colorTargetCount, pColorTargets, pDepthTarget);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    BindPoint* bindPoint = &grCmdBuffer->bindPoints[VK_PIPELINE_BIND_POINT_GRAPHICS];

    assert(colorTargetCount <= GR_MAX_COLOR_TARGETS);

    // Find minimum extent
    VkExtent3D minExtent = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
    for (unsigned i = 0; i < colorTargetCount; i++) {
        const GrColorTargetView* grColorTargetView = (GrColorTargetView*)pColorTargets[i].view;

        if (grColorTargetView != NULL) {
            minExtent.width = MIN(minExtent.width, grColorTargetView->extent.width);
            minExtent.height = MIN(minExtent.height, grColorTargetView->extent.height);
            minExtent.depth = MIN(minExtent.depth, grColorTargetView->extent.depth);
        }
    }
    if (pDepthTarget != NULL) {
        const GrDepthStencilView* grDepthStencilView = (GrDepthStencilView*)pDepthTarget->view;

        if (grDepthStencilView != NULL) {
            minExtent.width = MIN(minExtent.width, grDepthStencilView->extent.width);
            minExtent.height = MIN(minExtent.height, grDepthStencilView->extent.height);
            minExtent.depth = MIN(minExtent.depth, grDepthStencilView->extent.depth);
        }
    }

    // Copy attachments
    unsigned attachmentCount = 0;
    VkImageView attachments[COUNT_OF(grCmdBuffer->attachments)];
    for (unsigned i = 0; i < colorTargetCount; i++) {
        const GrColorTargetView* grColorTargetView = (GrColorTargetView*)pColorTargets[i].view;

        if (grColorTargetView != NULL) {
            attachments[attachmentCount] = grColorTargetView->imageView;
            attachmentCount++;
        }
    }
    if (pDepthTarget != NULL) {
        const GrDepthStencilView* grDepthStencilView = (GrDepthStencilView*)pDepthTarget->view;

        if (grDepthStencilView != NULL) {
            attachments[attachmentCount] = grDepthStencilView->imageView;
            attachmentCount++;
        }
    }

    if (memcmp(&minExtent, &grCmdBuffer->minExtent, sizeof(minExtent)) != 0 ||
        attachmentCount != grCmdBuffer->attachmentCount ||
        memcmp(attachments, grCmdBuffer->attachments, attachmentCount * sizeof(VkImageView)) != 0) {
        // Targets have changed
        grCmdBuffer->minExtent = minExtent;
        grCmdBuffer->attachmentCount = attachmentCount;
        memcpy(grCmdBuffer->attachments, attachments, attachmentCount * sizeof(VkImageView));

        bindPoint->dirtyFlags |= FLAG_DIRTY_FRAMEBUFFER;
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

    VkImageMemoryBarrier* barriers = malloc(transitionCount * sizeof(VkImageMemoryBarrier));
    for (unsigned i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrImage* grImage = (GrImage*)stateTransition->image;
        bool multiplyCubeLayers = quirkHas(QUIRK_CUBEMAP_LAYER_DIV_6) && grImage->isCube;

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
                                                           multiplyCubeLayers),
        };
    }

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, transitionCount, barriers);
    free(barriers);
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

    if (quirkHas(QUIRK_COMPRESSED_IMAGE_COPY_IN_TEXELS)) {
        srcTileSize = 1;
        dstTileSize = 1;
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    if (grSrcImage->image != VK_NULL_HANDLE) {
        VkImageCopy* vkRegions = malloc(regionCount * sizeof(VkImageCopy));
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
                    region->extent.width * dstTileSize,
                    region->extent.height * dstTileSize,
                    region->extent.depth,
                },
            };
        }

        VKD.vkCmdCopyImage(grCmdBuffer->commandBuffer,
                           grSrcImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                           grDstImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                           regionCount, vkRegions);

        free(vkRegions);
    } else {
        VkBufferImageCopy* vkRegions = malloc(regionCount * sizeof(VkBufferImageCopy));
        for (unsigned i = 0; i < regionCount; i++) {
            const GR_IMAGE_COPY* region = &pRegions[i];

            if (region->srcSubresource.aspect != GR_IMAGE_ASPECT_COLOR) {
                LOGW("unhandled non-color aspect 0x%X\n", region->srcSubresource.aspect);
            }
            if (region->srcOffset.x != 0 || region->srcOffset.y != 0 || region->srcOffset.z != 0) {
                LOGW("unhandled region offset %u %u %u for buffer\n",
                     region->srcOffset.x, region->srcOffset.y, region->srcOffset.z);
            }

            const VkExtent3D srcTexelExtent = {
                grSrcImage->extent.width * srcTileSize,
                grSrcImage->extent.height * srcTileSize,
                grSrcImage->extent.depth,
            };

            vkRegions[i] = (VkBufferImageCopy) {
                .bufferOffset = grImageGetBufferOffset(srcTexelExtent, grSrcImage->format,
                                                       region->srcSubresource.arraySlice,
                                                       grSrcImage->arrayLayers,
                                                       region->srcSubresource.mipLevel),
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = getVkImageSubresourceLayers(region->destSubresource),
                .imageOffset = {
                    region->destOffset.x * dstTileSize,
                    region->destOffset.y * dstTileSize,
                    region->destOffset.z,
                },
                .imageExtent = {
                    region->extent.width * dstTileSize,
                    region->extent.height * dstTileSize,
                    region->extent.depth,
                },
            };
        }

        VKD.vkCmdCopyBufferToImage(grCmdBuffer->commandBuffer,
                                   grSrcImage->buffer, grDstImage->image,
                                   getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                                   regionCount, vkRegions);

        free(vkRegions);
    }
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

    VkBufferImageCopy* vkRegions = malloc(regionCount * sizeof(VkBufferImageCopy));
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

    VKD.vkCmdCopyBufferToImage(grCmdBuffer->commandBuffer, grSrcGpuMemory->buffer,
                               grDstImage->image, getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
                               regionCount, vkRegions);

    free(vkRegions);
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

    VkBufferImageCopy* vkRegions = malloc(regionCount * sizeof(VkBufferImageCopy));
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

    free(vkRegions);
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

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        bool multiplyCubeLayers = quirkHas(QUIRK_CUBEMAP_LAYER_DIV_6) && grImage->isCube;

        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], multiplyCubeLayers);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
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

    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        bool multiplyCubeLayers = quirkHas(QUIRK_CUBEMAP_LAYER_DIV_6) && grImage->isCube;

        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], multiplyCubeLayers);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
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

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        bool multiplyCubeLayers = quirkHas(QUIRK_CUBEMAP_LAYER_DIV_6) && grImage->isCube;

        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], multiplyCubeLayers);
    }

    VKD.vkCmdClearDepthStencilImage(grCmdBuffer->commandBuffer, grImage->image,
                                    getVkImageLayout(GR_IMAGE_STATE_CLEAR), &depthStencilValue,
                                    rangeCount, vkRanges);

    free(vkRanges);
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
