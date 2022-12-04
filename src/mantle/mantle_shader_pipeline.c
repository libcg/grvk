#include "mantle_internal.h"
#include "amdilc.h"

typedef struct _Stage {
    const GR_PIPELINE_SHADER* shader;
    const VkShaderStageFlagBits flags;
} Stage;

static bool handleDynamicDescriptorSlots(
    PipelineDescriptorSlot* descriptorSlot,
    const GR_DYNAMIC_MEMORY_VIEW_SLOT_INFO* dynamicMapping,
    unsigned bindingCount,
    const IlcBinding* bindings,
    uint32_t* offsets,
    uint32_t* descriptorSetIndices,
    IlcBindingPatchEntry* patchEntries)
{
    for (unsigned i = 0; i < bindingCount; i++) {
        const IlcBinding* binding = &bindings[i];

        if (dynamicMapping->slotObjectType != GR_SLOT_UNUSED &&
            binding->ilIndex == dynamicMapping->shaderEntityIndex &&
            binding->type == ILC_BINDING_RESOURCE) {
            *descriptorSlot = (PipelineDescriptorSlot) {
                .pathDepth = 0,
                .path = { 0 },
                .strideCount = 0, // Initialized below
                .strideOffsets = { 0 }, // Initialized below
                .strideSlotIndexes = { 0 }, // Initialized below
            };

            if (binding->strideIndex >= 0) {
                unsigned strideOffset = binding->strideIndex * sizeof(uint32_t);
                descriptorSlot->strideCount++;
                descriptorSlot->strideOffsets[descriptorSlot->strideCount - 1] = strideOffset;
                descriptorSlot->strideSlotIndexes[descriptorSlot->strideCount - 1] = 0;
            }

            offsets[i] = 0;
            unsigned int descriptorSetIndex = DYNAMIC_MEMORY_VIEW_DESCRIPTOR_SET_ID;
            descriptorSetIndices[i] = descriptorSetIndex;
            patchEntries[i] = (IlcBindingPatchEntry) {
                .id = binding->id,
                .bindingIndex = DYNAMIC_MEMORY_VIEW_BINDING_ID,
                .descriptorSetIndex = descriptorSetIndex,
            };

            return true;
        }
    }

    return false;
}

static void getDescriptorSlotsFromMapping(
    unsigned* pDescriptorSlotCount,
    PipelineDescriptorSlot** pDescriptorSlots,
    const GR_DESCRIPTOR_SET_MAPPING* mapping,
    unsigned bindingCount,
    const IlcBinding* bindings,
    uint32_t* offsets,
    IlcBindingPatchEntry* patchEntries,
    unsigned pathDepth,
    unsigned* path)
{
    for (unsigned i = 0; i < mapping->descriptorCount; i++) {
        const GR_DESCRIPTOR_SLOT_INFO* slotInfo = &mapping->pDescriptorInfo[i];
        const IlcBinding* binding = NULL;

        if (slotInfo->slotObjectType == GR_SLOT_UNUSED) {
            continue;
        } else if (slotInfo->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
            if (pathDepth >= MAX_PATH_DEPTH) {
                LOGE("exceeded max path depth of %d\n", MAX_PATH_DEPTH);
                assert(false);
            }

            // Mark path
            path[pathDepth] = i;

            // Add slots from the nested set
            getDescriptorSlotsFromMapping(pDescriptorSlotCount, pDescriptorSlots,
                                          slotInfo->pNextLevelSet, bindingCount, bindings,
                                          offsets, patchEntries,
                                          pathDepth + 1, path);
            continue;
        }

        // Find matching binding
        for (unsigned j = 0; j < bindingCount; j++) {
            if (bindings[j].ilIndex == slotInfo->shaderEntityIndex &&
                ((bindings[j].type == ILC_BINDING_SAMPLER &&
                  slotInfo->slotObjectType == GR_SLOT_SHADER_SAMPLER) ||
                 (bindings[j].type == ILC_BINDING_RESOURCE &&
                  (slotInfo->slotObjectType == GR_SLOT_SHADER_RESOURCE ||
                   slotInfo->slotObjectType == GR_SLOT_SHADER_UAV)))) {
                binding = &bindings[j];

                uint32_t descriptorTypeOffset = getDescriptorOffset(bindings[j].descriptorType);
                offsets[j] = i * DESCRIPTORS_PER_SLOT + descriptorTypeOffset;

                break;
            }
        }

        if (binding == NULL) {
            // Unused mapping slot, skip
            continue;
        }

        (*pDescriptorSlotCount)++;
        *pDescriptorSlots = realloc(*pDescriptorSlots,
                                       *pDescriptorSlotCount * sizeof(PipelineDescriptorSlot));
        (*pDescriptorSlots)[*pDescriptorSlotCount - 1] = (PipelineDescriptorSlot) {
            .pathDepth = pathDepth,
            .path = { 0 }, // Initialized below
            .strideCount = 0, // Initialized below
            .strideOffsets = { 0 }, // Initialized below
            .strideSlotIndexes = { 0 }, // Initialized below
        };

        memcpy((*pDescriptorSlots)[*pDescriptorSlotCount - 1].path,
               path, pathDepth * sizeof(unsigned));

        if (binding->strideIndex >= 0) {
            unsigned strideOffset = binding->strideIndex * sizeof(uint32_t);
            (*pDescriptorSlots)[*pDescriptorSlotCount - 1].strideCount = 1;
            (*pDescriptorSlots)[*pDescriptorSlotCount - 1].strideOffsets[0] = strideOffset;
            (*pDescriptorSlots)[*pDescriptorSlotCount - 1].strideSlotIndexes[0] = i;
        }
    }
}

