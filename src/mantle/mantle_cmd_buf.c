#include "mantle_internal.h"
#include "amdilc.h"

#define SETS_PER_POOL   (32)

typedef enum _DirtyFlags {
    FLAG_DIRTY_GRAPHICS_DESCRIPTOR_SETS = 1,
    FLAG_DIRTY_COMPUTE_DESCRIPTOR_SETS = 2,
    FLAG_DIRTY_DYNAMIC_GRAPHICS_MEMORY_BINDING = 4,
    FLAG_DIRTY_DYNAMIC_COMPUTE_MEMORY_BINDING = 8,
    FLAG_DIRTY_FRAMEBUFFER = 16,
    FLAG_DIRTY_PIPELINE = 32,
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

static void grCmdBufferBeginRenderPass(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    const GrPipeline* grPipeline =
        grCmdBuffer->bindPoint[VK_PIPELINE_BIND_POINT_GRAPHICS].grPipeline;

    if (grCmdBuffer->hasActiveRenderPass) {
        return;
    }

    const VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = grPipeline->renderPass,
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
    VkPipelineBindPoint bindPoint)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrPipeline* grPipeline = grCmdBuffer->bindPoint[bindPoint].grPipeline;

    VKD.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, bindPoint,
                                grPipeline->pipelineLayout, 0, 1,
                                &grCmdBuffer->grObj.grDevice->globalDescriptorSet.descriptorTable, 0, NULL);
    uint64_t descSetBuffer[2] = {
        grCmdBuffer->bindPoint[bindPoint].descriptorSets[0] == NULL ? 0 : (grCmdBuffer->bindPoint[bindPoint].descriptorSets[0]->bufferDevicePtr + sizeof(uint64_t) * grCmdBuffer->bindPoint[bindPoint].descriptorSetOffsets[0]),
        grCmdBuffer->bindPoint[bindPoint].descriptorSets[1] == NULL ? 0 : (grCmdBuffer->bindPoint[bindPoint].descriptorSets[1]->bufferDevicePtr + sizeof(uint64_t) * grCmdBuffer->bindPoint[bindPoint].descriptorSetOffsets[1])
    };
    VKD.vkCmdPushConstants(grCmdBuffer->commandBuffer,
                           grPipeline->pipelineLayout, bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ?  VK_SHADER_STAGE_ALL_GRAPHICS : VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(uint64_t) * 2, descSetBuffer);
}


static VkDescriptorSet allocateDynamicBindingSet(GrCmdBuffer* grCmdBuffer, VkPipelineBindPoint bindPoint) {
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pSetLayouts = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? &grDevice->globalDescriptorSet.graphicsDynamicMemoryLayout : &grDevice->globalDescriptorSet.computeDynamicMemoryLayout, 
    };
    VkDescriptorSet outSet;
    if (grCmdBuffer->descriptorPools == NULL || grCmdBuffer->descriptorPoolCount == 0) {
        goto pool_allocation;
    }
    allocInfo.descriptorPool = grCmdBuffer->descriptorPools[grCmdBuffer->descriptorPoolCount - 1];
    VkResult allocResult = VKD.vkAllocateDescriptorSets(grDevice->device, &allocInfo, &outSet);
    if (allocResult == VK_SUCCESS) {
        return outSet;
    }
    else if (allocResult != VK_ERROR_FRAGMENTED_POOL && allocResult != VK_ERROR_OUT_OF_POOL_MEMORY) {
        LOGE("failed to properly allocate descriptor sets for dynamic binding: %d\n", allocResult);
        assert(false);
    }
pool_allocation:
    LOGT("Allocating a new descriptor pool for dynamic binding for buffer %p\n", grCmdBuffer);
    // TODO: make "vector" more efficient
    grCmdBuffer->descriptorPoolCount++;
    grCmdBuffer->descriptorPools = (VkDescriptorPool*)realloc(grCmdBuffer->descriptorPools, sizeof(VkDescriptorPool) * grCmdBuffer->descriptorPoolCount);

    const VkDescriptorPoolSize poolSizes[2] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = SETS_PER_POOL,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = SETS_PER_POOL,
        }
    };
    const VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = SETS_PER_POOL,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes,
    };
    VkDescriptorPool tempPool;
    if (VKD.vkCreateDescriptorPool(grDevice->device, &poolCreateInfo, NULL,
                                   &tempPool) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool for dynamic binding failed\n");
        assert(false);
    }
    grCmdBuffer->descriptorPools[grCmdBuffer->descriptorPoolCount - 1] = tempPool;
    allocInfo.descriptorPool = tempPool;
    if (VKD.vkAllocateDescriptorSets(grDevice->device, &allocInfo, &outSet) != VK_SUCCESS) {
        LOGE("vkAllocateDescriptorSets failed for created descriptor pool\n");
        assert(false);
    }
    return outSet;
}

