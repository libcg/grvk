#include "mantle_internal.h"

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