static int compareDescriptorSlots(
    const void* a,
    const void* b)
{
    const PipelineDescriptorSlot* slotA = a;
    const PipelineDescriptorSlot* slotB = b;

    return memcmp(slotA->path, slotB->path, slotA->pathDepth * sizeof(slotA->path[0]));
}


static void mergeDescriptorSlots(
    unsigned* descriptorSlotCount,
    PipelineDescriptorSlot** descriptorSlots)
{
    // Group slots by path
    qsort(*descriptorSlots, *descriptorSlotCount, sizeof(PipelineDescriptorSlot),
          compareDescriptorSlots);

    unsigned mergingDescriptorCount = 0;

    for (unsigned i = 0; i < *descriptorSlotCount; i++) {
        bool isLastSlot = (i + 1) == *descriptorSlotCount;
        PipelineDescriptorSlot* slot = &(*descriptorSlots)[i];
        PipelineDescriptorSlot* nextSlot = &(*descriptorSlots)[i + 1];

        mergingDescriptorCount++;

        if (!isLastSlot &&
            slot->pathDepth == nextSlot->pathDepth &&
            memcmp(slot->path, nextSlot->path, slot->pathDepth * sizeof(slot->path[0])) == 0) {
            // Can't merge yet
            continue;
        }

        unsigned mergedIdx = i - mergingDescriptorCount + 1;
        PipelineDescriptorSlot* mergedSlot = &(*descriptorSlots)[mergedIdx];

        // TODO deduplicate strides
        for (unsigned j = mergedIdx + 1; j <= i; j++) {
            PipelineDescriptorSlot* slotToMerge = &(*descriptorSlots)[j];

            if (slotToMerge->strideCount == 1) {
                if (mergedSlot->strideCount >= MAX_STRIDES) {
                    LOGE("exceeded max strides of %d\n", MAX_STRIDES);
                    assert(false);
                }

                mergedSlot->strideCount++;
                mergedSlot->strideOffsets[mergedSlot->strideCount - 1] =
                    slotToMerge->strideOffsets[0];
                mergedSlot->strideSlotIndexes[mergedSlot->strideCount - 1] =
                    slotToMerge->strideSlotIndexes[0];
            }
        }

        // Drop temporary slots
        memmove(mergedSlot + 1, nextSlot,
                (*descriptorSlotCount - i - 1) * sizeof(PipelineDescriptorSlot));
        *descriptorSlotCount -= mergingDescriptorCount - 1;
        *descriptorSlots = realloc(*descriptorSlots,
                                   *descriptorSlotCount * sizeof(PipelineDescriptorSlot));

        // Update state
        i = mergedIdx;
        mergingDescriptorCount = 0;
    }
}