static void grCmdUpdateDynamicBuffers(
    GrCmdBuffer* grCmdBuffer,
    VkPipelineBindPoint bindPoint)
{
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    VkBuffer buffer = ((GrGpuMemory*)(grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.mem))->buffer;
    const VkDescriptorBufferInfo bufferInfo = {
        .buffer = buffer,
        .offset = grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.offset,
        .range = grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.range,
    };
    VkFormat vkFormat = getVkFormat(grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.format);
    VkBufferView bufferView = VK_NULL_HANDLE;
    unsigned writeOffset = 2;
    if (vkFormat != VK_FORMAT_UNDEFINED) {
        grCmdBuffer->bufferViews = (VkBufferView*)realloc(grCmdBuffer->bufferViews, sizeof(VkBufferView) * (1 + grCmdBuffer->bufferViewCount));
        const VkBufferViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .pNext = NULL,
            .buffer = buffer,
            .format = vkFormat,
            .offset = grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.offset,
            .range = grCmdBuffer->bindPoint[bindPoint].memoryBufferInfo.range,
        };
        assert(VKD.vkCreateBufferView(grDevice->device, &createInfo, NULL, &bufferView) == VK_SUCCESS);
        grCmdBuffer->bufferViews[grCmdBuffer->bufferViewCount] = bufferView;
        grCmdBuffer->bufferViewCount++;
        writeOffset = 0;
    }
    bool pushDescriptorsSupported = grDevice->pushDescriptorSetSupported;
    VkDescriptorSet writeSet = pushDescriptorsSupported ? VK_NULL_HANDLE : allocateDynamicBindingSet(grCmdBuffer, bindPoint);
    VkWriteDescriptorSet writeDescriptorSet[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = writeSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = NULL,
            .pTexelBufferView = &bufferView,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = writeSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = NULL,
            .pTexelBufferView = &bufferView,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = writeSet,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = &bufferInfo,
        }
    };
    VkPipelineLayout layout = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? grDevice->pipelineLayouts.graphicsPipelineLayout : grDevice->pipelineLayouts.computePipelineLayout;
    if (pushDescriptorsSupported) {
        VKD.vkCmdPushDescriptorSetKHR(grCmdBuffer->commandBuffer, bindPoint,
                                      layout,
                                      1,//TODO: move in define upwards
                                      COUNT_OF(writeDescriptorSet) - writeOffset, writeDescriptorSet + writeOffset);
    }
    else {
        VKD.vkUpdateDescriptorSets(grDevice->device, COUNT_OF(writeDescriptorSet) - writeOffset, writeDescriptorSet + writeOffset, 0, NULL);
        VKD.vkCmdBindDescriptorSets(grCmdBuffer->commandBuffer, bindPoint,
                                    layout, 1, 1, &writeSet, 0, NULL);
    }
}

