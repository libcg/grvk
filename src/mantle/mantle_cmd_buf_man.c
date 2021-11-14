#include "mantle_internal.h"

void grCmdBufferResetState(
    GrCmdBuffer* grCmdBuffer,
    bool preserveDescriptorPools)
{
    // Release tracked resources
    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);
    unsigned descriptorPoolCount = grCmdBuffer->descriptorPoolCount;
    VkDescriptorPool* descriptorPools = grCmdBuffer->descriptorPools;

    if (preserveDescriptorPools) {
        for (unsigned i = 0; i < grCmdBuffer->descriptorPoolCount; i++) {
            if (i > grCmdBuffer->descriptorPoolIndex) {
                break;
            }

            VKD.vkResetDescriptorPool(grDevice->device, grCmdBuffer->descriptorPools[i], 0);
        }
    } else {
        for (unsigned i = 0; i < grCmdBuffer->descriptorPoolCount; i++) {
            VKD.vkDestroyDescriptorPool(grDevice->device, grCmdBuffer->descriptorPools[i], NULL);
        }
        free(grCmdBuffer->descriptorPools);
    }

    // Clear state
    unsigned stateOffset = OFFSET_OF(GrCmdBuffer, isBuilding);
    memset(&((uint8_t*)grCmdBuffer)[stateOffset], 0, sizeof(GrCmdBuffer) - stateOffset);

    if (preserveDescriptorPools) {
        grCmdBuffer->descriptorPoolCount = descriptorPoolCount;
        grCmdBuffer->descriptorPools = descriptorPools;
    }
}

// Command Buffer Management Functions

GR_RESULT GR_STDCALL grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pCmdBuffer);
    GrDevice* grDevice = (GrDevice*)device;
    VkResult vkRes;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pCmdBuffer == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pCreateInfo->flags != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (0) {
        // TODO check queue type
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = grDeviceGetQueueFamilyIndex(grDevice, pCreateInfo->queueType),
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

    VkBuffer atomicCounterBuffer;
    if (pCreateInfo->queueType == GR_QUEUE_UNIVERSAL) {
        atomicCounterBuffer = grDevice->universalAtomicCounterBuffer;
    } else if (pCreateInfo->queueType == GR_QUEUE_COMPUTE) {
        atomicCounterBuffer = grDevice->computeAtomicCounterBuffer;
    } else {
        atomicCounterBuffer = VK_NULL_HANDLE;
    }

    const DescriptorSetSlot atomicCounterSlot = {
        .type = SLOT_TYPE_BUFFER,
        .buffer = {
            .bufferView = VK_NULL_HANDLE,
            .bufferInfo = {
                .buffer = atomicCounterBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            .stride = 0, // Ignored
        },
    };

    GrCmdBuffer* grCmdBuffer = malloc(sizeof(GrCmdBuffer));
    *grCmdBuffer = (GrCmdBuffer) {
        .grObj = { GR_OBJ_TYPE_COMMAND_BUFFER, grDevice },
        .commandPool = vkCommandPool,
        .commandBuffer = vkCommandBuffer,
        .atomicCounterSlot = atomicCounterSlot,
        .isBuilding = false,
        .bindPoints = { { 0 }, { 0 } },
        .grViewportState = NULL,
        .grRasterState = NULL,
        .grMsaaState = NULL,
        .grDepthStencilState = NULL,
        .grColorBlendState = NULL,
        .colorAttachmentCount = 0,
        .colorAttachments = { { 0 } },
        .colorFormats = { 0 },
        .depthAttachment = { 0 },
        .stencilAttachment = { 0 },
        .depthStencilFormat = 0,
        .minExtent = { 0, 0, 0 },
        .isRendering = false,
        .descriptorPoolIndex = 0,
        .descriptorPoolCount = 0,
        .descriptorPools = NULL,
        .submitFence = NULL,
    };

    *pCmdBuffer = (GR_CMD_BUFFER)grCmdBuffer;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grBeginCommandBuffer(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags)
{
    LOGT("%p 0x%X\n", cmdBuffer, flags);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (grCmdBuffer == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grCmdBuffer) != GR_OBJ_TYPE_COMMAND_BUFFER) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (0) {
        // TODO check flags
        return GR_ERROR_INVALID_FLAGS;
    } else if (grCmdBuffer->isBuilding) {
        return GR_ERROR_INCOMPLETE_COMMAND_BUFFER;
    }

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

    grCmdBufferResetState(grCmdBuffer, true);
    grCmdBuffer->isBuilding = true;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGT("%p\n", cmdBuffer);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;

    if (grCmdBuffer == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grCmdBuffer) != GR_OBJ_TYPE_COMMAND_BUFFER) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (!grCmdBuffer->isBuilding) {
        return GR_ERROR_INCOMPLETE_COMMAND_BUFFER;
    }

    GrDevice* grDevice = GET_OBJ_DEVICE(grCmdBuffer);

    grCmdBufferEndRenderPass(grCmdBuffer);

    VkResult res = VKD.vkEndCommandBuffer(grCmdBuffer->commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    grCmdBuffer->isBuilding = false;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grResetCommandBuffer(
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

    if (quirkHas(QUIRK_INVALID_CMD_BUFFER_RESET) && grCmdBuffer->submitFence != NULL) {
        // Game attempts to reset a command buffer in use...
        // Wait for the submit fence ourselves to work around that issue.
        grWaitForFences((GR_DEVICE)grDevice, 1, (GR_FENCE*)&grCmdBuffer->submitFence, true, 10.0f);
    }

    VkResult res = VKD.vkResetCommandBuffer(grCmdBuffer->commandBuffer,
                                            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    if (res != VK_SUCCESS) {
        LOGE("vkResetCommandBuffer failed (%d)\n", res);
        return getGrResult(res);
    }

    grCmdBufferResetState(grCmdBuffer, false);

    return GR_SUCCESS;
}
