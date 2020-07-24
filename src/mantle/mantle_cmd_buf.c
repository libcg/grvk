#include "mantle_internal.h"

static VkImageSubresourceRange getVkImageSubresourceRange(
    const GR_IMAGE_SUBRESOURCE_RANGE* range)
{
    return (VkImageSubresourceRange) {
        .aspectMask = getVkImageAspectFlags(range->aspect),
        .baseMipLevel = range->baseMipLevel,
        .levelCount = range->mipLevels == GR_LAST_MIP_OR_SLICE ?
                      VK_REMAINING_MIP_LEVELS : range->mipLevels,
        .baseArrayLayer = range->baseArraySlice,
        .layerCount = range->arraySize == GR_LAST_MIP_OR_SLICE ?
                      VK_REMAINING_ARRAY_LAYERS : range->arraySize,
    };
}

static VkFramebuffer getVkFramebuffer(
    VkDevice device,
    VkRenderPass renderPass,
    uint32_t colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    VkImageView attachments[GR_MAX_COLOR_TARGETS + 1];
    int attachmentIdx = 0;

    for (int i = 0; i < colorTargetCount; i++) {
        GrvkColorTargetView* grvkColorTargetView = (GrvkColorTargetView*)pColorTargets[i].view;

        attachments[attachmentIdx++] = grvkColorTargetView->imageView;
    }

    // TODO
    if (pDepthTarget != NULL) {
        printf("%s: depth targets are not supported\n", __func__);
    }

    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = attachmentIdx,
        .pAttachments = attachments,
        .width = 1280, // FIXME hardcoded
        .height = 720, // FIXME hardcoded
        .layers = 1, // FIXME hardcoded
    };

    if (vki.vkCreateFramebuffer(device, &framebufferCreateInfo, NULL,
                                &framebuffer) != VK_SUCCESS) {
        printf("%s: vkCreateFramebuffer failed\n", __func__);
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}

static void initCmdBufferResources(
    GrvkCmdBuffer* grvkCmdBuffer)
{
    GrvkPipeline* grvkPipeline = grvkCmdBuffer->grvkPipeline;
    VkPipelineBindPoint bindPoint = getVkPipelineBindPoint(GR_PIPELINE_BIND_POINT_GRAPHICS);

    vki.vkCmdBindPipeline(grvkCmdBuffer->commandBuffer, bindPoint, grvkPipeline->pipeline);

    printf("%s: HACK only one descriptor bound\n", __func__);
    vki.vkCmdBindDescriptorSets(grvkCmdBuffer->commandBuffer, bindPoint,
                                grvkPipeline->pipelineLayout, 0, 1,
                                grvkCmdBuffer->grvkDescriptorSet->descriptorSets, 0, NULL);

    VkFramebuffer framebuffer =
        getVkFramebuffer(grvkCmdBuffer->grvkDescriptorSet->device, grvkPipeline->renderPass,
                         grvkCmdBuffer->colorTargetCount, grvkCmdBuffer->colorTargets,
                         grvkCmdBuffer->hasDepthTarget ? &grvkCmdBuffer->depthTarget : NULL);

    const VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = grvkPipeline->renderPass,
        .framebuffer = framebuffer,
        .renderArea = (VkRect2D) {
            .offset = { 0, 0 }, // FIXME hardcoded
            .extent = { 1280, 720 }, // FIXME hardcoded
        },
        .clearValueCount = 0,
        .pClearValues = NULL,
    };

    if (grvkCmdBuffer->hasActiveRenderPass) {
        vki.vkCmdEndRenderPass(grvkCmdBuffer->commandBuffer);
    }
    vki.vkCmdBeginRenderPass(grvkCmdBuffer->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    grvkCmdBuffer->hasActiveRenderPass = true;

    grvkCmdBuffer->isDirty = false;
}

// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkPipeline* grvkPipeline = (GrvkPipeline*)pipeline;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        printf("%s: unsupported bind point 0x%x\n", __func__, pipelineBindPoint);
    }

    grvkCmdBuffer->grvkPipeline = grvkPipeline;
    grvkCmdBuffer->isDirty = true;
}

