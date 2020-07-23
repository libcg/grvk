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

// Command Buffer Building Functions

GR_VOID grCmdBindPipeline(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_PIPELINE pipeline)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    GrvkPipeline* grvkPipeline = (GrvkPipeline*)pipeline;

    vki.vkCmdBindPipeline(grvkCmdBuffer->commandBuffer, getVkPipelineBindPoint(pipelineBindPoint),
                          grvkPipeline->pipeline);
    grvkCmdBuffer->boundPipeline = grvkPipeline;

    // Check if the descriptor set needs binding. Pipeline layout is required.
    if (grvkCmdBuffer->descriptorSet != NULL && !grvkCmdBuffer->descriptorSetIsBound) {
        grCmdBindDescriptorSet(cmdBuffer, pipelineBindPoint, 0,
                               (GR_DESCRIPTOR_SET)grvkCmdBuffer->descriptorSet, 0);
    }
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

    if (grvkCmdBuffer->boundPipeline != NULL) {
        printf("%s: HACK only one descriptor bound\n", __func__);
        vki.vkCmdBindDescriptorSets(grvkCmdBuffer->commandBuffer,
                                    getVkPipelineBindPoint(pipelineBindPoint),
                                    grvkCmdBuffer->boundPipeline->pipelineLayout,
                                    0, 1, grvkDescriptorSet->descriptorSets,
                                    0, NULL);
        grvkCmdBuffer->descriptorSet = grvkDescriptorSet;
        grvkCmdBuffer->descriptorSetIsBound = true;
    } else {
        // Defer descriptor set binding to pipeline bind call
        grvkCmdBuffer->descriptorSet = grvkDescriptorSet;
        grvkCmdBuffer->descriptorSetIsBound = false;
    }
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
