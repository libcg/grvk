#include "mantle_internal.h"

// State Object Functions

GR_RESULT grCreateViewportState(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState)
{
    VkViewport *vkViewports = malloc(sizeof(VkViewport) * pCreateInfo->viewportCount);
    for (int i = 0; i < pCreateInfo->viewportCount; i++) {
        const GR_VIEWPORT* viewport = &pCreateInfo->viewports[i];

        vkViewports[i] = (VkViewport) {
            .x = viewport->originX,
            .y = viewport->height - viewport->originY,
            .width = viewport->width,
            .height = -viewport->height,
            .minDepth = viewport->minDepth,
            .maxDepth = viewport->maxDepth,
        };
    }

    VkRect2D *vkScissors = malloc(sizeof(VkViewport) * pCreateInfo->viewportCount);
    for (int i = 0; i < pCreateInfo->viewportCount; i++) {
        const GR_RECT* scissor = &pCreateInfo->scissors[i];

        if (pCreateInfo->scissorEnable) {
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
        } else {
            vkScissors[i] = (VkRect2D) {
                .offset = {
                    .x = INT32_MIN,
                    .y = INT32_MIN,
                },
                .extent = {
                    .width = UINT32_MAX,
                    .height = UINT32_MAX,
                },
            };
        }
    }

    GrViewportStateObject* grViewportStateObject = malloc(sizeof(GrViewportStateObject));

    *grViewportStateObject = (GrViewportStateObject) {
        .sType = GR_STRUCT_TYPE_VIEWPORT_STATE_OBJECT,
        .viewports = vkViewports,
        .viewportCount = pCreateInfo->viewportCount,
        .scissors = vkScissors,
        .scissorCount = pCreateInfo->viewportCount,
    };

    *pState = (GR_VIEWPORT_STATE_OBJECT)grViewportStateObject;
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

    GrRasterStateObject* grRasterStateObject = malloc(sizeof(GrRasterStateObject));

    *grRasterStateObject = (GrRasterStateObject) {
        .sType = GR_STRUCT_TYPE_RASTER_STATE_OBJECT,
        .cullMode = getVkCullModeFlags(pCreateInfo->cullMode),
        .frontFace = getVkFrontFace(pCreateInfo->frontFace),
        .depthBiasConstantFactor = pCreateInfo->depthBias,
        .depthBiasClamp = pCreateInfo->depthBiasClamp,
        .depthBiasSlopeFactor = pCreateInfo->slopeScaledDepthBias,
    };

    *pState = (GR_RASTER_STATE_OBJECT)grRasterStateObject;
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

    GrColorBlendStateObject* grColorBlendStateObject =
        malloc(sizeof(GrColorBlendStateObject));

    *grColorBlendStateObject = (GrColorBlendStateObject) {
        .sType = GR_STRUCT_TYPE_COLOR_BLEND_STATE_OBJECT,
        .blendConstants = {
            pCreateInfo->blendConst[0], pCreateInfo->blendConst[1],
            pCreateInfo->blendConst[2], pCreateInfo->blendConst[3],
        },
    };

    *pState = (GR_COLOR_BLEND_STATE_OBJECT)grColorBlendStateObject;
    return GR_SUCCESS;
}

GR_RESULT grCreateDepthStencilState(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_STATE_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_STATE_OBJECT* pState)
{
    GrDepthStencilStateObject* grDepthStencilStateObject =
        malloc(sizeof(GrDepthStencilStateObject));

    *grDepthStencilStateObject = (GrDepthStencilStateObject) {
        .sType = GR_STRUCT_TYPE_DEPTH_STENCIL_STATE_OBJECT,
        .depthTestEnable = pCreateInfo->depthEnable,
        .depthWriteEnable = pCreateInfo->depthWriteEnable,
        .depthCompareOp = getVkCompareOp(pCreateInfo->depthFunc),
        .depthBoundsTestEnable = pCreateInfo->depthBoundsEnable,
        .stencilTestEnable = pCreateInfo->stencilEnable,
        .front = {
            .failOp = getVkStencilOp(pCreateInfo->front.stencilFailOp),
            .passOp = getVkStencilOp(pCreateInfo->front.stencilPassOp),
            .depthFailOp = getVkStencilOp(pCreateInfo->front.stencilDepthFailOp),
            .compareOp = getVkCompareOp(pCreateInfo->front.stencilFunc),
            .compareMask = pCreateInfo->stencilReadMask,
            .writeMask = pCreateInfo->stencilWriteMask,
            .reference = pCreateInfo->front.stencilRef,
        },
        .back = {
            .failOp = getVkStencilOp(pCreateInfo->back.stencilFailOp),
            .passOp = getVkStencilOp(pCreateInfo->back.stencilPassOp),
            .depthFailOp = getVkStencilOp(pCreateInfo->back.stencilDepthFailOp),
            .compareOp = getVkCompareOp(pCreateInfo->back.stencilFunc),
            .compareMask = pCreateInfo->stencilReadMask,
            .writeMask = pCreateInfo->stencilWriteMask,
            .reference = pCreateInfo->back.stencilRef,
        },
        .minDepthBounds = pCreateInfo->minDepth,
        .maxDepthBounds = pCreateInfo->maxDepth,
    };

    *pState = (GR_DEPTH_STENCIL_STATE_OBJECT)grDepthStencilStateObject;
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

    GrMsaaStateObject* grMsaaStateObject = malloc(sizeof(GrMsaaStateObject));
    *grMsaaStateObject = (GrMsaaStateObject) {
        .sType = GR_STRUCT_TYPE_MSAA_STATE_OBJECT,
    };

    *pState = (GR_MSAA_STATE_OBJECT)grMsaaStateObject;
    return GR_SUCCESS;
}
