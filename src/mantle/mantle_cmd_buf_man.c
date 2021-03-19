#include "mantle_internal.h"

// Command Buffer Management Functions

GR_RESULT grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pCmdBuffer);
    GrDevice* grDevice = (GrDevice*)device;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    if (pCreateInfo->queueType == GR_QUEUE_UNIVERSAL) {
        vkCommandPool = grDevice->universalCommandPool;
    } else if (pCreateInfo->queueType == GR_QUEUE_COMPUTE) {
        vkCommandPool = grDevice->computeCommandPool;
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

    VkResult res = VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, &vkCommandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed (%d)\n", res);
        return getGrResult(res);
    }

    GrCmdBuffer* grCmdBuffer = malloc(sizeof(GrCmdBuffer));
    *grCmdBuffer = (GrCmdBuffer) {
        .grObj = { GR_OBJ_TYPE_COMMAND_BUFFER, grDevice },
        .commandBuffer = vkCommandBuffer,
        .grPipeline = NULL,
        .grDescriptorSet = NULL,
        .attachmentCount = 0,
        .attachments = { VK_NULL_HANDLE },
        .minExtent2D = { 0, 0 },
        .minLayerCount = 0,
        .hasActiveRenderPass = false,
        .isDirty = false,
    };

    *pCmdBuffer = (GR_CMD_BUFFER)grCmdBuffer;
    return GR_SUCCESS;
}

GR_RESULT grBeginCommandBuffer(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags)
{
    LOGT("%p 0x%X\n", cmdBuffer, flags);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
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

    VkResult res = VKD.vkBeginCommandBuffer(grCmdBuffer->commandBuffer, &beginInfo);
    if (res != VK_SUCCESS) {
        LOGE("vkBeginCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    return GR_SUCCESS;
}

GR_RESULT grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGT("%p\n", cmdBuffer);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    if (grCmdBuffer->hasActiveRenderPass) {
        VKD.vkCmdEndRenderPass(grCmdBuffer->commandBuffer);
    }

    VkResult res = VKD.vkEndCommandBuffer(grCmdBuffer->commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    return GR_SUCCESS;
}
