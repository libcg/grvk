#include "mantle_internal.h"
#include "amdilc.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    VkShaderStageFlagBits flags;
} Stage;

static VkRenderPass getVkRenderPass(
    const GrDevice* grDevice,
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
            .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        colorReferences[colorReferenceCount] = (VkAttachmentReference) {
            .attachment = descriptionCount,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
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
            .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        depthStencilReference = (VkAttachmentReference) {
            .attachment = descriptionCount,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
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

    VkResult res = VKD.vkCreateRenderPass(grDevice->device, &renderPassCreateInfo, NULL,
                                          &renderPass);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateRenderPass failed (%d)\n", res);
        return VK_NULL_HANDLE;
    }

    return renderPass;
}

static VkPipeline getVkPipeline(
    const GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grPipeline);
    const PipelineCreateInfo* createInfo = grPipeline->createInfo;
    VkPipeline vkPipeline = VK_NULL_HANDLE;
    VkResult vkRes;

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
        .topology = createInfo->topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Ignored if no tessellation shaders are present
    const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo =  {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .patchControlPoints = createInfo->patchControlPoints,
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
        .depthClipEnable = createInfo->depthClipEnable,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = &depthClipStateCreateInfo,
        .flags = 0,
        .depthClampEnable = VK_TRUE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL, // TODO implement polygon modes
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
        .alphaToCoverageEnable = createInfo->alphaToCoverageEnable,
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

    unsigned attachmentCount = 0;
    VkPipelineColorBlendAttachmentState attachments[GR_MAX_COLOR_TARGETS];

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const VkPipelineColorBlendAttachmentState* blendState = &grColorBlendState->states[i];
        VkColorComponentFlags colorWriteMask = createInfo->colorWriteMasks[i];

        if (colorWriteMask == ~0u) {
            continue;
        }

        attachments[attachmentCount] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable = blendState->blendEnable,
            .srcColorBlendFactor = blendState->srcColorBlendFactor,
            .dstColorBlendFactor = blendState->dstColorBlendFactor,
            .colorBlendOp = blendState->colorBlendOp,
            .srcAlphaBlendFactor = blendState->srcAlphaBlendFactor,
            .dstAlphaBlendFactor = blendState->dstAlphaBlendFactor,
            .alphaBlendOp = blendState->alphaBlendOp,
            .colorWriteMask = colorWriteMask,
        };
        attachmentCount++;
    }

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = createInfo->logicOpEnable,
        .logicOp = createInfo->logicOp,
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
        .dynamicStateCount = COUNT_OF(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = createInfo->createFlags,
        .stageCount = createInfo->stageCount,
        .pStages = createInfo->stageCreateInfos,
        .pVertexInputState = &vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssemblyStateCreateInfo,
        .pTessellationState = &tessellationStateCreateInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizationStateCreateInfo,
        .pMultisampleState = &msaaStateCreateInfo,
        .pDepthStencilState = &depthStencilStateCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = grPipeline->pipelineLayout,
        .renderPass = grPipeline->renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    vkRes = VKD.vkCreateGraphicsPipelines(grDevice->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                          NULL, &vkPipeline);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateGraphicsPipelines failed (%d)\n", vkRes);
    }

    return vkPipeline;
}

// Exported Functions

VkPipeline grPipelineFindOrCreateVkPipeline(
    GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState)
{
    VkPipeline vkPipeline = VK_NULL_HANDLE;

    EnterCriticalSection(&grPipeline->pipelineSlotsMutex);

    for (unsigned i = 0; i < grPipeline->pipelineSlotCount; i++) {
        const PipelineSlot* slot = &grPipeline->pipelineSlots[i];

        if (grColorBlendState == slot->grColorBlendState) {
            vkPipeline = slot->pipeline;
            break;
        }
    }

    if (vkPipeline == VK_NULL_HANDLE) {
        vkPipeline = getVkPipeline(grPipeline, grColorBlendState);

        grPipeline->pipelineSlotCount++;
        grPipeline->pipelineSlots = realloc(grPipeline->pipelineSlots,
                                            grPipeline->pipelineSlotCount * sizeof(PipelineSlot));
        grPipeline->pipelineSlots[grPipeline->pipelineSlotCount - 1] = (PipelineSlot) {
            .pipeline = vkPipeline,
            .grColorBlendState = grColorBlendState,
        };
    }

    LeaveCriticalSection(&grPipeline->pipelineSlotsMutex);

    return vkPipeline;
}