static void setupDescriptorSetIndices(
    unsigned descriptorSetCount,
    const PipelineDescriptorSlot* descriptorSlots,
    const GR_DESCRIPTOR_SET_MAPPING* mapping,
    unsigned bindingCount,
    const IlcBinding* bindings,
    IlcBindingPatchEntry* patchEntries,
    unsigned* descriptorSetIndices,
    unsigned descriptorSetIndexOffset,
    unsigned pathDepth,
    unsigned* path)
{
    unsigned descriptorSetIndex = 0xFFFFFFFF;
    for (unsigned i = 0; i < descriptorSetCount; ++i) {
        const PipelineDescriptorSlot* slot = (const PipelineDescriptorSlot*)(&descriptorSlots[i]);
        if (slot->pathDepth == pathDepth && memcmp(slot->path, path, pathDepth * sizeof(path[0])) == 0) {
            descriptorSetIndex = i;
            break;
        }
    }
    for (unsigned i = 0; i < mapping->descriptorCount; i++) {
        const GR_DESCRIPTOR_SLOT_INFO* slotInfo = &mapping->pDescriptorInfo[i];

        if (slotInfo->slotObjectType == GR_SLOT_UNUSED) {
            continue;
        } else if (slotInfo->slotObjectType == GR_SLOT_NEXT_DESCRIPTOR_SET) {
            if (pathDepth >= MAX_PATH_DEPTH) {
                LOGE("exceeded max path depth of %d\n", MAX_PATH_DEPTH);
                assert(false);
            }

            // Mark path
            path[pathDepth] = i;

            // Add slots from the nested set
            setupDescriptorSetIndices(descriptorSetCount, descriptorSlots,
                                      slotInfo->pNextLevelSet, bindingCount, bindings,
                                      patchEntries, descriptorSetIndices,
                                      descriptorSetIndexOffset,
                                      pathDepth + 1, path);
            continue;
        }
        // Find matching binding
        for (unsigned j = 0; j < bindingCount; j++) {
            if (bindings[j].ilIndex == slotInfo->shaderEntityIndex &&
                ((bindings[j].type == ILC_BINDING_SAMPLER &&
                  slotInfo->slotObjectType == GR_SLOT_SHADER_SAMPLER) ||
                 (bindings[j].type == ILC_BINDING_RESOURCE &&
                  (slotInfo->slotObjectType == GR_SLOT_SHADER_RESOURCE ||
                   slotInfo->slotObjectType == GR_SLOT_SHADER_UAV)))) {
                unsigned computedDescriptorSetIndex = descriptorSetIndexOffset + descriptorSetIndex;
                descriptorSetIndices[j] = computedDescriptorSetIndex;
                patchEntries[j] = (IlcBindingPatchEntry) {
                    .id = bindings[j].id,
                    .bindingIndex = 0,
                    .descriptorSetIndex = computedDescriptorSetIndex,
                };
                break;
            }
        }
    }
}

