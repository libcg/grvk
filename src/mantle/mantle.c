#include <stdio.h>
#include <stdlib.h>
#include "mantle_internal.h"

static GR_ALLOC_FUNCTION mAllocFun = NULL;
static GR_FREE_FUNCTION mFreeFun = NULL;
static VkInstance mVkInstance = VK_NULL_HANDLE;

// Initialization and Device Functions

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    printf("%s: app %s (%08X), engine %s (%08X), api %08X\n", __func__,
           pAppInfo->pAppName, pAppInfo->appVersion,
           pAppInfo->pEngineName, pAppInfo->engineVersion,
           pAppInfo->apiVersion);

    if (pAllocCb == NULL) {
        mAllocFun = grvkAlloc;
        mFreeFun = grvkFree;
    } else {
        mAllocFun = pAllocCb->pfnAlloc;
        mFreeFun = pAllocCb->pfnFree;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = pAppInfo->pAppName,
        .applicationVersion = pAppInfo->appVersion,
        .pEngineName = pAppInfo->pEngineName,
        .engineVersion = pAppInfo->engineVersion,
        .apiVersion = VK_API_VERSION_1_2,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
    };

    if (vkCreateInstance(&createInfo, NULL, &mVkInstance) != VK_SUCCESS) {
        printf("%s: vkCreateInstance failed\n", __func__);
        return GR_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > GR_MAX_PHYSICAL_GPUS) {
        physicalDeviceCount = GR_MAX_PHYSICAL_GPUS;
    }

    VkPhysicalDevice physicalDevices[GR_MAX_PHYSICAL_GPUS];
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, physicalDevices);

    *pGpuCount = physicalDeviceCount;
    for (int i = 0; i < *pGpuCount; i++) {
        gpus[i] = (GR_PHYSICAL_GPU)physicalDevices[i];
    }

    return GR_SUCCESS;
}

GR_RESULT grCreateDevice(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice)
{
    GR_RESULT res = GR_SUCCESS;
    VkPhysicalDevice physicalDevice = (VkPhysicalDevice)gpu;
    uint32_t universalQueueIndex = -1;
    uint32_t universalQueueCount = 0;
    uint32_t computeQueueIndex = -1;
    uint32_t computeQueueCount = 0;

    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, NULL);

    VkQueueFamilyProperties* queueFamilyProperties =
        malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount,
                                             queueFamilyProperties);

    for (int i = 0; i < queueFamilyPropertyCount; i++) {
        const VkQueueFamilyProperties* queueFamilyProperty = &queueFamilyProperties[i];

        if ((queueFamilyProperty->queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            universalQueueIndex = i;
            universalQueueCount = queueFamilyProperty->queueCount;
        } else if ((queueFamilyProperty->queueFlags & VK_QUEUE_COMPUTE_BIT) ==
                   VK_QUEUE_COMPUTE_BIT) {
            computeQueueIndex = i;
            computeQueueCount = queueFamilyProperty->queueCount;
        }
    }

    VkDeviceQueueCreateInfo* queueCreateInfos =
        malloc(sizeof(VkDeviceQueueCreateInfo) * pCreateInfo->queueRecordCount);
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        const GR_DEVICE_QUEUE_CREATE_INFO* requestedQueue = &pCreateInfo->pRequestedQueues[i];

        float* queuePriorities = malloc(sizeof(float) * requestedQueue->queueCount);

        for (int j = 0; j < requestedQueue->queueCount; j++) {
            queuePriorities[j] = 1.0f; // Max priority
        }

        if ((requestedQueue->queueType == GR_QUEUE_UNIVERSAL &&
             requestedQueue->queueCount != universalQueueCount) ||
            (requestedQueue->queueType == GR_QUEUE_COMPUTE &&
             requestedQueue->queueCount != computeQueueCount)) {
            printf("%s: can't find requested queue type %X\n", __func__, requestedQueue->queueType);
            res = GR_ERROR_INVALID_VALUE;
            // Bail after the loop to properly release memory
        }

        queueCreateInfos[i] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = requestedQueue->queueType == GR_QUEUE_UNIVERSAL ?
                                universalQueueIndex : computeQueueIndex,
            .queueCount = requestedQueue->queueCount,
            .pQueuePriorities = queuePriorities,
        };
    }

    if (res != GR_SUCCESS) {
        goto bail;
    }

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = pCreateInfo->queueRecordCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .pEnabledFeatures = NULL,
    };

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &createInfo, NULL, &device) != VK_SUCCESS) {
        printf("%s: vkCreateDevice failed\n", __func__);
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    *pDevice = (GR_DEVICE)device;

bail:
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        free((void*)queueCreateInfos[i].pQueuePriorities);
    }
    free(queueCreateInfos);

    return res;
}

