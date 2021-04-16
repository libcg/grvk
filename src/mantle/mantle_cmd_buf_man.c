#include "mantle_internal.h"

void grCmdBufferResetState(
    GrCmdBuffer* grCmdBuffer)
{
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    // Free up tracked resources
    for (unsigned i = 0; i < grCmdBuffer->descriptorPoolCount; i++) {
        VKD.vkDestroyDescriptorPool(grDevice->device, grCmdBuffer->descriptorPools[i], NULL);
    }
    for (unsigned i = 0; i < grCmdBuffer->framebufferCount; i++) {
        VKD.vkDestroyFramebuffer(grDevice->device, grCmdBuffer->framebuffers[i], NULL);
    }
    for (unsigned i = 0; i < grCmdBuffer->bufferViewCount; i++) {
        VKD.vkDestroyBufferView(grDevice->device, grCmdBuffer->bufferViews[i], NULL);
    }
    free(grCmdBuffer->descriptorPools);
    free(grCmdBuffer->framebuffers);
    free(grCmdBuffer->bufferViews);

    // Clear state
    unsigned stateOffset = OFFSET_OF(GrCmdBuffer, dirtyFlags);
    memset(&((uint8_t*)grCmdBuffer)[stateOffset], 0, sizeof(GrCmdBuffer) - stateOffset);
}

// Command Buffer Management Functions

GR_RESULT grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pCmdBuffer);
    GrDevice* grDevice = (GrDevice*)device;
    VkResult vkRes;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    // TODO check params

    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = pCreateInfo->queueType == GR_QUEUE_UNIVERSAL ?
                            grDevice->universalQueueIndex : grDevice->computeQueueIndex,
    };

    vkRes = VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &vkCommandPool);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    vkRes = VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, &vkCommandBuffer);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    GrCmdBuffer* grCmdBuffer = malloc(sizeof(GrCmdBuffer));
    *grCmdBuffer = (GrCmdBuffer) {
        .grObj = { GR_OBJ_TYPE_COMMAND_BUFFER, grDevice },
        .commandPool = vkCommandPool,
        .commandBuffer = vkCommandBuffer,
        .dirtyFlags = 0,
        .bindPoint = { { 0 }, { 0 } },
        .framebuffer = VK_NULL_HANDLE,
        .attachmentCount = 0,
        .attachments = { VK_NULL_HANDLE },
        .minExtent = { 0, 0, 0 },
        .hasActiveRenderPass = false,
        .descriptorPoolCount = 0,
        .descriptorPools = NULL,
        .framebufferCount = 0,
        .framebuffers = NULL,
        .bufferViewCount = 0,
        .bufferViews = NULL,
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

    // TODO check params

    VkCommandBufferUsageFlags vkUsageFlags = 0;
    if ((flags & GR_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT) != 0) {
        vkUsageFlags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    } else {
        // XX: it's not clear if Mantle actually allows simultaneous usage
        vkUsageFlags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
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

    grCmdBufferResetState(grCmdBuffer);

    return GR_SUCCESS;
}

GR_RESULT grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGT("%p\n", cmdBuffer);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    // TODO check params

    grCmdBufferEndRenderPass(grCmdBuffer);

    VkResult res = VKD.vkEndCommandBuffer(grCmdBuffer->commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    return GR_SUCCESS;
}

GR_RESULT grResetCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGT("%p\n", cmdBuffer);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (grCmdBuffer == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grCmdBuffer) != GR_OBJ_TYPE_COMMAND_BUFFER) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    // FIXME FIXME FIXME unfortunate but necessary workaround for Star Swarm which attempts to
    // reset a command buffer in use and free related resources...
    VKD.vkDeviceWaitIdle(grDevice->device);

    VkResult res = VKD.vkResetCommandBuffer(grCmdBuffer->commandBuffer,
                                            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    if (res != VK_SUCCESS) {
        LOGE("vkResetCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    grCmdBufferResetState(grCmdBuffer);

    return GR_SUCCESS;
}
