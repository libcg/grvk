#include "mantle_internal.h"
#include "amdilc.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const VkShaderStageFlagBits flags;
} Stage;

static VkDescriptorSetLayout getVkDescriptorSetLayout(
    const VkDevice vkDevice,
    const Stage* stage)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayoutBinding* bindings = NULL;
    unsigned bindingCount = 0;

    if (stage->shader->shader != GR_NULL_HANDLE) {
        for (int i = 1; i < GR_MAX_DESCRIPTOR_SETS; i++) {
            const GR_DESCRIPTOR_SET_MAPPING* mapping = &stage->shader->descriptorSetMapping[i];

            if (mapping->descriptorCount > 0) {
                LOGW("multiple descriptor sets per stage is not supported\n");
                break;
            }
        }

        // TODO handle all descriptor sets
        const GR_DESCRIPTOR_SET_MAPPING* mapping = &stage->shader->descriptorSetMapping[0];

        // Find maximum entity index to generate unique binding numbers for unused slots
        unsigned maxEntityIndex = 0;
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
                LOGW("nested descriptor sets are not implemented\n");
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

    if (vki.vkCreateDescriptorSetLayout(vkDevice, &createInfo, NULL, &layout) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed\n");
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
                vki.vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[j], NULL);
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

    if (vki.vkCreatePipelineLayout(vkDevice, &createInfo, NULL, &layout) != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed\n");
        for (int i = 0; i < MAX_STAGE_COUNT; i++) {
            vki.vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayouts[i], NULL);
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
    unsigned descriptionCount = 0;
    unsigned colorReferenceCount = 0;
    bool hasDepthStencil = false;

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &cbTargets[i];
        VkFormat vkFormat = getVkFormat(target->format);

        if (vkFormat == VK_FORMAT_UNDEFINED) {
            continue;
        }

        descriptions[descriptionCount] = (VkAttachmentDescription) {
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

        colorReferences[colorReferenceCount] = (VkAttachmentReference) {
            .attachment = descriptionCount,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        descriptionCount++;
        colorReferenceCount++;
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

        descriptions[descriptionCount] = (VkAttachmentDescription) {
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
            .attachment = descriptionCount,
            .layout = layout,
        };

        descriptionCount++;
        hasDepthStencil = true;
    }

    const VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = colorReferenceCount,
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
        .attachmentCount = descriptionCount,
        .pAttachments = descriptions,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    if (vki.vkCreateRenderPass(vkDevice, &renderPassCreateInfo, NULL, &renderPass) != VK_SUCCESS) {
        LOGE("vkCreateRenderPass failed\n");
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
    LOGT("%p %p %p\n", device, pCreateInfo, pShader);
    GrDevice* grDevice = (GrDevice*)device;
    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    unsigned spirvCodeSize;
    uint32_t* spirvCode;

    if ((pCreateInfo->flags & GR_SHADER_CREATE_ALLOW_RE_Z) != 0) {
        LOGW("unhandled Re-Z flag\n");
    }

    if ((pCreateInfo->flags & GR_SHADER_CREATE_SPIRV) == 0) {
        spirvCode = ilcCompileShader(&spirvCodeSize, pCreateInfo->pCode, pCreateInfo->codeSize);
    } else {
        spirvCodeSize = pCreateInfo->codeSize;
        spirvCode = (uint32_t*)pCreateInfo->pCode;
    }

    const VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = spirvCodeSize,
        .pCode = spirvCode,
    };

    if (vki.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &vkShaderModule)) {
        LOGE("vkCreateShaderModule failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    if ((pCreateInfo->flags & GR_SHADER_CREATE_SPIRV) == 0) {
        free(spirvCode);
    }

    GrShader* grShader = malloc(sizeof(GrShader));
    *grShader = (GrShader) {
        .sType = GR_STRUCT_TYPE_SHADER,
        .shaderModule = vkShaderModule,
    };

    *pShader = (GR_SHADER)grShader;
    return GR_SUCCESS;
}

GR_RESULT grCreateGraphicsPipeline(
    GR_DEVICE device,
    const GR_GRAPHICS_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pPipeline);
    GrDevice* grDevice = (GrDevice*)device;

    // Ignored parameters:
    // - iaState.disableVertexReuse (hint)
    // - tessState.optimalTessFactor (hint)

    Stage stages[MAX_STAGE_COUNT] = {
        { &pCreateInfo->vs, VK_SHADER_STAGE_VERTEX_BIT },
        { &pCreateInfo->hs, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { &pCreateInfo->ds, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
        { &pCreateInfo->gs, VK_SHADER_STAGE_GEOMETRY_BIT },
        { &pCreateInfo->ps, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    unsigned stageCount = 0;
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo[MAX_STAGE_COUNT];

    for (int i = 0; i < MAX_STAGE_COUNT; i++) {
        Stage* stage = &stages[i];

        if (stage->shader->shader == GR_NULL_HANDLE) {
            continue;
        }

        if (stage->shader->linkConstBufferCount > 0) {
            // TODO implement
            LOGW("link-time constant buffers are not implemented\n");
        }

        if (stage->shader->dynamicMemoryViewMapping.slotObjectType != GR_SLOT_UNUSED) {
            // TODO implement
            LOGW("dynamic memory view mapping is not implemented\n");
        }

        GrShader* grShader = (GrShader*)stage->shader->shader;

        shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = stage->flags,
            .module = grShader->shaderModule,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;
    }

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = getVkPrimitiveTopology(pCreateInfo->iaState.topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    // Ignored if no tessellation shaders are present
    const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo =  {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .patchControlPoints = pCreateInfo->tessState.patchControlPoints,
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 0, // Dynamic state
        .pViewports = NULL, // Dynamic state
        .scissorCount = 0, // Dynamic state
        .pScissors = NULL, // Dynamic state
    };

    const VkPipelineRasterizationDepthClipStateCreateInfoEXT depthClipStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .depthClipEnable = pCreateInfo->rsState.depthClipEnable,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = &depthClipStateCreateInfo,
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

    const VkPipelineMultisampleStateCreateInfo msaaStateCreateInfo = {
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

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = 0, // Dynamic state
        .depthWriteEnable = 0, // Dynamic state
        .depthCompareOp = 0, // Dynamic state
        .depthBoundsTestEnable = 0, // Dynamic state
        .stencilTestEnable = 0, // Dynamic state
        .front = { 0 }, // Dynamic state
        .back = { 0 }, // Dynamic state
        .minDepthBounds = 0.f, // Dynamic state
        .maxDepthBounds = 0.f, // Dynamic state
    };

    // TODO implement
    if (pCreateInfo->cbState.dualSourceBlendEnable) {
        LOGW("dual source blend is not implemented\n");
    }

    unsigned attachmentCount = 0;
    VkPipelineColorBlendAttachmentState attachments[GR_MAX_COLOR_TARGETS];

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &pCreateInfo->cbState.target[i];

        if (!target->blendEnable &&
            target->format.channelFormat == GR_CH_FMT_UNDEFINED &&
            target->format.numericFormat == GR_NUM_FMT_UNDEFINED &&
            target->channelWriteMask == 0) {
            continue;
        }

        if (target->blendEnable) {
            // TODO implement blend settings
            attachments[attachmentCount] = (VkPipelineColorBlendAttachmentState) {
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
            attachments[attachmentCount] = (VkPipelineColorBlendAttachmentState) {
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

        attachmentCount++;
    }

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_TRUE,
        .logicOp = getVkLogicOp(pCreateInfo->cbState.logicOp),
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .blendConstants = { 0.f }, // Dynamic state
    };

    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_CULL_MODE_EXT,
        VK_DYNAMIC_STATE_FRONT_FACE_EXT,
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_STENCIL_OP_EXT,
    };

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = sizeof(dynamicStates) / sizeof(VkDynamicState),
        .pDynamicStates = dynamicStates,
    };

    VkPipelineLayout layout = getVkPipelineLayout(grDevice->device, stages);
    if (layout == VK_NULL_HANDLE) {
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkRenderPass renderPass = getVkRenderPass(grDevice->device,
                                              pCreateInfo->cbState.target, &pCreateInfo->dbState);
    if (renderPass == VK_NULL_HANDLE)
    {
        vki.vkDestroyPipelineLayout(grDevice->device, layout, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                 VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stageCount = stageCount,
        .pStages = shaderStageCreateInfo,
        .pVertexInputState = &vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssemblyStateCreateInfo,
        .pTessellationState = &tessellationStateCreateInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizationStateCreateInfo,
        .pMultisampleState = &msaaStateCreateInfo,
        .pDepthStencilState = &depthStencilStateCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = layout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    if (vki.vkCreateGraphicsPipelines(grDevice->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                      NULL, &vkPipeline) != VK_SUCCESS) {
        LOGE("vkCreateGraphicsPipelines failed\n");
        vki.vkDestroyPipelineLayout(grDevice->device, layout, NULL);
        vki.vkDestroyRenderPass(grDevice->device, renderPass, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .sType = GR_STRUCT_TYPE_PIPELINE,
        .pipelineLayout = layout,
        .pipeline = vkPipeline,
        .renderPass = renderPass,
    };

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;
}
