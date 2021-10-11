#include "mantle_internal.h"
#include "amdilc.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const VkShaderStageFlagBits flags;
} Stage;

static void copyDescriptorSetMapping(
    GR_DESCRIPTOR_SET_MAPPING* dst,
    const GR_DESCRIPTOR_SET_MAPPING* src);

static void copyDescriptorSlotInfo(
    GR_DESCRIPTOR_SLOT_INFO* dst,
    const GR_DESCRIPTOR_SLOT_INFO* src)
{
    dst->slotObjectType = src->slotObjectType;
    if (src->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
        // Go down one level...
        dst->pNextLevelSet = malloc(sizeof(GR_DESCRIPTOR_SET_MAPPING));
        copyDescriptorSetMapping((GR_DESCRIPTOR_SET_MAPPING*)dst->pNextLevelSet,
                                 src->pNextLevelSet);
    } else {
        dst->shaderEntityIndex = src->shaderEntityIndex;
    }
}

static void copyDescriptorSetMapping(
    GR_DESCRIPTOR_SET_MAPPING* dst,
    const GR_DESCRIPTOR_SET_MAPPING* src)
{
    dst->descriptorCount = src->descriptorCount;
    dst->pDescriptorInfo = malloc(src->descriptorCount * sizeof(GR_DESCRIPTOR_SLOT_INFO));
    for (unsigned i = 0; i < src->descriptorCount; i++) {
        copyDescriptorSlotInfo((GR_DESCRIPTOR_SLOT_INFO*)&dst->pDescriptorInfo[i],
                               &src->pDescriptorInfo[i]);
    }
}

static void copyPipelineShader(
    GR_PIPELINE_SHADER* dst,
    const GR_PIPELINE_SHADER* src)
{
    dst->shader = src->shader;
    for (unsigned i = 0; i < COUNT_OF(dst->descriptorSetMapping); i++) {
        copyDescriptorSetMapping(&dst->descriptorSetMapping[i], &src->descriptorSetMapping[i]);
    }
    dst->linkConstBufferCount = 0; // Ignored
    dst->pLinkConstBufferInfo = NULL; // Ignored
    dst->dynamicMemoryViewMapping = src->dynamicMemoryViewMapping;
}

static VkDescriptorSetLayout getVkDescriptorSetLayout(
    unsigned* dynamicOffsetCount,
    const GrDevice* grDevice,
    const Stage* stage)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    unsigned bindingCount = 0;
    VkDescriptorSetLayoutBinding* bindings = NULL;

    if (stage->shader->shader != GR_NULL_HANDLE) {
        const GrShader* grShader = stage->shader->shader;
        const GR_DYNAMIC_MEMORY_VIEW_SLOT_INFO* dynamicSlotInfo =
            &stage->shader->dynamicMemoryViewMapping;

        bindingCount = grShader->bindingCount;
        bindings = malloc(bindingCount * sizeof(VkDescriptorSetLayoutBinding));

        for (unsigned i = 0; i < grShader->bindingCount; i++) {
            const IlcBinding* binding = &grShader->bindings[i];

            VkDescriptorType vkDescriptorType = binding->descriptorType;

            if (dynamicSlotInfo->slotObjectType != GR_SLOT_UNUSED &&
                dynamicSlotInfo->shaderEntityIndex + ILC_BASE_RESOURCE_ID == binding->index) {
                // Use dynamic offsets for dynamic memory views to avoid invalidating
                // descriptor sets each time the buffer offset changes
                vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                (*dynamicOffsetCount)++;
            }

            bindings[i] = (VkDescriptorSetLayoutBinding) {
                .binding = binding->index,
                .descriptorType = vkDescriptorType,
                .descriptorCount = 1,
                .stageFlags = stage->flags,
                .pImmutableSamplers = NULL,
            };
        }
    }

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = bindingCount,
        .pBindings = bindings,
    };

    VkResult res = VKD.vkCreateDescriptorSetLayout(grDevice->device, &createInfo, NULL, &layout);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed (%d)\n", res);
    }

    free(bindings);
    return layout;
}

static VkPipelineLayout getVkPipelineLayout(
    const GrDevice* grDevice,
    unsigned stageCount,
    const Stage* stages,
    const VkDescriptorSetLayout* descriptorSetLayouts)
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = ILC_MAX_STRIDE_CONSTANTS * sizeof(uint32_t),
    };

    const VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = stageCount,
        .pSetLayouts = descriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };

    VkResult res = VKD.vkCreatePipelineLayout(grDevice->device, &createInfo, NULL, &layout);
    if (res != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed (%d)\n", res);
    }

    return layout;
}