static void grCmdBufferUpdateResources(
    GrCmdBuffer* grCmdBuffer)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrPipeline* grGraphicsPipeline =
        grCmdBuffer->bindPoint[VK_PIPELINE_BIND_POINT_GRAPHICS].grPipeline;

    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_GRAPHICS_DESCRIPTOR_SETS) {
        grCmdBufferUpdateDescriptorSets(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_COMPUTE_DESCRIPTOR_SETS) {
        grCmdBufferUpdateDescriptorSets(grCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
    }
    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_DYNAMIC_GRAPHICS_MEMORY_BINDING) {
        grCmdUpdateDynamicBuffers(grCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }
    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_DYNAMIC_COMPUTE_MEMORY_BINDING) {
        grCmdUpdateDynamicBuffers(grCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
    }
    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_FRAMEBUFFER) {
        grCmdBufferEndRenderPass(grCmdBuffer);

        grCmdBuffer->framebuffer = getVkFramebuffer(grDevice, grGraphicsPipeline->renderPass,
                                                    grCmdBuffer->attachmentCount,
                                                    grCmdBuffer->attachments,
                                                    grCmdBuffer->minExtent);

        // Track framebuffer
        grCmdBuffer->framebufferCount++;
        grCmdBuffer->framebuffers = realloc(grCmdBuffer->framebuffers,
                                            grCmdBuffer->framebufferCount * sizeof(VkFramebuffer));
        grCmdBuffer->framebuffers[grCmdBuffer->framebufferCount - 1] = grCmdBuffer->framebuffer;
    }

    if (grCmdBuffer->dirtyFlags & FLAG_DIRTY_PIPELINE) {
        VkPipeline vkPipeline =
            grPipelineFindOrCreateVkPipeline(grGraphicsPipeline, grCmdBuffer->grColorBlendState);

        VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    }

    grCmdBuffer->dirtyFlags = 0;
}

// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pipeline);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrPipeline* grPipeline = (GrPipeline*)pipeline;
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);

    if (grPipeline == grCmdBuffer->bindPoint[vkBindPoint].grPipeline) {
        return;
    }

    grCmdBuffer->bindPoint[vkBindPoint].grPipeline = grPipeline;

    if (pipelineBindPoint == GR_PIPELINE_BIND_POINT_GRAPHICS) {
        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_GRAPHICS_DESCRIPTOR_SETS |
                                   FLAG_DIRTY_FRAMEBUFFER |
                                   FLAG_DIRTY_PIPELINE;
    } else {
        // Pipeline creation isn't deferred for compute, bind now
        VKD.vkCmdBindPipeline(grCmdBuffer->commandBuffer, vkBindPoint,
                              grPipelineFindOrCreateVkPipeline(grPipeline, NULL));

        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_COMPUTE_DESCRIPTOR_SETS;
    }
}

GR_VOID grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, stateBindPoint, state);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

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
        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_PIPELINE;
    }   break;
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
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);

    if (index != 0) {
        LOGW("unsupported index %u\n", index);
    }

    grCmdBuffer->bindPoint[vkBindPoint].descriptorSets[index] = grDescriptorSet;
    grCmdBuffer->bindPoint[vkBindPoint].descriptorSetOffsets[index] = slotOffset;
    if (pipelineBindPoint == GR_PIPELINE_BIND_POINT_GRAPHICS) {
        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_GRAPHICS_DESCRIPTOR_SETS;
    } else {
        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_COMPUTE_DESCRIPTOR_SETS;
    }
}

GR_VOID grCmdBindDynamicMemoryView(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    const GR_MEMORY_VIEW_ATTACH_INFO* pMemView)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, pMemView);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    VkPipelineBindPoint vkBindPoint = getVkPipelineBindPoint(pipelineBindPoint);
    //TODO: temporary stub
    if (memcmp(pMemView, &grCmdBuffer->bindPoint[vkBindPoint].memoryBufferInfo, sizeof(GR_MEMORY_VIEW_ATTACH_INFO)) != 0) {
        memcpy(&grCmdBuffer->bindPoint[vkBindPoint].memoryBufferInfo, pMemView, sizeof(GR_MEMORY_VIEW_ATTACH_INFO));
        if (vkBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
            grCmdBuffer->dirtyFlags |= FLAG_DIRTY_DYNAMIC_GRAPHICS_MEMORY_BINDING;
        } else {
            grCmdBuffer->dirtyFlags |= FLAG_DIRTY_DYNAMIC_COMPUTE_MEMORY_BINDING;
        }
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
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferEndRenderPass(grCmdBuffer);

    VkBufferMemoryBarrier* barriers = malloc(transitionCount * sizeof(VkBufferMemoryBarrier));
    for (unsigned i = 0; i < transitionCount; i++) {
        const GR_MEMORY_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        GrGpuMemory* grGpuMemory = (GrGpuMemory*)stateTransition->mem;

        grGpuMemoryBindBuffer(grGpuMemory);

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
GR_VOID grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    LOGT("%p %u %p %p\n", cmdBuffer, colorTargetCount, pColorTargets, pDepthTarget);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

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
        grCmdBuffer->dirtyFlags |= FLAG_DIRTY_FRAMEBUFFER;
    }
}

