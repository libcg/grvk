#include "mantle_internal.h"
#include "amdilc.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const VkShaderStageFlagBits flags;
} Stage;

static void copyDescriptorSetMapping(
    GrDescriptorSetMapping* dst,
    unsigned* pTotalDescriptorBindingCount,
    const GR_DESCRIPTOR_SET_MAPPING* src,
    unsigned bindingCount,
    const IlcBinding* bindings);

static bool copyDescriptorSlotInfo(
    GrPipelineDescriptorSlotInfo* dst,
    unsigned* pTotalDescriptorBindingCount,
    const GR_DESCRIPTOR_SLOT_INFO* src,
    unsigned descriptorIndex,
    unsigned bindingCount,
    const IlcBinding* bindings)
{
    dst->slotObjectType = src->slotObjectType;
    dst->descriptorSlotIndex = descriptorIndex;
    dst->descriptorType = -1;
    if (src->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
        // Go down one level...
        dst->pNextLevelSet = malloc(sizeof(GrDescriptorSetMapping));
        copyDescriptorSetMapping((GrDescriptorSetMapping*)dst->pNextLevelSet,
                                 pTotalDescriptorBindingCount,
                                 src->pNextLevelSet,
                                 bindingCount,
                                 bindings);
    } else if (src->slotObjectType != GR_SLOT_UNUSED) {
        (*pTotalDescriptorBindingCount)++;
        dst->bindingIndex = src->shaderEntityIndex;

        if (src->slotObjectType != GR_SLOT_SHADER_SAMPLER) {
            dst->bindingIndex += ILC_BASE_RESOURCE_ID;
        } else {
            dst->bindingIndex += ILC_BASE_SAMPLER_ID;
        }
        for (unsigned i = 0; i < bindingCount; ++i) {
            const IlcBinding* binding = &bindings[i];
            if (binding->index == dst->bindingIndex) {
                dst->descriptorType = binding->descriptorType;
                dst->strideIndex = binding->strideIndex;
                break;
            }
        }
        if (dst->descriptorType == -1) {
            (*pTotalDescriptorBindingCount)--;
            LOGT("failed to find appropriate descriptor type in bindings for index %d\n", src->shaderEntityIndex);
            return false;
        }
    }
    return true;
}

static void copyDescriptorSetMapping(
    GrDescriptorSetMapping* dst,
    unsigned* pTotalDescriptorBindingCount,
    const GR_DESCRIPTOR_SET_MAPPING* src,
    unsigned bindingCount,
    const IlcBinding* bindings)
{
    unsigned usedDescriptorCount = 0;
    for (unsigned i = 0; i < src->descriptorCount; i++) {
        //TODO: also validate bindings here
        if (src->pDescriptorInfo[i].slotObjectType != GR_SLOT_UNUSED) {
            usedDescriptorCount++;
        }
    }
    dst->descriptorSlotCount = usedDescriptorCount;
    dst->pDescriptorInfo = malloc(usedDescriptorCount * sizeof(GrPipelineDescriptorSlotInfo));
    for (unsigned i = 0, j = 0; i < src->descriptorCount; i++) {
        if (src->pDescriptorInfo[i].slotObjectType == GR_SLOT_UNUSED) {
            continue;
        }
        bool slotAllocated = copyDescriptorSlotInfo((GrPipelineDescriptorSlotInfo*)&dst->pDescriptorInfo[j],
                               pTotalDescriptorBindingCount,
                               &src->pDescriptorInfo[i],
                               i,
                               bindingCount,
                               bindings);
        if (slotAllocated) {
            j++;
        } else {
            dst->descriptorSlotCount--;
        }
    }
}