static VkPipeline getVkPipeline(
    VkRenderPass renderPass,
    const GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    const VkFormat* vkAttachmentFormats,
    unsigned totalAttachmentCount)
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
        .polygonMode = grRasterState->polygonMode,
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
        .rasterizationSamples = grMsaaState->sampleCountFlags,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = &grMsaaState->sampleMask,
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
    // cap the maximum attachment count, as depth attachment always comes last
    totalAttachmentCount = min(GR_MAX_COLOR_TARGETS, totalAttachmentCount);
    for (unsigned i = 0; i < totalAttachmentCount; i++) {
        const VkPipelineColorBlendAttachmentState* blendState = &grColorBlendState->states[i];
        VkColorComponentFlags colorWriteMask = createInfo->colorWriteMasks[i];
        VkFormat vkFormat = vkAttachmentFormats[i];
        if (isVkFormatDepthStencil(vkFormat)) {
            // could just break though
            continue;
        }
        if (colorWriteMask == ~0u) {
            continue;
        } else {
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
        }
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
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    LOGT("creating vkPipeline %p\n", renderPass);
    vkRes = VKD.vkCreateGraphicsPipelines(grDevice->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                          NULL, &vkPipeline);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateGraphicsPipelines failed (%d)\n", vkRes);
    }
    LOGT("vkPipeline created successfuly\n");
    return vkPipeline;
}

// Exported Functions

VkPipeline grPipelineFindOrCreateVkPipeline(
    VkRenderPass renderPass,
    GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    const VkFormat* attachmentFormats,
    unsigned attachmentCount)
{
    LOGT("%p\n", grPipeline);
    VkPipeline vkPipeline = VK_NULL_HANDLE;

    EnterCriticalSection(&grPipeline->pipelineSlotsMutex);

    for (unsigned i = 0; i < grPipeline->pipelineSlotCount; i++) {
        const PipelineSlot* slot = &grPipeline->pipelineSlots[i];

        if (grColorBlendState == slot->grColorBlendState &&
            grMsaaState == slot->grMsaaState &&
            grRasterState == slot->grRasterState &&
            attachmentCount == slot->attachmentCount &&
            memcmp(slot->attachmentFormats, attachmentFormats, sizeof(VkFormat) * attachmentCount) == 0) {
            vkPipeline = slot->pipeline;
            break;
        }
    }

    LeaveCriticalSection(&grPipeline->pipelineSlotsMutex);

    if (vkPipeline == VK_NULL_HANDLE) {
        LOGT("creating pipeline: %p\n", grPipeline);
        vkPipeline = getVkPipeline(renderPass, grPipeline, grColorBlendState, grMsaaState, grRasterState, attachmentFormats, attachmentCount);

        EnterCriticalSection(&grPipeline->pipelineSlotsMutex);
        // search for the pipeline slot again
        VkPipeline conflictedVkPipeline = VK_NULL_HANDLE;
        for (unsigned i = 0; i < grPipeline->pipelineSlotCount; i++) {
            const PipelineSlot* slot = &grPipeline->pipelineSlots[i];

            if (grColorBlendState == slot->grColorBlendState &&
                grMsaaState == slot->grMsaaState &&
                grRasterState == slot->grRasterState &&
                attachmentCount == slot->attachmentCount &&
                memcmp(slot->attachmentFormats, attachmentFormats, sizeof(VkFormat) * attachmentCount) == 0) {
                conflictedVkPipeline = slot->pipeline;
                break;
            }
        }

        if (conflictedVkPipeline == VK_NULL_HANDLE) {
            grPipeline->pipelineSlotCount++;
            grPipeline->pipelineSlots = realloc(grPipeline->pipelineSlots,
                                                grPipeline->pipelineSlotCount * sizeof(PipelineSlot));
            grPipeline->pipelineSlots[grPipeline->pipelineSlotCount - 1] = (PipelineSlot) {
                .pipeline = vkPipeline,
                .grColorBlendState = grColorBlendState,
                .grMsaaState = grMsaaState,
                .grRasterState = grRasterState,
                .attachmentFormats = {},
                .attachmentCount = attachmentCount,
            };
            memcpy(grPipeline->pipelineSlots[grPipeline->pipelineSlotCount - 1].attachmentFormats, attachmentFormats, sizeof(VkFormat) * attachmentCount);
        }
        LeaveCriticalSection(&grPipeline->pipelineSlotsMutex);
        if (conflictedVkPipeline != VK_NULL_HANDLE) {
            LOGT("found existing pipeline after creating new one %p\n", conflictedVkPipeline);
            GrDevice* grDevice = GET_OBJ_DEVICE(grPipeline);
            // destroy newly created pipeline outside of mutex scope
            VKD.vkDestroyPipeline(grDevice->device, vkPipeline, NULL);
            vkPipeline = conflictedVkPipeline;
        }
    } else {
        LOGT("found existing pipeline %p\n", vkPipeline);
    }

    return vkPipeline;
}