static void getDescriptorSlotMappings(
    unsigned* descriptorSlotCount,
    PipelineDescriptorSlot** descriptorSlots,
    const GrDevice* grDevice,
    unsigned stageCount,
    const Stage* stages,
    IlcBindingPatchEntry** patchEntries,
    uint32_t** specOffsets,
    uint32_t** specDescriptorIndices,
    unsigned mappingIndex,
    unsigned descriptorSetIndexOffset)
{
    for (unsigned i = 0; i < stageCount; i++) {
        const Stage* stage = &stages[i];
        const GR_PIPELINE_SHADER* shader = stage->shader;
        const GrShader* grShader = shader->shader;
        unsigned path[MAX_PATH_DEPTH];

        if (grShader == NULL) {
            continue;
        }

        getDescriptorSlotsFromMapping(descriptorSlotCount, descriptorSlots,
                                      &shader->descriptorSetMapping[mappingIndex],
                                      grShader->bindingCount, grShader->bindings,
                                      specOffsets[i], patchEntries[i],
                                      0, path);
    }

    mergeDescriptorSlots(descriptorSlotCount, descriptorSlots);
    for (unsigned i = 0; i < stageCount; i++) {
        const Stage* stage = &stages[i];
        const GR_PIPELINE_SHADER* shader = stage->shader;
        const GrShader* grShader = shader->shader;
        unsigned path[MAX_PATH_DEPTH];

        if (grShader == NULL) {
            continue;
        }

        setupDescriptorSetIndices(*descriptorSlotCount, *descriptorSlots,
                                  &shader->descriptorSetMapping[mappingIndex],
                                  grShader->bindingCount, grShader->bindings,
                                  patchEntries[i],
                                  specDescriptorIndices[i],
                                  descriptorSetIndexOffset,
                                  0, path);
    }
}

static VkPipelineLayout getVkPipelineLayout(
    const GrDevice* grDevice,
    unsigned descriptorSetCount,
    VkPipelineBindPoint vkBindPoint)
{
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout setLayouts[32] = {
        grDevice->dynamicMemorySetLayout,
        grDevice->atomicCounterSetLayout,
    };

    assert((descriptorSetCount + 2) <= COUNT_OF(setLayouts));
    for (unsigned i = 0; i < descriptorSetCount; ++i) {
        setLayouts[i + 2] = grDevice->defaultDescriptorSetLayout;
    }
    const VkPushConstantRange pushConstantRanges[] = {
        {
            .stageFlags = (vkBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) ? VK_SHADER_STAGE_ALL_GRAPHICS : VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = DESCRIPTOR_OFFSET_COUNT * sizeof(uint32_t) + ILC_MAX_STRIDE_CONSTANTS * sizeof(uint32_t),
        }
    };

    const VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = descriptorSetCount + 2,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = COUNT_OF(pushConstantRanges),
        .pPushConstantRanges = pushConstantRanges,
    };

    VkResult res = VKD.vkCreatePipelineLayout(grDevice->device, &createInfo, NULL, &pipelineLayout);
    if (res != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed (%d)\n", res);
    }

    return pipelineLayout;
}

// Exported Functions