GR_VOID grCmdPrepareImages(
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
                                                           grImage->isCube),
        };
    }

    VKD.vkCmdPipelineBarrier(grCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, transitionCount, barriers);
    free(barriers);
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->dirtyFlags != 0) {
        grCmdBufferUpdateResources(grCmdBuffer);
    }

    grCmdBufferBeginRenderPass(grCmdBuffer);

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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->dirtyFlags != 0) {
        grCmdBufferUpdateResources(grCmdBuffer);
    }

    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndexed(grCmdBuffer->commandBuffer,
                         indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

GR_VOID grCmdDrawIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %u\n", cmdBuffer, mem, offset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grCmdBuffer->dirtyFlags != 0) {
        grCmdBufferUpdateResources(grCmdBuffer);
    }

    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndirect(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset, 1, 0);
}

GR_VOID grCmdDrawIndexedIndirect(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGT("%p %p %u\n", cmdBuffer, mem, offset);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grGpuMemory = (GrGpuMemory*)mem;

    if (grCmdBuffer->dirtyFlags != 0) {
        grCmdBufferUpdateResources(grCmdBuffer);
    }

    grCmdBufferBeginRenderPass(grCmdBuffer);

    VKD.vkCmdDrawIndexedIndirect(grCmdBuffer->commandBuffer, grGpuMemory->buffer, offset, 1, 0);
}

GR_VOID grCmdDispatch(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT x,
    GR_UINT y,
    GR_UINT z)
{
    LOGT("%p %u %u %u\n", cmdBuffer, x, y, z);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->dirtyFlags != 0) {
        grCmdBufferUpdateResources(grCmdBuffer);
    }

    grCmdBufferEndRenderPass(grCmdBuffer);

    VKD.vkCmdDispatch(grCmdBuffer->commandBuffer, x, y, z);
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrGpuMemory* grSrcGpuMemory = (GrGpuMemory*)srcMem;
    GrGpuMemory* grDstGpuMemory = (GrGpuMemory*)destMem;

    grCmdBufferEndRenderPass(grCmdBuffer);
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grSrcImage = (GrImage*)srcImage;
    GrImage* grDstImage = (GrImage*)destImage;

    grCmdBufferEndRenderPass(grCmdBuffer);

    if (grSrcImage->image != VK_NULL_HANDLE) {
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

            vkRegions[i] = (VkBufferImageCopy) {
                .bufferOffset = grImageGetBufferOffset(grSrcImage->extent, grSrcImage->format,
                                                       region->srcSubresource.arraySlice,
                                                       grSrcImage->arrayLayers,
                                                       region->srcSubresource.mipLevel),
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = getVkImageSubresourceLayers(region->destSubresource),
                .imageOffset = {
                    region->destOffset.x,
                    region->destOffset.y,
                    region->destOffset.z,
                },
                .imageExtent = {
                    region->extent.width,
                    region->extent.height,
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

GR_VOID grCmdCopyMemoryToImage(
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

    grCmdBufferEndRenderPass(grCmdBuffer);
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

GR_VOID grCmdFillMemory(
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
    grGpuMemoryBindBuffer(grDstGpuMemory);

    VKD.vkCmdFillBuffer(grCmdBuffer->commandBuffer, grDstGpuMemory->buffer, destOffset,
                        fillSize, data);
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    grCmdBufferEndRenderPass(grCmdBuffer);

    const VkClearColorValue vkColor = {
        .float32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->isCube);
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
    const GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    GrImage* grImage = (GrImage*)image;

    grCmdBufferEndRenderPass(grCmdBuffer);

    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->isCube);
    }

    VKD.vkCmdClearColorImage(grCmdBuffer->commandBuffer, grImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}

GR_VOID grCmdClearDepthStencil(
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
        vkRanges[i] = getVkImageSubresourceRange(pRanges[i], grImage->isCube);
    }

    VKD.vkCmdClearDepthStencilImage(grCmdBuffer->commandBuffer, grImage->image,
                                    getVkImageLayout(GR_IMAGE_STATE_CLEAR), &depthStencilValue,
                                    rangeCount, vkRanges);

    free(vkRanges);
}

GR_VOID grCmdSetEvent(
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

GR_VOID grCmdResetEvent(
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