// Shader and Pipeline Functions

GR_RESULT GR_STDCALL grCreateShader(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pShader);
    GrDevice* grDevice = (GrDevice*)device;
    VkShaderModule vkShaderModule = VK_NULL_HANDLE;

    if ((pCreateInfo->flags & GR_SHADER_CREATE_ALLOW_RE_Z) != 0) {
        LOGW("unhandled Re-Z flag\n");
    }

    IlcShader ilcShader = ilcCompileShader(pCreateInfo->pCode, pCreateInfo->codeSize);

    const VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = ilcShader.codeSize,
        .pCode = ilcShader.code,
    };

    VkResult res = VKD.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &vkShaderModule);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateShaderModule failed (%d)\n", res);
        free(ilcShader.code);
        free(ilcShader.outputLocations);
        free(ilcShader.inputs);
        free(ilcShader.bindings);
        free(ilcShader.name);
        return getGrResult(res);
    }

    GrShader* grShader = malloc(sizeof(GrShader));
    *grShader = (GrShader) {
        .grObj = { GR_OBJ_TYPE_SHADER, grDevice },
        .shaderModule = vkShaderModule,
        .code = ilcShader.code,
        .codeSize = ilcShader.codeSize,
        .bindingCount = ilcShader.bindingCount,
        .bindings = ilcShader.bindings,
        .inputCount = ilcShader.inputCount,
        .inputs = ilcShader.inputs,
        .outputCount = ilcShader.outputCount,
        .outputLocations = ilcShader.outputLocations,
        .name = ilcShader.name,
    };

    *pShader = (GR_SHADER)grShader;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateGraphicsPipeline(
    GR_DEVICE device,
    const GR_GRAPHICS_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pPipeline);
    GrDevice* grDevice = (GrDevice*)device;
    GR_RESULT res = GR_SUCCESS;
    VkDescriptorSetLayout descriptorSetLayouts[MAX_STAGE_COUNT] = { VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    unsigned dynamicOffsetCount = 0;

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
    VkShaderModule recompiledGeometryShader = VK_NULL_HANDLE;

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
        VkShaderModule shaderModule = grShader->shaderModule;
        LOGT("linking shader %s\n", grShader->name == NULL ? "no_name" : grShader->name);

        shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = stage->flags,
            .module = shaderModule,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;
    }

    if (pCreateInfo->iaState.topology == GR_TOPOLOGY_RECT_LIST) {
        if (stages[3].shader->shader == GR_NULL_HANDLE) {
            GrShader* grPixelShader = (GrShader*)stages[4].shader->shader;
            IlcShader geomShader = ilcCompileRectangleGeometryShader(
                grPixelShader == NULL ? 0 : grPixelShader->inputCount,
                grPixelShader == NULL ? NULL : grPixelShader->inputs);
            const VkShaderModuleCreateInfo geomShaderCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = geomShader.codeSize,
                .pCode = geomShader.code,
            };
            VkResult vkRes = VKD.vkCreateShaderModule(grDevice->device, &geomShaderCreateInfo, NULL, &recompiledGeometryShader);
            free(geomShader.code);
            if (vkRes != VK_SUCCESS) {
                res = getGrResult(vkRes);
                goto bail;
            }
            shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
                .module = recompiledGeometryShader,
                .pName = "main",
                .pSpecializationInfo = NULL,
            };
            stageCount++;
        } else {
            LOGW("unhandled geometry shader stage with RECT_LIST topology\n");
        }
    }
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
        .depthAttachmentEnable = pCreateInfo->dbState.format.channelFormat != GR_CH_FMT_UNDEFINED || pCreateInfo->dbState.format.numericFormat != GR_NUM_FMT_UNDEFINED,
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

    // Create one descriptor set layout per stage
    for (unsigned i = 0; i < COUNT_OF(stages); i++) {
        descriptorSetLayouts[i] = getVkDescriptorSetLayout(&dynamicOffsetCount,
                                                           grDevice, &stages[i]);
        if (descriptorSetLayouts[i] == VK_NULL_HANDLE) {
            res = GR_ERROR_OUT_OF_MEMORY;
            goto bail;
        }
    }

    pipelineLayout = getVkPipelineLayout(grDevice, COUNT_OF(stages), stages, descriptorSetLayouts);
    if (pipelineLayout == VK_NULL_HANDLE) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .createInfo = pipelineCreateInfo,
        .pipelineSlotCount = 0,
        .pipelineSlots = NULL,
        .pipelineSlotsMutex = { 0 }, // Initialized below
        .pipelineLayout = pipelineLayout,
        .stageCount = COUNT_OF(stages),
        .descriptorSetLayouts = { 0 }, // Initialized below
        .shaderInfos = { { 0 } }, // Initialized below
        .dynamicOffsetCount = dynamicOffsetCount,
        .patchedGeometryModule = recompiledGeometryShader,
    };

    InitializeCriticalSectionAndSpinCount(&grPipeline->pipelineSlotsMutex, 0);
    for (unsigned i = 0; i < COUNT_OF(stages); i++) {
        grPipeline->descriptorSetLayouts[i] = descriptorSetLayouts[i];
        copyPipelineShader(&grPipeline->shaderInfos[i], stages[i].shader);
    }

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    if (recompiledGeometryShader != VK_NULL_HANDLE) {
        VKD.vkDestroyShaderModule(grDevice->device, recompiledGeometryShader, NULL);
    }
    for (unsigned i = 0; i < COUNT_OF(descriptorSetLayouts); i++) {
        VKD.vkDestroyDescriptorSetLayout(grDevice->device, descriptorSetLayouts[i], NULL);
    }
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    return res;
}

