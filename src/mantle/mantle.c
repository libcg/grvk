#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mantle_internal.h"

#define MAX_STAGE_COUNT 5 // VS, HS, DS, GS, PS

typedef struct _STAGE {
    const GR_PIPELINE_SHADER* shader;
    const char* entryPoint;
    const VkShaderStageFlagBits flags;
} STAGE;

static GR_ALLOC_FUNCTION mAllocFun = NULL;
static GR_FREE_FUNCTION mFreeFun = NULL;
static VkInstance mVkInstance = VK_NULL_HANDLE;

static VkDescriptorSetLayout getVkDescriptorSetLayout(
    VkDevice vkDevice,
    STAGE* stage)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    uint32_t bindingCount = 0;

    // Count descriptors in this stage
    for (int i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        const GR_DESCRIPTOR_SET_MAPPING* mapping =
            &stage->shader->descriptorSetMapping[i];

        for (int j = 0; j < mapping->descriptorCount; j++) {
            const GR_DESCRIPTOR_SLOT_INFO* info = &mapping->pDescriptorInfo[j];

            if (info->slotObjectType == GR_SLOT_UNUSED) {
                continue;
            }

            if (info->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
                printf("%s: nested descriptor sets are not implemented\n", __func__);
                continue;
            }

            bindingCount++;
        }
    }

    VkDescriptorSetLayoutBinding* bindings =
        malloc(sizeof(VkDescriptorSetLayoutBinding) * bindingCount);

    // Fill out descriptor array
    uint32_t bindingIndex = 0;
    for (int i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        const GR_DESCRIPTOR_SET_MAPPING* mapping =
            &stage->shader->descriptorSetMapping[i];

        for (int j = 0; j < mapping->descriptorCount; j++) {
            const GR_DESCRIPTOR_SLOT_INFO* info = &mapping->pDescriptorInfo[j];

            if (info->slotObjectType == GR_SLOT_UNUSED ||
                info->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
                continue;
            }

            bindings[bindingIndex++] = (VkDescriptorSetLayoutBinding) {
                .binding = info->shaderEntityIndex,
                .descriptorType = getVkDescriptorType(info->slotObjectType),
                .descriptorCount = 1,
                .stageFlags = stage->flags,
                .pImmutableSamplers = NULL,
            };
        }
    }

    assert(bindingIndex == bindingCount);

    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = bindingCount,
        .pBindings = bindings,
    };

    if (vkCreateDescriptorSetLayout(vkDevice, &createInfo, NULL, &layout) != VK_SUCCESS) {
        printf("%s: vkCreateDescriptorSetLayout failed\n", __func__);
    }

    free(bindings);
    return layout;
}

static VkPipelineLayout getVkPipelineLayout(
    VkDevice vkDevice,
    STAGE* stages,
    uint32_t stageCount)
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // One descriptor set layout per stage
    VkDescriptorSetLayout* descriptorSetLayouts =
        malloc(sizeof(VkDescriptorSetLayout) * stageCount);

    uint32_t stageIndex = 0;
    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        STAGE* stage = &stages[i];

        if (stage->shader->shader == GR_NULL_HANDLE) {
            continue;
        }

        VkDescriptorSetLayout layout = getVkDescriptorSetLayout(vkDevice, stage);

        if (layout == VK_NULL_HANDLE) {
            // Bail out
            for (int j = 0; j < stageIndex; j++) {
                vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[j], NULL);
            }
            free(descriptorSetLayouts);
            return VK_NULL_HANDLE;
        }

        descriptorSetLayouts[stageIndex++] = layout;
    }

    assert(stageIndex == stageCount);

    VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = stageCount,
        .pSetLayouts = descriptorSetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    if (vkCreatePipelineLayout(vkDevice, &createInfo, NULL, &layout) != VK_SUCCESS) {
        printf("%s: vkCreatePipelineLayout failed\n", __func__);
        for (int i = 0; i < stageCount; i++) {
            vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[i], NULL);
        }
    }

    free(descriptorSetLayouts);
    return layout;
}

