#include "mantle_internal.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const char* entryPoint;
    const VkShaderStageFlagBits flags;
} Stage;

static VkDescriptorSetLayout getVkDescriptorSetLayout(
    const VkDevice vkDevice,
    const Stage* stage)
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

        // Find maximum entity index to generate unique binding numbers for unused slots
        uint32_t maxEntityIndex = 0;
        for (int i = 0; i < mapping->descriptorCount; i++) {
            const GR_DESCRIPTOR_SLOT_INFO* info = &mapping->pDescriptorInfo[i];

            if (info->shaderEntityIndex > maxEntityIndex) {
                maxEntityIndex = info->shaderEntityIndex;
            }
        }

        bindings = malloc(sizeof(VkDescriptorSetLayoutBinding) * mapping->descriptorCount);
        bindingCount = mapping->descriptorCount;

        for (int i = 0; i < mapping->descriptorCount; i++) {
            const GR_DESCRIPTOR_SLOT_INFO* info = &mapping->pDescriptorInfo[i];

            if (info->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
                printf("%s: nested descriptor sets are not implemented\n", __func__);
            }

            if (info->slotObjectType == GR_SLOT_UNUSED ||
                info->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
                bindings[i] = (VkDescriptorSetLayoutBinding) {
                    .binding = maxEntityIndex + 1 + i, // HACK: unique binding number, ignored
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

    const VkDescriptorSetLayoutCreateInfo createInfo = {
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
    const VkDevice vkDevice,
    const Stage* stages)
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // One descriptor set layout per stage, with an empty layout for each unused stage
    VkDescriptorSetLayout descriptorSetLayouts[MAX_STAGE_COUNT];

    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        const Stage* stage = &stages[i];

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

    const VkPipelineLayoutCreateInfo createInfo = {
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

static VkRenderPass getVkRenderPass(
    const VkDevice vkDevice,
    const GR_PIPELINE_CB_TARGET_STATE* cbTargets,
    const GR_PIPELINE_DB_STATE* dbTarget)
{
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkAttachmentDescription descriptions[GR_MAX_COLOR_TARGETS + 1];
    VkAttachmentReference colorReferences[GR_MAX_COLOR_TARGETS];
    VkAttachmentReference depthStencilReference;
    uint32_t descriptionIdx = 0;
    uint32_t colorReferenceIdx = 0;
    bool hasDepthStencil = false;

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &cbTargets[i];
        VkFormat vkFormat = getVkFormat(target->format);

        if (vkFormat == VK_FORMAT_UNDEFINED) {
            continue;
        }

        descriptions[descriptionIdx] = (VkAttachmentDescription) {
            .flags = 0,
            .format = vkFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = (target->channelWriteMask & 0xF) != 0 ?
                       VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        colorReferences[colorReferenceIdx] = (VkAttachmentReference) {
            .attachment = descriptionIdx,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        descriptionIdx++;
        colorReferenceIdx++;
    }

    VkFormat dbVkFormat = getVkFormat(dbTarget->format);
    if (dbVkFormat != VK_FORMAT_UNDEFINED) {
        // Table 10 in the API reference
        bool hasDepth = dbTarget->format.channelFormat == GR_CH_FMT_R16 ||
                        dbTarget->format.channelFormat == GR_CH_FMT_R32 ||
                        dbTarget->format.channelFormat == GR_CH_FMT_R16G8 ||
                        dbTarget->format.channelFormat == GR_CH_FMT_R32G8;
        bool hasStencil = dbTarget->format.channelFormat == GR_CH_FMT_R8 ||
                          dbTarget->format.channelFormat == GR_CH_FMT_R16G8 ||
                          dbTarget->format.channelFormat == GR_CH_FMT_R32G8;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (hasDepth && hasStencil) {
            layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else if (hasDepth && !hasStencil) {
            layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        } else if (!hasDepth && hasStencil) {
            layout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        }

        descriptions[descriptionIdx] = (VkAttachmentDescription) {
            .flags = 0,
            .format = dbVkFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = hasDepth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = hasDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = hasStencil ?
                             VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = hasStencil ?
                             VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = layout,
            .finalLayout = layout,
        };

        depthStencilReference = (VkAttachmentReference) {
            .attachment = descriptionIdx,
            .layout = layout,
        };

        descriptionIdx++;
        hasDepthStencil = true;
    }

    const VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = colorReferenceIdx,
        .pColorAttachments = colorReferences,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = hasDepthStencil ? &depthStencilReference : NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = descriptionIdx,
        .pAttachments = descriptions,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    if (vkCreateRenderPass(vkDevice, &renderPassCreateInfo, NULL, &renderPass) != VK_SUCCESS) {
        printf("%s: vkCreateRenderPass failed\n", __func__);
        return VK_NULL_HANDLE;
    }

    return renderPass;
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
    GrvkDevice* grvkDevice = (GrvkDevice*)device;

    // Ignored parameters:
    // - iaState.disableVertexReuse (hint)
    // - tessState.optimalTessFactor (hint)

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

    VkPipelineViewportStateCreateInfo* viewportStateCreateInfo =
        malloc(sizeof(VkPipelineViewportStateCreateInfo));

    *viewportStateCreateInfo = (VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 0, // Dynamic state
        .pViewports = NULL, // Dynamic state
        .scissorCount = 0, // Dynamic state
        .pScissors = NULL, // Dynamic state
    };

    VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo =
        malloc(sizeof(VkPipelineRasterizationStateCreateInfo));
    VkPipelineRasterizationDepthClipStateCreateInfoEXT* depthClipStateCreateInfo =
        malloc(sizeof(VkPipelineRasterizationDepthClipStateCreateInfoEXT));

    *depthClipStateCreateInfo = (VkPipelineRasterizationDepthClipStateCreateInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .depthClipEnable = pCreateInfo->rsState.depthClipEnable,
    };

    *rasterizationStateCreateInfo = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = depthClipStateCreateInfo,
        .flags = 0,
        .depthClampEnable = VK_TRUE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL, // TODO implement wireframe
        .cullMode = 0, // Dynamic state
        .frontFace = 0, // Dynamic state
        .depthBiasEnable = VK_TRUE,
        .depthBiasConstantFactor = 0.f, // Dynamic state
        .depthBiasClamp = 0.f, // Dynamic state
        .depthBiasSlopeFactor = 0.f, // Dynamic state
        .lineWidth = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo* msaaStateCreateInfo =
        malloc(sizeof(VkPipelineMultisampleStateCreateInfo));

    *msaaStateCreateInfo = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, // TODO implement MSAA
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = NULL, // TODO implement MSAA
        .alphaToCoverageEnable = pCreateInfo->cbState.alphaToCoverageEnable ? VK_TRUE : VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo* colorBlendStateCreateInfo =
        malloc(sizeof(VkPipelineColorBlendStateCreateInfo));
    VkPipelineColorBlendAttachmentState* attachments =
        malloc(sizeof(VkPipelineColorBlendAttachmentState) * GR_MAX_COLOR_TARGETS);

    // TODO implement
    if (pCreateInfo->cbState.dualSourceBlendEnable) {
        printf("%s: dual source blend is not implemented\n", __func__);
    }

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &pCreateInfo->cbState.target[i];

        if (target->blendEnable) {
            // TODO implement blend settings
            attachments[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = getVkColorComponentFlags(target->channelWriteMask),
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
                .colorWriteMask = getVkColorComponentFlags(target->channelWriteMask),
            };
        }
    }

    *colorBlendStateCreateInfo = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_TRUE,
        .logicOp = getVkLogicOp(pCreateInfo->cbState.logicOp),
        .attachmentCount = GR_MAX_COLOR_TARGETS,
        .pAttachments = attachments,
        .blendConstants = { 0.f }, // Dynamic state
    };

    VkPipelineLayout layout = getVkPipelineLayout(grvkDevice->device, stages);
    if (layout == VK_NULL_HANDLE) {
        free(shaderStageCreateInfo);
        free(vertexInputStateCreateInfo);
        free(inputAssemblyStateCreateInfo);
        free(tessellationStateCreateInfo);
        free(depthClipStateCreateInfo);
        free(attachments);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkRenderPass renderPass = getVkRenderPass(grvkDevice->device,
                                              pCreateInfo->cbState.target, &pCreateInfo->dbState);
    if (renderPass == VK_NULL_HANDLE)
    {
        free(shaderStageCreateInfo);
        free(vertexInputStateCreateInfo);
        free(inputAssemblyStateCreateInfo);
        free(tessellationStateCreateInfo);
        free(depthClipStateCreateInfo);
        free(attachments);
        vkDestroyPipelineLayout(grvkDevice->device, layout, NULL);
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
        .pViewportState = viewportStateCreateInfo,
        .pRasterizationState = rasterizationStateCreateInfo,
        .pMultisampleState = msaaStateCreateInfo,
        .pDepthStencilState = NULL, // Filled in at bind time
        .pColorBlendState = colorBlendStateCreateInfo,
        .pDynamicState = NULL, // TODO implement VK_EXT_extended_dynamic_state
        .layout = layout,
        .renderPass = renderPass,
        .subpass = 0,
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