GR_RESULT GR_STDCALL grCreateComputePipeline(
    GR_DEVICE device,
    const GR_COMPUTE_PIPELINE_CREATE_INFO* pCreateInfo,
    GR_PIPELINE* pPipeline)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pPipeline);
    GrDevice* grDevice = (GrDevice*)device;
    GR_RESULT res = GR_SUCCESS;
    VkResult vkRes;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline vkPipeline = VK_NULL_HANDLE;
    unsigned dynamicOffsetCount = 0;

    // TODO validate parameters

    Stage stage = { &pCreateInfo->cs, VK_SHADER_STAGE_COMPUTE_BIT };

    if (stage.shader->linkConstBufferCount > 0) {
        // TODO implement
        LOGW("link-time constant buffers are not implemented\n");
    }

    GrShader* grShader = (GrShader*)stage.shader->shader;

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = stage.flags,
        .module = grShader->shaderModule,
        .pName = "main",
        .pSpecializationInfo = NULL,
    };

    descriptorSetLayout = getVkDescriptorSetLayout(&dynamicOffsetCount, grDevice, &stage);
    if (descriptorSetLayout == VK_NULL_HANDLE) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }

    pipelineLayout = getVkPipelineLayout(grDevice, 1, &stage, &descriptorSetLayout);
    if (pipelineLayout == VK_NULL_HANDLE) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                 VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stage = shaderStageCreateInfo,
        .layout = pipelineLayout,
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
    *pipelineSlot = (PipelineSlot) {
        .pipeline = vkPipeline,
        .grColorBlendState = NULL,
    };

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .createInfo = NULL,
        .pipelineSlotCount = 1,
        .pipelineSlots = pipelineSlot,
        .pipelineSlotsMutex = { 0 }, // Initialized below
        .pipelineLayout = pipelineLayout,
        .stageCount = 1,
        .descriptorSetLayouts = { descriptorSetLayout },
        .shaderInfos = { { 0 } }, // Initialized below
        .dynamicOffsetCount = dynamicOffsetCount,
    };

    InitializeCriticalSectionAndSpinCount(&grPipeline->pipelineSlotsMutex, 0);
    copyPipelineShader(&grPipeline->shaderInfos[0], stage.shader);

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    VKD.vkDestroyDescriptorSetLayout(grDevice->device, descriptorSetLayout, NULL);
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    return res;
}