// Initialization and Device Functions

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    printf("%s: app \"%s\" (%08X), engine \"%s\" (%08X), api %08X\n", __func__,
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
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
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

// Shader and Pipeline Functions

GR_RESULT grCreateShader(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader)
{
    VkDevice vkDevice = (VkDevice)device;
    VkShaderModule vkShaderModule = VK_NULL_HANDLE;

    // TODO support AMDIL shaders
    if ((pCreateInfo->flags & GR_SHADER_CREATE_SPIRV) == 0) {
        printf("%s: AMDIL shaders not supported\n", __func__);
        return GR_ERROR_BAD_SHADER_CODE;
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = pCreateInfo->codeSize,
        .pCode = pCreateInfo->pCode,
    };

    if (vkCreateShaderModule(vkDevice, &createInfo, NULL, &vkShaderModule)) {
        printf("%s: vkCreateShaderModule failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    *pShader = (GR_SHADER)vkShaderModule;
    return GR_SUCCESS;
}

GR_RESULT grCreateGraphicsPipeline(
    GR_DEVICE device,
    const GR_GRAPHICS_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    VkDevice vkDevice = (VkDevice)device;

    // Ignored parameters:
    // - iaState.disableVertexReuse (hint)
    // - tessState.optimalTessFactor (hint)
    // - cbState.format (defined at draw time)
    // - dbState.format (defined at draw time)

    // FIXME entry points are guessed
    STAGE stages[MAX_STAGE_COUNT] = {
        { &pCreateInfo->vs, "VShader", VK_SHADER_STAGE_VERTEX_BIT },
        { &pCreateInfo->hs, "HShader", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { &pCreateInfo->ds, "DShader", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
        { &pCreateInfo->gs, "GShader", VK_SHADER_STAGE_GEOMETRY_BIT },
        { &pCreateInfo->ps, "PShader", VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    // Figure out how many stages are used before we allocate the info array
    uint32_t stageCount = 0;
    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        if (stages[i].shader->shader != GR_NULL_HANDLE) {
            stageCount++;
        }
    }

    VkPipelineShaderStageCreateInfo* shaderStageCreateInfo =
        malloc(sizeof(VkPipelineShaderStageCreateInfo) * stageCount);

    // Fill in the info array
    uint32_t stageIndex = 0;
    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        STAGE* stage = &stages[i];

        if (stage->shader->shader == GR_NULL_HANDLE) {
            continue;
        }

        if (stage->shader->linkConstBufferCount > 0) {
            // TODO implement
            printf("%s: link-time constant buffers are not implemented\n", __func__);
        }

        if (stage->shader->dynamicMemoryViewMapping.slotObjectType != GR_SLOT_UNUSED) {
            // TODO implement
            printf("%s: dynamic memory view mapping is not implemented\n", __func__);
        }

        shaderStageCreateInfo[stageIndex++] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = stage->flags,
            .module = (VkShaderModule)stage->shader->shader,
            .pName = stage->entryPoint,
            .pSpecializationInfo = NULL,
        };
    }

    assert(stageIndex == stageCount);

    VkPipelineVertexInputStateCreateInfo* vertexInputStateCreateInfo =
        malloc(sizeof(VkPipelineVertexInputStateCreateInfo));

    *vertexInputStateCreateInfo = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
    };

    VkPipelineInputAssemblyStateCreateInfo* inputAssemblyStateCreateInfo =
        malloc(sizeof(VkPipelineInputAssemblyStateCreateInfo));

    *inputAssemblyStateCreateInfo = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = getVkPrimitiveTopology(pCreateInfo->iaState.topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineTessellationStateCreateInfo* tessellationStateCreateInfo = NULL;

    if (pCreateInfo->hs.shader != GR_NULL_HANDLE && pCreateInfo->ds.shader != GR_NULL_HANDLE) {
        tessellationStateCreateInfo = malloc(sizeof(VkPipelineTessellationStateCreateInfo));

        *tessellationStateCreateInfo = (VkPipelineTessellationStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .patchControlPoints = pCreateInfo->tessState.patchControlPoints,
        };
    }

    VkPipelineRasterizationDepthClipStateCreateInfoEXT* depthClipStateCreateInfo =
        malloc(sizeof(VkPipelineRasterizationDepthClipStateCreateInfoEXT));

    *depthClipStateCreateInfo = (VkPipelineRasterizationDepthClipStateCreateInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .depthClipEnable = pCreateInfo->rsState.depthClipEnable,
    };

    VkPipelineColorBlendAttachmentState* attachments =
        malloc(sizeof(VkPipelineColorBlendAttachmentState) * GR_MAX_COLOR_TARGETS);

    // TODO implement
    if (pCreateInfo->cbState.alphaToCoverageEnable) {
        printf("%s: alpha-to-coverage is not implemented\n", __func__);
    }

    if (pCreateInfo->cbState.dualSourceBlendEnable) {
        // Nothing to do, set through VkBlendFactor
    }

    // TODO implement
    if (pCreateInfo->cbState.logicOp != GR_LOGIC_OP_COPY) {
        printf("%s: unsupported logic operation %X\n", __func__, pCreateInfo->cbState.logicOp);
    }

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &pCreateInfo->cbState.target[i];

        attachments[i] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable = target->blendEnable,
            .srcColorBlendFactor = 0, // Filled in grCreateColorBlendState
            .dstColorBlendFactor = 0, // Filled in grCreateColorBlendState
            .colorBlendOp = 0, // Filled in grCreateColorBlendState
            .srcAlphaBlendFactor = 0, // Filled in grCreateColorBlendState
            .dstAlphaBlendFactor = 0, // Filled in grCreateColorBlendState
            .alphaBlendOp = 0, // Filled in grCreateColorBlendState
            .colorWriteMask = getVkColorComponentFlags(target->channelWriteMask),
        };
    }

    VkPipelineLayout layout = getVkPipelineLayout(vkDevice, stages, stageCount);

    if (layout == VK_NULL_HANDLE) {
        free(shaderStageCreateInfo);
        free(vertexInputStateCreateInfo);
        free(inputAssemblyStateCreateInfo);
        free(tessellationStateCreateInfo);
        free(depthClipStateCreateInfo);
        free(attachments);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    // Pipeline will be created at bind time because we're missing some state
    VkGraphicsPipelineCreateInfo* pipelineCreateInfo = malloc(sizeof(VkGraphicsPipelineCreateInfo));

    *pipelineCreateInfo = (VkGraphicsPipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                 VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stageCount = stageCount,
        .pStages = shaderStageCreateInfo,
        .pVertexInputState = vertexInputStateCreateInfo,
        .pInputAssemblyState = inputAssemblyStateCreateInfo,
        .pTessellationState = tessellationStateCreateInfo,
        .pViewportState = NULL, // Filled in at bind time
        .pRasterizationState = // Combined with raster state at bind time
            (VkPipelineRasterizationStateCreateInfo*)depthClipStateCreateInfo,
        .pMultisampleState = NULL, // Filled in at bind time
        .pDepthStencilState = NULL, // Filled in at bind time
        .pColorBlendState = // Combined with color blend state at bind time
            (VkPipelineColorBlendStateCreateInfo*)attachments,
        .pDynamicState = NULL, // TODO implement VK_EXT_extended_dynamic_state
        .layout = layout,
        .renderPass = VK_NULL_HANDLE, // Filled in later
        .subpass = 0, // Filled in later
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    *pPipeline = (GR_PIPELINE)pipelineCreateInfo;
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
        .depthClampEnable = VK_TRUE, // depthClipEnable is set from grCreateGraphicsPipeline
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
                .colorWriteMask = 0, // Set from grCreateGraphicsPipeline
            };
        } else {
            attachments[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = 0, // Ignored
                .dstColorBlendFactor = 0, // Ignored
                .colorBlendOp = 0, // Ignored
                .srcAlphaBlendFactor = 0, // Ignored
                .dstAlphaBlendFactor = 0, // Ignored
                .alphaBlendOp = 0, // Ignored
                .colorWriteMask = 0, // Set from grCreateGraphicsPipeline
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
        .logicOp = 0, // Ignored
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
        .front = {
            .failOp = getVkStencilOp(pCreateInfo->front.stencilFailOp), // vkCmdSetStencilOpEXT
            .passOp = getVkStencilOp(pCreateInfo->front.stencilPassOp), // ^
            .depthFailOp = getVkStencilOp(pCreateInfo->front.stencilDepthFailOp), // ^
            .compareOp = getVkCompareOp(pCreateInfo->front.stencilFunc), // ^
            .compareMask = pCreateInfo->stencilReadMask, // vkCmdSetStencilCompareMask
            .writeMask = pCreateInfo->stencilWriteMask, // vkCmdSetStencilWriteMask
            .reference = pCreateInfo->front.stencilRef, // vkCmdSetStencilReference
        },
        .back = {
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

        vkCmdPipelineBarrier(vkCommandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}
