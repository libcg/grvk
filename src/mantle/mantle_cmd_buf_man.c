#include "mantle_internal.h"

// Command Buffer Management Functions

GR_RESULT grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    GrvkDevice* grvkDevice = (GrvkDevice*)device;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    if (pCreateInfo->queueType == GR_QUEUE_UNIVERSAL) {
        vkCommandPool = grvkDevice->universalCommandPool;
    } else if (pCreateInfo->queueType == GR_QUEUE_COMPUTE) {
        vkCommandPool = grvkDevice->computeCommandPool;
    }

    if (vkCommandPool == VK_NULL_HANDLE) {
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vki.vkAllocateCommandBuffers(grvkDevice->device, &allocateInfo,
                                     &vkCommandBuffer) != VK_SUCCESS) {
        printf("%s: vkAllocateCommandBuffers failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrvkCmdBuffer* grvkCmdBuffer = malloc(sizeof(GrvkCmdBuffer));
    *grvkCmdBuffer = (GrvkCmdBuffer) {
        .sType = GRVK_STRUCT_TYPE_COMMAND_BUFFER,
        .commandBuffer = vkCommandBuffer,
        .boundPipeline = NULL,
        .descriptorSet = NULL,
        .descriptorSetIsBound = false,
    };

    *pCmdBuffer = (GR_CMD_BUFFER)grvkCmdBuffer;
    return GR_SUCCESS;
}

GR_RESULT grBeginCommandBuffer(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;
    VkCommandBufferUsageFlags vkUsageFlags = 0;

    if ((flags & GR_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT) != 0) {
        vkUsageFlags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = vkUsageFlags,
        .pInheritanceInfo = NULL,
    };

    if (vki.vkBeginCommandBuffer(grvkCmdBuffer->commandBuffer, &beginInfo) != VK_SUCCESS) {
        printf("%s: vkBeginCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

GR_RESULT grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    GrvkCmdBuffer* grvkCmdBuffer = (GrvkCmdBuffer*)cmdBuffer;

    if (vki.vkEndCommandBuffer(grvkCmdBuffer->commandBuffer) != VK_SUCCESS) {
        printf("%s: vkEndCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}