GR_VOID grCmdBindStateObject(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM stateBindPoint,
    GR_STATE_OBJECT state)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkViewportStateObject* viewportState = (GrvkViewportStateObject*)state;
    GrvkRasterStateObject* rasterState = (GrvkRasterStateObject*)state;
    GrvkDepthStencilStateObject* depthStencilState = (GrvkDepthStencilStateObject*)state;
    GrvkColorBlendStateObject* colorBlendState = (GrvkColorBlendStateObject*)state;

    switch ((GR_STATE_BIND_POINT)stateBindPoint) {
    case GR_STATE_BIND_VIEWPORT:
        vki.vkCmdSetViewportWithCountEXT(grvkCmdBuffer->commandBuffer,
                                         viewportState->viewportCount, viewportState->viewports);
        vki.vkCmdSetScissorWithCountEXT(grvkCmdBuffer->commandBuffer, viewportState->scissorCount,
                                        viewportState->scissors);
        break;
    case GR_STATE_BIND_RASTER:
        vki.vkCmdSetCullModeEXT(grvkCmdBuffer->commandBuffer, rasterState->cullMode);
        vki.vkCmdSetFrontFaceEXT(grvkCmdBuffer->commandBuffer, rasterState->frontFace);
        vki.vkCmdSetDepthBias(grvkCmdBuffer->commandBuffer, rasterState->depthBiasConstantFactor,
                              rasterState->depthBiasClamp, rasterState->depthBiasSlopeFactor);
        break;
    case GR_STATE_BIND_DEPTH_STENCIL:
        vki.vkCmdSetDepthTestEnableEXT(grvkCmdBuffer->commandBuffer,
                                       depthStencilState->depthTestEnable);
        vki.vkCmdSetDepthWriteEnableEXT(grvkCmdBuffer->commandBuffer,
                                        depthStencilState->depthWriteEnable);
        vki.vkCmdSetDepthCompareOpEXT(grvkCmdBuffer->commandBuffer,
                                      depthStencilState->depthCompareOp);
        vki.vkCmdSetDepthBoundsTestEnableEXT(grvkCmdBuffer->commandBuffer,
                                             depthStencilState->depthBoundsTestEnable);
        vki.vkCmdSetStencilTestEnableEXT(grvkCmdBuffer->commandBuffer,
                                         depthStencilState->stencilTestEnable);
        vki.vkCmdSetStencilOpEXT(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                 depthStencilState->front.failOp,
                                 depthStencilState->front.passOp,
                                 depthStencilState->front.depthFailOp,
                                 depthStencilState->front.compareOp);
        vki.vkCmdSetStencilCompareMask(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                       depthStencilState->front.compareMask);
        vki.vkCmdSetStencilWriteMask(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.writeMask);
        vki.vkCmdSetStencilReference(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_FRONT_BIT,
                                     depthStencilState->front.reference);
        vki.vkCmdSetStencilOpEXT(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                 depthStencilState->back.failOp,
                                 depthStencilState->back.passOp,
                                 depthStencilState->back.depthFailOp,
                                 depthStencilState->back.compareOp);
        vki.vkCmdSetStencilCompareMask(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                       depthStencilState->back.compareMask);
        vki.vkCmdSetStencilWriteMask(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.writeMask);
        vki.vkCmdSetStencilReference(grvkCmdBuffer->commandBuffer, VK_STENCIL_FACE_BACK_BIT,
                                     depthStencilState->back.reference);
        vki.vkCmdSetDepthBounds(grvkCmdBuffer->commandBuffer,
                                depthStencilState->minDepthBounds, depthStencilState->maxDepthBounds);
        break;
    case GR_STATE_BIND_COLOR_BLEND:
        vki.vkCmdSetBlendConstants(grvkCmdBuffer->commandBuffer, colorBlendState->blendConstants);
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
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkDescriptorSet* grvkDescriptorSet = (GrvkDescriptorSet*)descriptorSet;

    if (pipelineBindPoint != GR_PIPELINE_BIND_POINT_GRAPHICS) {
        printf("%s: unsupported bind point 0x%x\n", __func__, pipelineBindPoint);
    } else if (index != 0) {
        printf("%s: unsupported index %u\n", __func__, index);
    } else if (slotOffset != 0) {
        printf("%s: unsupported slot offset %u\n", __func__, slotOffset);
    }

    grvkCmdBuffer->grvkDescriptorSet = grvkDescriptorSet;
    grvkCmdBuffer->isDirty = true;
}

