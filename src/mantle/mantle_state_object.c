#include "mantle_internal.h"

// State Object Functions

GR_RESULT grCreateViewportState(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState)
{
    const uint32_t scissorCount = pCreateInfo->scissorEnable ? pCreateInfo->viewportCount : 0;

    VkViewport *vkViewports = malloc(sizeof(VkViewport) * pCreateInfo->viewportCount);
    for (int i = 0; i < pCreateInfo->viewportCount; i++) {
        const GR_VIEWPORT* viewport = &pCreateInfo->viewports[i];

        vkViewports[i] = (VkViewport) {
            .x = viewport->originX,
            .y = viewport->originY,
            .width = viewport->width,
            .height = viewport->height,
            .minDepth = viewport->minDepth,
            .maxDepth = viewport->maxDepth,
        };
    }

    VkRect2D *vkScissors = malloc(sizeof(VkViewport) * scissorCount);
    for (int i = 0; i < scissorCount; i++) {
        const GR_RECT* scissor = &pCreateInfo->scissors[i];

        vkScissors[i] = (VkRect2D) {
            .offset = {
                .x = scissor->offset.x,
                .y = scissor->offset.y,
            },
            .extent = {
                .width = scissor->extent.width,
                .height = scissor->extent.height,
            },
        };
    }

    GrvkViewportStateObject* grvkViewportStateObject = malloc(sizeof(GrvkViewportStateObject));
    *grvkViewportStateObject = (GrvkViewportStateObject) {
        .sType = GRVK_STRUCT_TYPE_VIEWPORT_STATE_OBJECT,
        .viewports = vkViewports,
        .viewportCount = pCreateInfo->viewportCount,
        .scissors = vkScissors,
        .scissorCount = scissorCount,
    };

    *pState = (GR_VIEWPORT_STATE_OBJECT)grvkViewportStateObject;
    return GR_SUCCESS;
}

GR_RESULT grCreateRasterState(
    GR_DEVICE device,
    const GR_RASTER_STATE_CREATE_INFO* pCreateInfo,
    GR_RASTER_STATE_OBJECT* pState)
{
    if (pCreateInfo->fillMode != GR_FILL_SOLID) {
        printf("%s: fill mode 0x%x is not supported\n", __func__, pCreateInfo->fillMode);
    }

    GrvkRasterStateObject* grvkRasterStateObject = malloc(sizeof(GrvkRasterStateObject));
    *grvkRasterStateObject = (GrvkRasterStateObject) {
        .sType = GRVK_STRUCT_TYPE_RASTER_STATE_OBJECT,
        .cullMode = getVkCullModeFlags(pCreateInfo->cullMode),
        .frontFace = getVkFrontFace(pCreateInfo->frontFace),
        .depthBiasConstantFactor = pCreateInfo->depthBias,
        .depthBiasClamp = pCreateInfo->depthBiasClamp,
        .depthBiasSlopeFactor = pCreateInfo->slopeScaledDepthBias,
    };

    *pState = (GR_RASTER_STATE_OBJECT)grvkRasterStateObject;
    return GR_SUCCESS;
}

GR_RESULT grCreateColorBlendState(
    GR_DEVICE device,
    const GR_COLOR_BLEND_STATE_CREATE_INFO* pCreateInfo,
    GR_COLOR_BLEND_STATE_OBJECT* pState)
{
    for (int i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_COLOR_TARGET_BLEND_STATE* target = &pCreateInfo->target[i];

        if (target->blendEnable &&
            (target->srcBlendColor != GR_BLEND_SRC_ALPHA ||
             target->destBlendColor != GR_BLEND_ONE_MINUS_SRC_ALPHA ||
             target->blendFuncColor != GR_BLEND_FUNC_ADD ||
             target->srcBlendAlpha != GR_BLEND_ONE ||
             target->destBlendAlpha != GR_BLEND_ONE ||
             target->blendFuncAlpha != GR_BLEND_FUNC_ADD)) {
            printf("%s: unsupported blend settings 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", __func__,
                   target->srcBlendColor, target->destBlendColor, target->blendFuncColor,
                   target->srcBlendAlpha, target->destBlendAlpha, target->blendFuncAlpha);
        }
    }

    GrvkColorBlendStateObject* grvkColorBlendStateObject =
        malloc(sizeof(GrvkColorBlendStateObject));
    *grvkColorBlendStateObject = (GrvkColorBlendStateObject) {
        .sType = GRVK_STRUCT_TYPE_COLOR_BLEND_STATE_OBJECT,
        .blendConstants = {
            pCreateInfo->blendConst[0], pCreateInfo->blendConst[1],
            pCreateInfo->blendConst[2], pCreateInfo->blendConst[3],
        },
    };

    *pState = (GR_COLOR_BLEND_STATE_OBJECT)grvkColorBlendStateObject;
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

    GrvkDepthStencilStateObject* grvkDepthStencilStateObject =
        malloc(sizeof(GrvkDepthStencilStateObject));
    *grvkDepthStencilStateObject = (GrvkDepthStencilStateObject) {
        .sType = GRVK_STRUCT_TYPE_DEPTH_STENCIL_STATE_OBJECT,
        .depthStencilStateCreateInfo = depthStencilStateCreateInfo,
    };

    *pState = (GR_DEPTH_STENCIL_STATE_OBJECT)grvkDepthStencilStateObject;
    return GR_SUCCESS;
}

GR_RESULT grCreateMsaaState(
    GR_DEVICE device,
    const GR_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    if (pCreateInfo->samples != 1) {
        // TODO implement (don't forget samplingMask)
        printf("%s: unsupported MSAA level %d\n", __func__, pCreateInfo->samples);
    }

    GrvkMsaaStateObject* grvkMsaaStateObject = malloc(sizeof(GrvkMsaaStateObject));
    *grvkMsaaStateObject = (GrvkMsaaStateObject) {
        .sType = GRVK_STRUCT_TYPE_MSAA_STATE_OBJECT,
    };

    *pState = (GR_MSAA_STATE_OBJECT)grvkMsaaStateObject;
    return GR_SUCCESS;
}