VkPipeline grPipelineGetVkPipeline(
    const GrPipeline* grPipeline,
    VkFormat depthFormat,
    VkFormat stencilFormat)
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
        .polygonMode = 0, // Dynamic state
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
        .rasterizationSamples = 1, // Dynamic state
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = NULL, // Dynamic state
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

    VkPipelineColorBlendAttachmentState attachments[GR_MAX_COLOR_TARGETS];

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        attachments[i] = (VkPipelineColorBlendAttachmentState) {
            .blendEnable = false, // Dynamic state
            .srcColorBlendFactor = 0, // Dynamic state
            .dstColorBlendFactor = 0, // Dynamic state
            .colorBlendOp = 0, // Dynamic state
            .srcAlphaBlendFactor = 0, // Dynamic state
            .dstAlphaBlendFactor = 0, // Dynamic state
            .alphaBlendOp = 0, // Dynamic state
            .colorWriteMask = createInfo->colorWriteMasks[i],
        };
    }

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = createInfo->logicOpEnable,
        .logicOp = createInfo->logicOp,
        .attachmentCount = GR_MAX_COLOR_TARGETS,
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
        VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
        VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
        VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
        VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
    };

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = COUNT_OF(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    if (depthFormat != createInfo->depthFormat ||
        stencilFormat != createInfo->stencilFormat) {
        LOGD("depth-stencil attachment format mismatch, got %d %d, expected %d %d\n",
             depthFormat, stencilFormat, createInfo->depthFormat, createInfo->stencilFormat);
    }

    const VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = NULL,
        .viewMask = 0,
        .colorAttachmentCount = GR_MAX_COLOR_TARGETS,
        .pColorAttachmentFormats = createInfo->colorFormats,
        .depthAttachmentFormat = depthFormat,
        .stencilAttachmentFormat = stencilFormat,
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

// Shader and Pipeline Functions

GR_RESULT GR_STDCALL grCreateShader(
    GR_DEVICE device,
    const GR_SHADER_CREATE_INFO* pCreateInfo,
    GR_SHADER* pShader)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pShader);
    GrDevice* grDevice = (GrDevice*)device;

    // ALLOW_RE_Z flag doesn't have a Vulkan equivalent. RADV determines it automatically.

    IlcShader ilcShader = ilcCompileShader(pCreateInfo->pCode, pCreateInfo->codeSize);

    GrShader* grShader = malloc(sizeof(GrShader));
    *grShader = (GrShader) {
        .grObj = { GR_OBJ_TYPE_SHADER, grDevice },
        .bindingCount = ilcShader.bindingCount,
        .bindings = ilcShader.bindings,
        .inputCount = ilcShader.inputCount,
        .inputs = ilcShader.inputs,
        .name = ilcShader.name,
        .codeSize = ilcShader.codeSize,
        .code = ilcShader.code,
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
    bool hasTessellation = false;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkShaderModule shaderModules[MAX_STAGE_COUNT] = { 0 };
    IlcBindingPatchEntry* patchEntries[MAX_STAGE_COUNT] = { NULL };
    uint32_t* specData[MAX_STAGE_COUNT] = { NULL };
    uint32_t* descriptorSetIndices[MAX_STAGE_COUNT] = { NULL };
    VkSpecializationMapEntry* mapEntries[MAX_STAGE_COUNT] = { NULL };
    VkSpecializationInfo specInfos[MAX_STAGE_COUNT] = { { 0 } };

    PipelineDescriptorSlot dynamicDescriptorSlot = { 0 };
    unsigned descriptorSetCounts[GR_MAX_DESCRIPTOR_SETS] = { 0 };
    PipelineDescriptorSlot* pipelineDescriptorSlots[GR_MAX_DESCRIPTOR_SETS] = { NULL };

    VkResult vkRes;

    // TODO validate parameters

    // Ignored parameters:
    // - cbState.dualSourceBlendEnable (Vulkan handles it dynamically)
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

    bool dynamicMappingUsed = false;
    for (int i = 0; i < COUNT_OF(stages); i++) {
        Stage* stage = &stages[i];

        if (stage->shader->shader == GR_NULL_HANDLE) {
            continue;
        }

        GrShader* grShader = (GrShader*)stage->shader->shader;

        patchEntries[i] = malloc(sizeof(IlcBindingPatchEntry) * grShader->bindingCount);
        mapEntries[i] = malloc(grShader->bindingCount * 2 * sizeof(VkSpecializationMapEntry));
        specData[i] = malloc(sizeof(uint32_t) * 2 * grShader->bindingCount);
        specInfos[i] = (VkSpecializationInfo) {
            .pData = specData[i],
            .pMapEntries = mapEntries[i],
            .dataSize = sizeof(uint32_t) * grShader->bindingCount * 2,
            .mapEntryCount = grShader->bindingCount * 2,
        };
        descriptorSetIndices[i] = &specData[i][grShader->bindingCount];
        for (unsigned j = 0; j < grShader->bindingCount; ++j) {
            mapEntries[i][j * 2] = (VkSpecializationMapEntry) {
                .constantID = grShader->bindings[j].offsetSpecId,
                .offset = j * sizeof(uint32_t),
                .size = sizeof(uint32_t),
            };
            mapEntries[i][j * 2 + 1] = (VkSpecializationMapEntry) {
                .constantID = grShader->bindings[j].descriptorSetIndexSpecId,
                .offset = (j + grShader->bindingCount) * sizeof(uint32_t),
                .size = sizeof(uint32_t),
            };
        }

        dynamicMappingUsed |= handleDynamicDescriptorSlots(
            &dynamicDescriptorSlot,
            &stage->shader->dynamicMemoryViewMapping,
            grShader->bindingCount, grShader->bindings,
            specData[i],
            &specData[i][grShader->bindingCount],
            patchEntries[i]);
    }

    unsigned descriptorSetCount = 0;
    for (unsigned i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        getDescriptorSlotMappings(&descriptorSetCounts[i], &pipelineDescriptorSlots[i],
                                  grDevice, COUNT_OF(stages), stages, patchEntries, specData, descriptorSetIndices, i, descriptorSetCount + DESCRIPTOR_SET_ID);
        descriptorSetCount += descriptorSetCounts[i];
    }

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

        void* code = malloc(grShader->codeSize);
        memcpy(code, grShader->code, grShader->codeSize);

        const VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .codeSize = grShader->codeSize,
            .pCode = code,//grShader->code,
        };

        patchShaderBindings(
            code,
            grShader->codeSize,
            patchEntries[i],
            grShader->bindingCount);

        vkRes = VKD.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &shaderModules[stageCount]);
        free(code);
        if (vkRes != VK_SUCCESS) {
            res = getGrResult(vkRes);
            goto bail;
        }

        if (stageCount != i) {
            patchEntries[stageCount] = patchEntries[i];
            mapEntries[stageCount] = mapEntries[i];
            specData[stageCount] = specData[i];

            patchEntries[i] = NULL;
            mapEntries[i] = NULL;
            specData[i] = NULL;
            memcpy(&specInfos[stageCount], &specInfos[i], sizeof(VkSpecializationInfo));
        }

        shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = stage->flags,
            .module = shaderModules[stageCount],
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;

        if (stage->flags == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
            stage->flags == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
            hasTessellation = true;
        }
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
                                         &shaderModules[stageCount]);
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
            .module = shaderModules[stageCount],
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        stageCount++;
    }

    VkFormat colorFormats[GR_MAX_COLOR_TARGETS];
    VkColorComponentFlags colorWriteMasks[GR_MAX_COLOR_TARGETS];

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_PIPELINE_CB_TARGET_STATE* target = &pCreateInfo->cbState.target[i];

        colorFormats[i] = getVkFormat(target->format);
        colorWriteMasks[i] = getVkColorComponentFlags(target->channelWriteMask);
    }

    PipelineCreateInfo* pipelineCreateInfo = malloc(sizeof(PipelineCreateInfo));
    *pipelineCreateInfo = (PipelineCreateInfo) {
        .createFlags = (pCreateInfo->flags & GR_PIPELINE_CREATE_DISABLE_OPTIMIZATION) != 0 ?
                       VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT : 0,
        .stageCount = stageCount,
        .stageCreateInfos = { { 0 } }, // Initialized below
        .specInfos = { { 0 } }, // Initialized below
        .specData = { NULL }, // Initialized below
        .mapEntries = { NULL }, // Initialized below
        .topology = getVkPrimitiveTopology(pCreateInfo->iaState.topology),
        .patchControlPoints = pCreateInfo->tessState.patchControlPoints,
        .depthClipEnable = !!pCreateInfo->rsState.depthClipEnable,
        .alphaToCoverageEnable = !!pCreateInfo->cbState.alphaToCoverageEnable,
        .logicOpEnable = pCreateInfo->cbState.logicOp != GR_LOGIC_OP_COPY,
        .logicOp = getVkLogicOp(pCreateInfo->cbState.logicOp),
        .colorFormats = { 0 }, // Initialized below
        .colorWriteMasks = { 0 }, // Initialized below
        .depthFormat = getDepthVkFormat(pCreateInfo->dbState.format),
        .stencilFormat = getStencilVkFormat(pCreateInfo->dbState.format),
    };

    memcpy(pipelineCreateInfo->stageCreateInfos, shaderStageCreateInfo,
           stageCount * sizeof(VkPipelineShaderStageCreateInfo));
    memcpy(pipelineCreateInfo->specInfos, specInfos, sizeof(specInfos));
    memcpy(pipelineCreateInfo->specData, specData, sizeof(specData));
    memcpy(pipelineCreateInfo->mapEntries, mapEntries, sizeof(mapEntries));
    memcpy(pipelineCreateInfo->colorFormats, colorFormats,
           GR_MAX_COLOR_TARGETS * sizeof(VkFormat));
    memcpy(pipelineCreateInfo->colorWriteMasks, colorWriteMasks,
           GR_MAX_COLOR_TARGETS * sizeof(VkColorComponentFlags));

    for (unsigned i = 0; i < MAX_STAGE_COUNT; i++) {
        pipelineCreateInfo->stageCreateInfos[i].pSpecializationInfo = &pipelineCreateInfo->specInfos[i];
    }

    descriptorSetCount = 0;
    for (unsigned i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        descriptorSetCount += descriptorSetCounts[i];
    }
    pipelineLayout = getVkPipelineLayout(grDevice, descriptorSetCount, VK_PIPELINE_BIND_POINT_GRAPHICS);
    if (pipelineLayout == VK_NULL_HANDLE) {
        res = GR_ERROR_OUT_OF_MEMORY;
        goto bail;
    }
    // TODO keep track of rectangle shader module
    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .shaderModules = { VK_NULL_HANDLE },
        .createInfo = pipelineCreateInfo,
        .hasTessellation = hasTessellation,
        .pipeline = VK_NULL_HANDLE, // We don't know the attachment formats yet (Frostbite bug)
        .pipelineLayout = pipelineLayout,
        .stageCount = COUNT_OF(stages),
        .dynamicMappingUsed = dynamicMappingUsed,
        .dynamicDescriptorSlot = dynamicDescriptorSlot,
        .descriptorSetCounts = { 0 }, // Initialized below
        .descriptorSlots = { NULL }, // Initialized below
    };

    memcpy(grPipeline->shaderModules, shaderModules, sizeof(grPipeline->shaderModules));

    memcpy(grPipeline->descriptorSetCounts, descriptorSetCounts,
           sizeof(grPipeline->descriptorSetCounts));
    memcpy(grPipeline->descriptorSlots, pipelineDescriptorSlots,
           sizeof(grPipeline->descriptorSlots));

    for (uint32_t i = 0; i < MAX_STAGE_COUNT; i++) {
        free(patchEntries[i]);
    }

    *pPipeline = (GR_PIPELINE)grPipeline;

    return GR_SUCCESS;