GR_VOID grCmdPrepareMemoryRegions(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_MEMORY_STATE_TRANSITION* pStateTransitions)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_MEMORY_STATE_TRANSITION* stateTransition = &pStateTransitions[i];

        // TODO use buffer memory barrier
        const VkMemoryBarrier memoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsMemory(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsMemory(stateTransition->newState),
        };

        // TODO batch
        vki.vkCmdPipelineBarrier(grvkCmdBuffer->commandBuffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                                 0, 1, &memoryBarrier, 0, NULL, 0, NULL);
    }
}

GR_VOID grCmdBindTargets(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT colorTargetCount,
    const GR_COLOR_TARGET_BIND_INFO* pColorTargets,
    const GR_DEPTH_STENCIL_BIND_INFO* pDepthTarget)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    memcpy(grvkCmdBuffer->colorTargets, pColorTargets,
           sizeof(GR_COLOR_TARGET_BIND_INFO) * colorTargetCount);
    grvkCmdBuffer->colorTargetCount = colorTargetCount;

    if (pDepthTarget == NULL) {
        grvkCmdBuffer->hasDepthTarget = false;
    } else {
        memcpy(&grvkCmdBuffer->depthTarget, pDepthTarget, sizeof(GR_DEPTH_STENCIL_BIND_INFO));
        grvkCmdBuffer->hasDepthTarget = true;
    }
}

GR_VOID grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    const GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        const GR_IMAGE_SUBRESOURCE_RANGE* range = &stateTransition->subresourceRange;
        GrvkImage* grvkImage = (GrvkImage*)stateTransition->image;

        const VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlagsImage(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlagsImage(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = grvkImage->image,
            .subresourceRange = getVkImageSubresourceRange(range),
        };

        // TODO batch
        vki.vkCmdPipelineBarrier(grvkCmdBuffer->commandBuffer,
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
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    if (grvkCmdBuffer->isDirty) {
        initCmdBufferResources(grvkCmdBuffer);
    }

    vki.vkCmdDraw(grvkCmdBuffer->commandBuffer,
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
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    if (grvkCmdBuffer->isDirty) {
        initCmdBufferResources(grvkCmdBuffer);
    }

    vki.vkCmdDrawIndexed(grvkCmdBuffer->commandBuffer,
                         indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

GR_VOID grCmdClearColorImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE image,
    const GR_FLOAT color[4],
    GR_UINT rangeCount,
    const GR_IMAGE_SUBRESOURCE_RANGE* pRanges)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkImage* grvkImage = (GrvkImage*)image;

    const VkClearColorValue vkColor = {
        .float32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(&pRanges[i]);
    }

    vki.vkCmdClearColorImage(grvkCmdBuffer->commandBuffer, grvkImage->image,
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
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkImage* grvkImage = (GrvkImage*)image;

    const VkClearColorValue vkColor = {
        .uint32 = { color[0], color[1], color[2], color[3] },
    };

    VkImageSubresourceRange* vkRanges = malloc(rangeCount * sizeof(VkImageSubresourceRange));
    for (int i = 0; i < rangeCount; i++) {
        vkRanges[i] = getVkImageSubresourceRange(&pRanges[i]);
    }

    vki.vkCmdClearColorImage(grvkCmdBuffer->commandBuffer, grvkImage->image,
                             getVkImageLayout(GR_IMAGE_STATE_CLEAR),
                             &vkColor, rangeCount, vkRanges);

    free(vkRanges);
}
