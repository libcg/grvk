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
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

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