bail:
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    for (uint32_t i = 0; i < MAX_STAGE_COUNT; i++) {
        VKD.vkDestroyShaderModule(grDevice->device, shaderModules[i], NULL);
        free(patchEntries[i]);
        free(specData[i]);
        free(mapEntries[i]);
    }
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
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkPipeline vkPipeline = VK_NULL_HANDLE;

    uint32_t* specData = NULL;
    VkSpecializationMapEntry* mapEntries = NULL;
    VkSpecializationInfo specInfo = { 0 };
    IlcBindingPatchEntry* patchEntries = NULL;
    uint32_t* descriptorOffsets = NULL;
    uint32_t* descriptorSetIndices = NULL;

    PipelineDescriptorSlot dynamicDescriptorSlot = { 0 };
    unsigned descriptorSetCounts[GR_MAX_DESCRIPTOR_SETS] = { 0 };
    PipelineDescriptorSlot* pipelineDescriptorSlots[GR_MAX_DESCRIPTOR_SETS] = { NULL };

    // TODO validate parameters

    Stage stage = { &pCreateInfo->cs, VK_SHADER_STAGE_COMPUTE_BIT };

    if (stage.shader->linkConstBufferCount > 0) {
        // TODO implement
        LOGW("link-time constant buffers are not implemented\n");
    }
    GrShader* grShader = (GrShader*)stage.shader->shader;

    patchEntries = malloc(sizeof(IlcBindingPatchEntry) * grShader->bindingCount);
    mapEntries = malloc(grShader->bindingCount * 2 * sizeof(VkSpecializationMapEntry));
    specData = malloc(sizeof(uint32_t) * 2 * grShader->bindingCount);
    specInfo = (VkSpecializationInfo) {
        .pData = specData,
        .pMapEntries = mapEntries,
        .dataSize = sizeof(uint32_t) * grShader->bindingCount * 2,
        .mapEntryCount = grShader->bindingCount * 2,
    };
    descriptorOffsets = specData;
    descriptorSetIndices = &specData[grShader->bindingCount];

    for (unsigned j = 0; j < grShader->bindingCount; ++j) {
        mapEntries[j * 2] = (VkSpecializationMapEntry) {
            .constantID = grShader->bindings[j].offsetSpecId,
            .offset = j * sizeof(uint32_t),
            .size = sizeof(uint32_t)
        };
        mapEntries[j * 2 + 1] = (VkSpecializationMapEntry) {
            .constantID = grShader->bindings[j].descriptorSetIndexSpecId,
            .offset = (j + grShader->bindingCount) * sizeof(uint32_t),
            .size = sizeof(uint32_t)
        };
    }
    bool dynamicMappingUsed = handleDynamicDescriptorSlots(
        &dynamicDescriptorSlot,
        &stage.shader->dynamicMemoryViewMapping,
        grShader->bindingCount, grShader->bindings,
        specData,
        &specData[grShader->bindingCount],
        patchEntries);

    unsigned descriptorSetCount = 0;
    for (unsigned i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        getDescriptorSlotMappings(&descriptorSetCounts[i], &pipelineDescriptorSlots[i],
                                  grDevice, 1, &stage, &patchEntries, &descriptorOffsets, &descriptorSetIndices, i, descriptorSetCount + DESCRIPTOR_SET_ID);
        descriptorSetCount += descriptorSetCounts[i];
    }

    void* code = malloc(grShader->codeSize);
    memcpy(code, grShader->code, grShader->codeSize);

    const VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = grShader->codeSize,
        .pCode = code,//grShader->code,
    };

    patchShaderBindings(
        code,
        grShader->codeSize,
        patchEntries,
        grShader->bindingCount);

    vkRes = VKD.vkCreateShaderModule(grDevice->device, &createInfo, NULL, &shaderModule);
    free(code);
    if (vkRes != VK_SUCCESS) {
        res = getGrResult(vkRes);
        goto bail;
    }

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = stage.flags,
        .module = shaderModule,
        .pName = "main",
        .pSpecializationInfo = &specInfo,
    };

    descriptorSetCount = 0;
    for (unsigned i = 0; i < GR_MAX_DESCRIPTOR_SETS; i++) {
        descriptorSetCount += descriptorSetCounts[i];
    }
    pipelineLayout = getVkPipelineLayout(grDevice, descriptorSetCount, VK_PIPELINE_BIND_POINT_COMPUTE);
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

    GrPipeline* grPipeline = malloc(sizeof(GrPipeline));
    *grPipeline = (GrPipeline) {
        .grObj = { GR_OBJ_TYPE_PIPELINE, grDevice },
        .shaderModules = { shaderModule },
        .createInfo = NULL,
        .hasTessellation = false,
        .pipeline = vkPipeline,
        .pipelineLayout = pipelineLayout,
        .stageCount = 1,
        .dynamicMappingUsed = dynamicMappingUsed,
        .dynamicDescriptorSlot = dynamicDescriptorSlot,
        .descriptorSetCounts = { 0 }, // Initialized below
        .descriptorSlots = { NULL }, // Initialized below
    };

    memcpy(grPipeline->descriptorSetCounts, descriptorSetCounts,
           sizeof(grPipeline->descriptorSetCounts));
    memcpy(grPipeline->descriptorSlots, pipelineDescriptorSlots,
           sizeof(grPipeline->descriptorSlots));

    free(specData);
    free(mapEntries);
    free(patchEntries);

    *pPipeline = (GR_PIPELINE)grPipeline;
    return GR_SUCCESS;

bail:
    free(patchEntries);
    free(specData);
    free(mapEntries);
    VKD.vkDestroyPipelineLayout(grDevice->device, pipelineLayout, NULL);
    VKD.vkDestroyShaderModule(grDevice->device, shaderModule, NULL);
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
