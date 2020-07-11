#include "mantle_internal.h"

#define MAX_STAGE_COUNT 5 // VS, HS, DS, GS, PS

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const char* entryPoint;
    const VkShaderStageFlagBits flags;
} Stage;

static VkDescriptorSetLayout getVkDescriptorSetLayout(
    VkDevice vkDevice,
    Stage* stage)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayoutBinding* bindings = NULL;
    uint32_t bindingCount = 0;

    if (stage->shader->shader != GR_NULL_HANDLE) {
        for (int i = 1; i < GR_MAX_DESCRIPTOR_SETS; i++) {
            const GR_DESCRIPTOR_SET_MAPPING* mapping = &stage->shader->descriptorSetMapping[i];

            if (mapping->descriptorCount > 0) {
                printf("%s: multiple descriptor sets per stage is not supported\n", __func__);
                break;
            }
        }

        // TODO handle all descriptor sets
        const GR_DESCRIPTOR_SET_MAPPING* mapping = &stage->shader->descriptorSetMapping[0];

        bindings = malloc(sizeof(VkDescriptorSetLayoutBinding) * mapping->descriptorCount);
        bindingCount = mapping->descriptorCount;

        for (int i = 0; i < mapping->descriptorCount; i++) {
            const GR_DESCRIPTOR_SLOT_INFO* info = &mapping->pDescriptorInfo[i];

            if (info->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
                printf("%s: nested descriptor sets are not implemented\n", __func__);
                continue;
            }

            if (info->slotObjectType == GR_SLOT_UNUSED) {
                bindings[i] = (VkDescriptorSetLayoutBinding) {
                    .binding = 0,
                    .descriptorType = getVkDescriptorType(info->slotObjectType),
                    .descriptorCount = 0,
                    .stageFlags = 0,
                    .pImmutableSamplers = NULL,
                };
            } else {
                bindings[i] = (VkDescriptorSetLayoutBinding) {
                    .binding = info->shaderEntityIndex,
                    .descriptorType = getVkDescriptorType(info->slotObjectType),
                    .descriptorCount = 1,
                    .stageFlags = stage->flags,
                    .pImmutableSamplers = NULL,
                };
            }
        }
    }

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
    Stage* stages)
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // One descriptor set layout per stage, with an empty layout for each unused stage
    VkDescriptorSetLayout descriptorSetLayouts[MAX_STAGE_COUNT];

    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        Stage* stage = &stages[i];

        VkDescriptorSetLayout layout = getVkDescriptorSetLayout(vkDevice, stage);

        if (layout == VK_NULL_HANDLE) {
            // Bail out
            for (int j = 0; j < i; j++) {
                vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[j], NULL);
            }
            return VK_NULL_HANDLE;
        }

        descriptorSetLayouts[i] = layout;
    }

    VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = MAX_STAGE_COUNT,
        .pSetLayouts = descriptorSetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    if (vkCreatePipelineLayout(vkDevice, &createInfo, NULL, &layout) != VK_SUCCESS) {
        printf("%s: vkCreatePipelineLayout failed\n", __func__);
        for (int i = 0; i < MAX_STAGE_COUNT; i++) {
            vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[i], NULL);
        }
    }

    return layout;
}

// Shader and Pipeline Functions

GR_RESULT grCreateShader(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader)
{
    VkDevice vkDevice = ((GrvkDevice*)device)->device;
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

    GrvkShader* grvkShader = malloc(sizeof(GrvkShader));
    *grvkShader = (GrvkShader) {
        .sType = GRVK_STRUCT_TYPE_SHADER,
        .shaderModule = vkShaderModule,
    };

    *pShader = (GR_SHADER)grvkShader;
    return GR_SUCCESS;
}

GR_RESULT grCreateGraphicsPipeline(
    GR_DEVICE device,
    const GR_GRAPHICS_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    VkDevice vkDevice = ((GrvkDevice*)device)->device;

    // Ignored parameters:
    // - iaState.disableVertexReuse (hint)
    // - tessState.optimalTessFactor (hint)
    // - cbState.format (defined at draw time)
    // - dbState.format (defined at draw time)

    // FIXME entry points are guessed
    Stage stages[MAX_STAGE_COUNT] = {
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
        Stage* stage = &stages[i];

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

    VkPipelineLayout layout = getVkPipelineLayout(vkDevice, stages);

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

    GrvkPipeline* grvkPipeline = malloc(sizeof(GrvkPipeline));
    *grvkPipeline = (GrvkPipeline) {
        .sType = GRVK_STRUCT_TYPE_PIPELINE,
        .graphicsPipelineCreateInfo = pipelineCreateInfo,
    };

    *pPipeline = (GR_PIPELINE)grvkPipeline;
    return GR_SUCCESS;
}