// Shader and Pipeline Functions

GR_RESULT grCreateShader(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pShader);
    GrDevice* grDevice = (GrDevice*)device;
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->grBaseObj.grObjType != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pShader == NULL || pCreateInfo->pCode == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    if ((pCreateInfo->flags & GR_SHADER_CREATE_ALLOW_RE_Z) != 0) {
        LOGW("unhandled Re-Z flag\n");
    }
    GrShader* grShader = malloc(sizeof(GrShader));
    if (grShader == NULL) {
        return GR_ERROR_OUT_OF_MEMORY;
    }

    uint32_t* codeCopy = (uint32_t*)malloc(pCreateInfo->codeSize);

    if (codeCopy == NULL) {
        return GR_ERROR_OUT_OF_MEMORY;
    }

    memcpy(codeCopy, pCreateInfo->pCode, pCreateInfo->codeSize);

    *grShader = (GrShader) {
        .grObj = { GR_OBJ_TYPE_SHADER, grDevice },
        .code = codeCopy,
        .codeSize = pCreateInfo->codeSize,
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
    GR_RESULT res = GR_SUCCESS;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // TODO validate parameters

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
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo[COUNT_OF(stages)];

    for (int i = 0; i < COUNT_OF(stages); i++) {
        Stage* stage = &stages[i];

        if (stage->shader->shader == GR_NULL_HANDLE) {
            continue;
        }

        if (stage->shader->linkConstBufferCount > 0) {
            // TODO implement
            LOGW("link-time constant buffers are not implemented\n");
        }

        GrShader* grShader = (GrShader*)stage->shader->shader;

        IlcShader ilcShader = ilcCompileShader(grShader->code, grShader->codeSize, stage->shader);
        if (ilcShader.code == NULL) {
            res = GR_ERROR_OUT_OF_MEMORY;
            goto bail;
        }
        LOGT("shader compiled\n");
        const VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .codeSize = ilcShader.codeSize,
            .pCode = ilcShader.code
        };
        // idk whether to free spirvCode in case if shader compiles correctly
        VkShaderModule module;
        VkResult vkRes = VKD.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &module);
        free(ilcShader.code);
        if (vkRes != VK_SUCCESS) {
            res = getGrResult(vkRes);
            goto bail;
        }
        shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = stage->flags,
            .module = module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;
    }
    LOGT("shaders compiled successfuly\n");
    // TODO implement
    if (pCreateInfo->cbState.dualSourceBlendEnable) {
        LOGW("dual source blend is not implemented\n");
    }

    unsigned attachmentCount = 0;
    VkColorComponentFlags colorWriteMasks[GR_MAX_COLOR_TARGETS];

    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &pCreateInfo->cbState.target[i];

        if (!target->blendEnable &&
            target->format.channelFormat == GR_CH_FMT_UNDEFINED &&
            target->format.numericFormat == GR_NUM_FMT_UNDEFINED &&
            target->channelWriteMask == 0) {
            colorWriteMasks[attachmentCount] = ~0u;
        } else {
            colorWriteMasks[attachmentCount] = getVkColorComponentFlags(target->channelWriteMask);
        }

        attachmentCount++;
    }

    PipelineCreateInfo* pipelineCreateInfo = malloc(sizeof(PipelineCreateInfo));
    *pipelineCreateInfo = (PipelineCreateInfo) {
        .createFlags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                       VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stageCount = stageCount,
        .stageCreateInfos = { { 0 } }, // Initialized below
        .topology = getVkPrimitiveTopology(pCreateInfo->iaState.topology),
        .patchControlPoints = pCreateInfo->tessState.patchControlPoints,
        .depthClipEnable = !!pCreateInfo->rsState.depthClipEnable,
        .alphaToCoverageEnable = !!pCreateInfo->cbState.alphaToCoverageEnable,
        .logicOpEnable = pCreateInfo->cbState.logicOp != GR_LOGIC_OP_COPY,
        .logicOp = getVkLogicOp(pCreateInfo->cbState.logicOp),
        .colorWriteMasks = { 0 }, // Initialized below
    };

    memcpy(pipelineCreateInfo->stageCreateInfos, shaderStageCreateInfo,
           stageCount * sizeof(VkPipelineShaderStageCreateInfo));
    memcpy(pipelineCreateInfo->colorWriteMasks, colorWriteMasks,
           GR_MAX_COLOR_TARGETS * sizeof(VkColorComponentFlags));

    renderPass = getVkRenderPass(grDevice, pCreateInfo->cbState.target, &pCreateInfo->dbState);
    if (renderPass == VK_NULL_HANDLE)
    {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    if (grPipeline == NULL) {
        LOGE("failed to allocate memory for GrPipeline\n");
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .createInfo = pipelineCreateInfo,
        .pipelineSlotCount = 0,
        .pipelineSlots = NULL,
        .pipelineSlotsMutex = { 0 }, // Initialized below
        .pipelineLayout = grDevice->pipelineLayouts.graphicsPipelineLayout,
        .renderPass = renderPass,
    };

    InitializeCriticalSectionAndSpinCount(&grPipeline->pipelineSlotsMutex, 0);

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    for (unsigned i = 0; i < stageCount;++i) {
        if (shaderStageCreateInfo[i].module != VK_NULL_HANDLE) {
            VKD.vkDestroyShaderModule(grDevice->device, shaderStageCreateInfo[i].module, NULL);
        }
    }
    if (renderPass != VK_NULL_HANDLE) {
        VKD.vkDestroyRenderPass(grDevice->device, renderPass, NULL);
    }
    return res;
}