// Queue Functions

GR_RESULT grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    VkDevice vkDevice = (VkDevice)device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    vkGetDeviceQueue(vkDevice, getVkQueueFamilyIndex(queueType), queueId, &vkQueue);

    *pQueue = (GR_QUEUE)vkQueue;
    return GR_SUCCESS;
}

GR_RESULT grQueueSubmit(
    GR_QUEUE queue,
    GR_UINT cmdBufferCount,
    const GR_CMD_BUFFER* pCmdBuffers,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs,
    GR_FENCE fence)
{
    VkQueue vkQueue = (VkQueue)queue;
    VkFence vkFence = (VkFence)fence;

    VkCommandBuffer* vkCommandBuffers = malloc(sizeof(VkCommandBuffer) * cmdBufferCount);
    for (int i = 0; i < cmdBufferCount; i++) {
        vkCommandBuffers[i] = (VkCommandBuffer)pCmdBuffers[i];
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = cmdBufferCount,
        .pCommandBuffers = vkCommandBuffers,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    if (vkQueueSubmit(vkQueue, 1, &submitInfo, vkFence) != VK_SUCCESS) {
        printf("%s: vkQueueSubmit failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    free(vkCommandBuffers);

    return GR_SUCCESS;
}

// Image View Functions

GR_RESULT grCreateColorTargetView(
    GR_DEVICE device,
    const GR_COLOR_TARGET_VIEW_CREATE_INFO* pCreateInfo,
    GR_COLOR_TARGET_VIEW* pView)
{
    VkDevice vkDevice = (VkDevice)device;
    VkImageView vkImageView = VK_NULL_HANDLE;

    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = (VkImage)pCreateInfo->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = getVkFormat(pCreateInfo->format),
        .components = (VkComponentMapping) {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = (VkImageSubresourceRange) {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = pCreateInfo->mipLevel,
            .levelCount = 1,
            .baseArrayLayer = pCreateInfo->baseArraySlice,
            .layerCount = pCreateInfo->arraySize == GR_LAST_MIP_OR_SLICE ?
                          VK_REMAINING_ARRAY_LAYERS : pCreateInfo->arraySize,
        }
    };

    if (vkCreateImageView(vkDevice, &createInfo, NULL, &vkImageView) != VK_SUCCESS) {
        printf("%s: vkCreateImageView failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    *pView = (GR_COLOR_TARGET_VIEW)vkImageView;
    return GR_SUCCESS;
}

// State Object Functions

GR_RESULT grCreateViewportState(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState)
{
    uint32_t scissorCount = pCreateInfo->scissorEnable ? pCreateInfo->viewportCount : 0;

    VkViewport *vkViewports = malloc(sizeof(VkViewport) * pCreateInfo->viewportCount);
    for (int i = 0; i < pCreateInfo->viewportCount; i++) {
        vkViewports[i] = (VkViewport) {
            .x = pCreateInfo->viewports[i].originX,
            .y = pCreateInfo->viewports[i].originY,
            .width = pCreateInfo->viewports[i].width,
            .height = pCreateInfo->viewports[i].height,
            .minDepth = pCreateInfo->viewports[i].minDepth,
            .maxDepth = pCreateInfo->viewports[i].maxDepth,
        };
    }

    VkRect2D *vkScissors = malloc(sizeof(VkViewport) * scissorCount);
    for (int i = 0; i < scissorCount; i++) {
        vkScissors[i] = (VkRect2D) {
            .offset = {
                .x = pCreateInfo->scissors[i].offset.x,
                .y = pCreateInfo->scissors[i].offset.y,
            },
            .extent = {
                .width = pCreateInfo->scissors[i].extent.width,
                .height = pCreateInfo->scissors[i].extent.height,
            },
        };
    }

    VkPipelineViewportStateCreateInfo* viewportStateCreateInfo =
        malloc(sizeof(VkPipelineViewportStateCreateInfo));

    // Will be set dynamically with vkCmdSet{Viewport/Scissor}WithCountEXT
    *viewportStateCreateInfo = (VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = pCreateInfo->viewportCount,
        .pViewports = vkViewports,
        .scissorCount = scissorCount,
        .pScissors = vkScissors,
    };

    *pState = (GR_VIEWPORT_STATE_OBJECT)viewportStateCreateInfo;
    return GR_SUCCESS;
}

GR_RESULT grCreateRasterState(
    GR_DEVICE device,
    const GR_RASTER_STATE_CREATE_INFO* pCreateInfo,
    GR_RASTER_STATE_OBJECT* pState)
{
    VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo =
        malloc(sizeof(VkPipelineRasterizationStateCreateInfo));

    *rasterizationStateCreateInfo = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = getVkPolygonMode(pCreateInfo->fillMode), // TODO no dynamic state
        .cullMode = getVkCullModeFlags(pCreateInfo->cullMode), // vkCmdSetCullModeEXT
        .frontFace = getVkFrontFace(pCreateInfo->frontFace), // vkCmdSetFrontFaceEXT
        .depthBiasEnable = VK_TRUE,
        .depthBiasConstantFactor = pCreateInfo->depthBias, // vkCmdSetDepthBias
        .depthBiasClamp = pCreateInfo->depthBiasClamp, // vkCmdSetDepthBias
        .depthBiasSlopeFactor = pCreateInfo->slopeScaledDepthBias, // vkCmdSetDepthBias
        .lineWidth = 1.f,
    };

    *pState = (GR_RASTER_STATE_OBJECT)rasterizationStateCreateInfo;
    return GR_SUCCESS;
}

GR_RESULT grCreateColorBlendState(
    GR_DEVICE device,
    const GR_COLOR_BLEND_STATE_CREATE_INFO* pCreateInfo,
    GR_COLOR_BLEND_STATE_OBJECT* pState)
{
    VkPipelineColorBlendAttachmentState* attachments =
        malloc(sizeof(VkPipelineColorBlendAttachmentState) * GR_MAX_COLOR_TARGETS);
    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        if (pCreateInfo->target[i].blendEnable) {
            attachments[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = getVkBlendFactor(pCreateInfo->target[i].srcBlendColor),
                .dstColorBlendFactor = getVkBlendFactor(pCreateInfo->target[i].destBlendColor),
                .colorBlendOp = getVkBlendOp(pCreateInfo->target[i].blendFuncColor),
                .srcAlphaBlendFactor = getVkBlendFactor(pCreateInfo->target[i].srcBlendAlpha),
                .dstAlphaBlendFactor = getVkBlendFactor(pCreateInfo->target[i].destBlendAlpha),
                .alphaBlendOp = getVkBlendOp(pCreateInfo->target[i].blendFuncAlpha),
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
        } else {
            attachments[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = 0,
                .dstColorBlendFactor = 0,
                .colorBlendOp = 0,
                .srcAlphaBlendFactor = 0,
                .dstAlphaBlendFactor = 0,
                .alphaBlendOp = 0,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
        }
    }

    VkPipelineColorBlendStateCreateInfo* colorBlendStateCreateInfo =
        malloc(sizeof(VkPipelineColorBlendStateCreateInfo));

    *colorBlendStateCreateInfo = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_CLEAR,
        .attachmentCount = GR_MAX_COLOR_TARGETS,
        .pAttachments = attachments, // TODO no dynamic state
        .blendConstants = { // vkCmdSetBlendConstants
            pCreateInfo->blendConst[0], pCreateInfo->blendConst[1],
            pCreateInfo->blendConst[2], pCreateInfo->blendConst[3],
        },
    };

    *pState = (GR_COLOR_BLEND_STATE_OBJECT)colorBlendStateCreateInfo;
    return GR_SUCCESS;
}

GR_RESULT grCreateDepthStencilState(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_STATE_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_STATE_OBJECT* pState)
{
    VkPipelineDepthStencilStateCreateInfo* depthStencilStateCreateInfo =
        malloc(sizeof(VkPipelineDepthStencilStateCreateInfo));

    *depthStencilStateCreateInfo = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = pCreateInfo->depthEnable, // vkCmdSetDepthTestEnableEXT
        .depthWriteEnable = pCreateInfo->depthWriteEnable, // vkCmdSetDepthWriteEnableEXT
        .depthCompareOp = getVkCompareOp(pCreateInfo->depthFunc), // vkCmdSetDepthCompareOpEXT
        .depthBoundsTestEnable = pCreateInfo->depthBoundsEnable, // vkCmdSetDepthBoundsTestEnableEXT
        .stencilTestEnable = pCreateInfo->stencilEnable, // vkCmdSetStencilTestEnableEXT
        .front = (VkStencilOpState) {
            .failOp = getVkStencilOp(pCreateInfo->front.stencilFailOp), // vkCmdSetStencilOpEXT
            .passOp = getVkStencilOp(pCreateInfo->front.stencilPassOp), // ^
            .depthFailOp = getVkStencilOp(pCreateInfo->front.stencilDepthFailOp), // ^
            .compareOp = getVkCompareOp(pCreateInfo->front.stencilFunc), // ^
            .compareMask = pCreateInfo->stencilReadMask, // vkCmdSetStencilCompareMask
            .writeMask = pCreateInfo->stencilWriteMask, // vkCmdSetStencilWriteMask
            .reference = pCreateInfo->front.stencilRef, // vkCmdSetStencilReference
        },
        .back = (VkStencilOpState) {
            .failOp = getVkStencilOp(pCreateInfo->back.stencilFailOp), // vkCmdSetStencilOpEXT
            .passOp = getVkStencilOp(pCreateInfo->back.stencilPassOp), // ^
            .depthFailOp = getVkStencilOp(pCreateInfo->back.stencilDepthFailOp), // ^
            .compareOp = getVkCompareOp(pCreateInfo->back.stencilFunc), // ^
            .compareMask = pCreateInfo->stencilReadMask, // vkCmdSetStencilCompareMask
            .writeMask = pCreateInfo->stencilWriteMask, // vkCmdSetStencilWriteMask
            .reference = pCreateInfo->back.stencilRef, // vkCmdSetStencilReference
        },
        .minDepthBounds = pCreateInfo->minDepth, // vkCmdSetDepthBounds
        .maxDepthBounds = pCreateInfo->maxDepth, // ^
    };

    *pState = (GR_DEPTH_STENCIL_STATE_OBJECT)depthStencilStateCreateInfo;
    return GR_SUCCESS;
}

GR_RESULT grCreateMsaaState(
    GR_DEVICE device,
    const GR_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    VkPipelineMultisampleStateCreateInfo* msaaStateCreateInfo =
        malloc(sizeof(VkPipelineMultisampleStateCreateInfo));

    // TODO no dynamic state
    *msaaStateCreateInfo = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = getVkSampleCountFlagBits(pCreateInfo->samples),
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = &pCreateInfo->sampleMask,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    *pState = (GR_MSAA_STATE_OBJECT)msaaStateCreateInfo;
    return GR_SUCCESS;
}

// Command Buffer Management Functions

GR_RESULT grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    VkDevice vkDevice = (VkDevice)device;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    // FIXME we shouldn't create one command pool per command buffer :)
    VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = getVkQueueFamilyIndex(pCreateInfo->queueType),
    };

    if (vkCreateCommandPool(vkDevice, &poolCreateInfo, NULL, &vkCommandPool) != VK_SUCCESS) {
        printf("%s: vkCreateCommandPool failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(vkDevice, &allocateInfo, &vkCommandBuffer) != VK_SUCCESS) {
        printf("%s: vkAllocateCommandBuffers failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    *pCmdBuffer = (GR_QUEUE)vkCommandBuffer;
    return GR_SUCCESS;
}

GR_RESULT grBeginCommandBuffer(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;
    VkCommandBufferUsageFlags vkUsageFlags = 0;

    if ((flags & GR_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT) != 0) {
        vkUsageFlags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = vkUsageFlags,
        .pInheritanceInfo = NULL,
    };

    if (vkBeginCommandBuffer(vkCommandBuffer, &beginInfo) != VK_SUCCESS) {
        printf("%s: vkBeginCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

GR_RESULT grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;

    if (vkEndCommandBuffer(vkCommandBuffer) != VK_SUCCESS) {
        printf("%s: vkEndCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

// Command Buffer Building Functions

GR_VOID grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        const GR_IMAGE_SUBRESOURCE_RANGE* range = &stateTransition->subresourceRange;

        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlags(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlags(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = (VkImage)stateTransition->image,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask = getVkImageAspectFlags(range->aspect),
                .baseMipLevel = range->baseMipLevel,
                .levelCount = range->mipLevels == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_MIP_LEVELS : range->mipLevels,
                .baseArrayLayer = range->baseArraySlice,
                .layerCount = range->arraySize == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_ARRAY_LAYERS : range->arraySize,
            }
        };

        vkCmdPipelineBarrier(vkCommandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}