static void copyPipelineShader(
    GrPipelineShaderInfo* dst,
    const GR_PIPELINE_SHADER* src)
{
    const GrShader* grShader = (const GrShader*)src->shader;
    dst->dynamicMemoryViewMapping.slotObjectType = GR_SLOT_UNUSED;
    dst->atomicCounterInfo.atomicCounterBufferUsed = false;
    dst->hasShaderAttached = grShader != NULL;
    if (grShader == NULL) {
        return;
    }
    for (unsigned i = 0; i < COUNT_OF(dst->descriptorSetMapping); i++) {
        copyDescriptorSetMapping(&dst->descriptorSetMapping[i], &dst->descriptorBindingCount[i], &src->descriptorSetMapping[i], grShader->bindingCount, grShader->bindings);
    }
    dst->linkConstBufferCount = 0; // Ignored
    dst->pLinkConstBufferInfo = NULL; // Ignored

    for (unsigned i = 0; i < grShader->bindingCount; ++i) {
        const IlcBinding* binding = &grShader->bindings[i];
        if (binding->index == (ILC_BASE_RESOURCE_ID + src->dynamicMemoryViewMapping.shaderEntityIndex) &&
            src->dynamicMemoryViewMapping.slotObjectType != GR_SLOT_UNUSED) {
            // set dynamic memory type usage  only if shader uses it
            dst->dynamicMemoryViewMapping.slotObjectType = src->dynamicMemoryViewMapping.slotObjectType;
            dst->dynamicMemoryViewMapping.descriptorType = binding->descriptorType;
            dst->dynamicMemoryViewMapping.bindingIndex = binding->index;
            dst->dynamicMemoryViewMapping.strideIndex = binding->strideIndex;
        } else if (binding->index == ILC_ATOMIC_COUNTER_ID) {
            dst->atomicCounterInfo.bindingIndex = binding->index;
            dst->atomicCounterInfo.atomicCounterBufferUsed = true;
        }
    }
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
    const GrPipeline* grPipeline,
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    unsigned colorFormatCount,
    const VkFormat* colorFormats,
    VkFormat depthStencilFormat)
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

    const VkPipelineRenderingCreateInfoKHR renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext = NULL,
        .viewMask = 0,
        .colorAttachmentCount = colorFormatCount,
        .pColorAttachmentFormats = colorFormats,
        .depthAttachmentFormat = depthStencilFormat,
        .stencilAttachmentFormat = depthStencilFormat,
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCreateInfo,
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
        .renderPass = VK_NULL_HANDLE,
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
    const GrColorBlendStateObject* grColorBlendState,
    const GrMsaaStateObject* grMsaaState,
    const GrRasterStateObject* grRasterState,
    unsigned colorFormatCount,
    const VkFormat* colorFormats,
    VkFormat depthStencilFormat)
{
    VkPipeline vkPipeline = VK_NULL_HANDLE;

    AcquireSRWLockExclusive(&grPipeline->pipelineSlotsLock);

    for (unsigned i = 0; i < grPipeline->pipelineSlotCount; i++) {
        const PipelineSlot* slot = &grPipeline->pipelineSlots[i];

        if (grColorBlendState == slot->grColorBlendState &&
            grMsaaState == slot->grMsaaState &&
            grRasterState == slot->grRasterState &&
            colorFormatCount == slot->colorFormatCount &&
            !memcmp(colorFormats, slot->colorFormats, colorFormatCount * sizeof(VkFormat)) &&
            depthStencilFormat == slot->depthStencilFormat) {
            vkPipeline = slot->pipeline;
            break;
        }
    }

    if (vkPipeline == VK_NULL_HANDLE) {
        vkPipeline = getVkPipeline(grPipeline, grColorBlendState, grMsaaState, grRasterState,
                                   colorFormatCount, colorFormats, depthStencilFormat);

        PipelineSlot slot = {
            .pipeline = vkPipeline,
            .grColorBlendState = grColorBlendState,
            .grMsaaState = grMsaaState,
            .grRasterState = grRasterState,
            .colorFormatCount = colorFormatCount,
            .colorFormats = { 0 }, // Initialized below
            .depthStencilFormat = depthStencilFormat,
        };

        memcpy(slot.colorFormats, colorFormats, colorFormatCount * sizeof(VkFormat));

        grPipeline->pipelineSlotCount++;
        grPipeline->pipelineSlots = realloc(grPipeline->pipelineSlots,
                                            grPipeline->pipelineSlotCount * sizeof(PipelineSlot));
        grPipeline->pipelineSlots[grPipeline->pipelineSlotCount - 1] = slot;
    }

    ReleaseSRWLockExclusive(&grPipeline->pipelineSlotsLock);

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

    // ALLOW_RE_Z flag doesn't have a Vulkan equivalent. RADV determines it automatically.

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
        free(ilcShader.bindings);
        free(ilcShader.inputs);
        free(ilcShader.name);
        return getGrResult(res);
    }

    free(ilcShader.code);

    GrShader* grShader = malloc(sizeof(GrShader));
    *grShader = (GrShader) {
        .grObj = { GR_OBJ_TYPE_SHADER, grDevice },
        .shaderModule = vkShaderModule,
        .bindingCount = ilcShader.bindingCount,
        .bindings = ilcShader.bindings,
        .inputCount = ilcShader.inputCount,
        .inputs = ilcShader.inputs,
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
    VkShaderModule rectangleShaderModule = VK_NULL_HANDLE;
    unsigned dynamicOffsetCount = 0;
    VkResult vkRes;

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

    // Use a geometry shader to emulate RECT_LIST primitive topology
    if (pCreateInfo->iaState.topology == GR_TOPOLOGY_RECT_LIST) {
        if (stages[1].shader->shader != GR_NULL_HANDLE ||
            stages[2].shader->shader != GR_NULL_HANDLE ||
            stages[3].shader->shader != GR_NULL_HANDLE) {
            LOGE("unhandled RECT_LIST topology with predefined HS, DS or GS shaders\n");
            assert(false);
        }

        GrShader* grPixelShader = (GrShader*)stages[4].shader->shader;
        IlcShader rectangleShader = ilcCompileRectangleGeometryShader(
            grPixelShader != NULL ? grPixelShader->inputCount : 0,
            grPixelShader != NULL ? grPixelShader->inputs : NULL);

        const VkShaderModuleCreateInfo rectangleShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .codeSize = rectangleShader.codeSize,
            .pCode = rectangleShader.code,
        };

        vkRes = VKD.vkCreateShaderModule(grDevice->device, &rectangleShaderModuleCreateInfo, NULL,
                                         &rectangleShaderModule);
        free(rectangleShader.code);

        if (vkRes != VK_SUCCESS) {
            res = getGrResult(vkRes);
            goto bail;
        }

        shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = rectangleShaderModule,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;
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

    // TODO keep track of rectangle shader module
    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .createInfo = pipelineCreateInfo,
        .pipelineSlotCount = 0,
        .pipelineSlots = NULL,
        .pipelineSlotsLock = SRWLOCK_INIT,
        .pipelineLayout = pipelineLayout,
        .stageCount = COUNT_OF(stages),
        .descriptorSetLayouts = { 0 }, // Initialized below
        .shaderInfos = { { 0 } }, // Initialized below
        .dynamicOffsetCount = dynamicOffsetCount,
    };

    for (unsigned i = 0; i < COUNT_OF(stages); i++) {
        grPipeline->descriptorSetLayouts[i] = descriptorSetLayouts[i];
        copyPipelineShader(&grPipeline->shaderInfos[i], stages[i].shader);
    }

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    for (unsigned i = 0; i < COUNT_OF(descriptorSetLayouts); i++) {
        VKD.vkDestroyDescriptorSetLayout(grDevice->device, descriptorSetLayouts[i], NULL);
    }
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    VKD.vkDestroyShaderModule(grDevice->device, rectangleShaderModule, NULL);
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
        .pipelineSlotsLock = SRWLOCK_INIT,
        .pipelineLayout = pipelineLayout,
        .stageCount = 1,
        .descriptorSetLayouts = { descriptorSetLayout },
        .shaderInfos = { { 0 } }, // Initialized below
        .dynamicOffsetCount = dynamicOffsetCount,
    };

    copyPipelineShader(&grPipeline->shaderInfos[0], stage.shader);

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    VKD.vkDestroyDescriptorSetLayout(grDevice->device, descriptorSetLayout, NULL);
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    return res;
}

GR_RESULT GR_STDCALL grStorePipeline(
    GR_PIPELINE pipeline,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p %p %p\n", pipeline, pDataSize, pData);

    // TODO implement
    return GR_UNSUPPORTED;
}