GR_RESULT grCreateComputePipeline(
    GR_DEVICE device,
    const GR_COMPUTE_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pPipeline);
    GrDevice* grDevice = (GrDevice*)device;
    GR_RESULT res = GR_SUCCESS;
    VkResult vkRes;
    VkPipeline vkPipeline = VK_NULL_HANDLE;

    // TODO validate parameters

    Stage stage = { &pCreateInfo->cs, VK_SHADER_STAGE_COMPUTE_BIT };

    if (stage.shader->linkConstBufferCount > 0) {
        // TODO implement
        LOGW("link-time constant buffers are not implemented\n");
    }

    GrShader* grShader = (GrShader*)stage.shader->shader;

    IlcShader ilcShader = ilcCompileShader(grShader->code, grShader->codeSize, stage.shader);
    if (ilcShader.code == NULL) {
        return GR_ERROR_OUT_OF_MEMORY;
    }
    const VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = ilcShader.codeSize,
        .pCode = ilcShader.code
    };
    // idk whether to free spirvCode in case if shader compiles correctly
    VkShaderModule module = VK_NULL_HANDLE;
    vkRes = VKD.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &module);

    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateShaderModule failed\n");
        res = getGrResult(vkRes);
        goto bail;
    }

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = stage.flags,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = NULL,
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                 VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stage = shaderStageCreateInfo,
        .layout = grDevice->pipelineLayouts.computePipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    vkRes = VKD.vkCreateComputePipelines(grDevice->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                         NULL, &vkPipeline);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateComputePipelines failed (%d)\n", vkRes);
        res = getGrResult(vkRes);
        goto bail;
    }

    PipelineSlot* pipelineSlot = malloc(sizeof(PipelineSlot));
    if (pipelineSlot == NULL) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    *pipelineSlot = (PipelineSlot) {
        .pipeline = vkPipeline,
        .grColorBlendState = NULL,
    };

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    if (grPipeline == NULL) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .createInfo = NULL,
        .pipelineSlotCount = 1,
        .pipelineSlots = pipelineSlot,
        .pipelineSlotsMutex = { 0 }, // Initialized below
        .pipelineLayout = grDevice->pipelineLayouts.computePipelineLayout,
        .renderPass = VK_NULL_HANDLE,
    };

    InitializeCriticalSectionAndSpinCount(&grPipeline->pipelineSlotsMutex, 0);

    *pPipeline = (GR_PIPELINE)grPipeline;
    VKD.vkDestroyShaderModule(grDevice->device, module, NULL);
    return GR_SUCCESS;

bail:
    if (ilcShader.code != NULL) {
        free(ilcShader.code);
    }
    if (pipelineSlot != NULL) {
        free(pipelineSlot);
    }
    if (grPipeline != NULL) {
        free(grPipeline);
    }
    if (module != VK_NULL_HANDLE) {
        VKD.vkDestroyShaderModule(grDevice->device, module, NULL);
    }
    return res;
}
