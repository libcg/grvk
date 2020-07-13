#include "mantle_internal.h"

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
        vkCmdPipelineBarrier(grvkCmdBuffer->commandBuffer,
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
            .subresourceRange = {
                .aspectMask = getVkImageAspectFlags(range->aspect),
                .baseMipLevel = range->baseMipLevel,
                .levelCount = range->mipLevels == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_MIP_LEVELS : range->mipLevels,
                .baseArrayLayer = range->baseArraySlice,
                .layerCount = range->arraySize == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_ARRAY_LAYERS : range->arraySize,
            }
        };

        // TODO batch
        vkCmdPipelineBarrier(grvkCmdBuffer->commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}
