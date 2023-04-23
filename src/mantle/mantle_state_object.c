#include "mantle_internal.h"

// State Object Functions

GR_RESULT GR_STDCALL grCreateViewportState(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pState == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pCreateInfo->viewportCount > GR_MAX_VIEWPORTS) {
        return GR_ERROR_INVALID_VALUE;
    }

    VkViewport *vkViewports = malloc(sizeof(VkViewport) * pCreateInfo->viewportCount);
    for (int i = 0; i < pCreateInfo->viewportCount; i++) {
        const GR_VIEWPORT* viewport = &pCreateInfo->viewports[i];

        if (viewport->originX < -32768.f || viewport->originX > 32768.f ||
            viewport->originY < -32768.f || viewport->originY > 32768.f ||
            viewport->width < 0.f || viewport->width > 32768.f ||
            viewport->height < 0.f || viewport->height > 32768.f ||
            viewport->minDepth < 0.f || viewport->minDepth > 1.f ||
            viewport->maxDepth < 0.f || viewport->maxDepth > 1.f ||
            viewport->minDepth >= viewport->maxDepth) {
            LOGW("invalid viewport parameters %g %g %g %g %g %g\n",
                 viewport->originX, viewport->originY, viewport->width, viewport->height,
                 viewport->minDepth, viewport->maxDepth);
            free(vkViewports);
            return GR_ERROR_INVALID_VALUE;
        }

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
            if (scissor->offset.x < 0.f || scissor->offset.x > 32768.f ||
                scissor->offset.y < 0.f || scissor->offset.y > 32768.f ||
                scissor->extent.width < 0.f || scissor->extent.width > 32768.f ||
                scissor->extent.height < 0.f || scissor->extent.height > 32768.f) {
                LOGW("invalid scissor parameters %g %g %g %g\n",
                     scissor->offset.x, scissor->offset.y,
                     scissor->extent.width, scissor->extent.height);
                free(vkViewports);
                free(vkScissors);
                return GR_ERROR_INVALID_VALUE;
            }

            vkScissors[i] = (VkRect2D) {
                .offset = { scissor->offset.x, scissor->offset.y },
                .extent = { scissor->extent.width, scissor->extent.height },
            };
        } else {
            vkScissors[i] = (VkRect2D) {
                .offset = { 0, 0 },
                .extent = { INT32_MAX, INT32_MAX },
            };
        }
    }

    GrViewportStateObject* grViewportStateObject = malloc(sizeof(GrViewportStateObject));

    *grViewportStateObject = (GrViewportStateObject) {
        .grObj = { GR_OBJ_TYPE_VIEWPORT_STATE_OBJECT, grDevice },
        .viewports = vkViewports,
        .viewportCount = pCreateInfo->viewportCount,
        .scissors = vkScissors,
        .scissorCount = pCreateInfo->viewportCount,
    };

    *pState = (GR_VIEWPORT_STATE_OBJECT)grViewportStateObject;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateRasterState(
    GR_DEVICE device,
    const GR_RASTER_STATE_CREATE_INFO* pCreateInfo,
    GR_RASTER_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

    GrRasterStateObject* grRasterStateObject = malloc(sizeof(GrRasterStateObject));

    *grRasterStateObject = (GrRasterStateObject) {
        .grObj = { GR_OBJ_TYPE_RASTER_STATE_OBJECT, grDevice },
        .polygonMode = getVkPolygonMode(pCreateInfo->fillMode),
        .cullMode = getVkCullModeFlags(pCreateInfo->cullMode),
        .frontFace = getVkFrontFace(pCreateInfo->frontFace),
        .depthBiasConstantFactor = (float)pCreateInfo->depthBias,
        .depthBiasClamp = pCreateInfo->depthBiasClamp,
        .depthBiasSlopeFactor = pCreateInfo->slopeScaledDepthBias,
    };

    *pState = (GR_RASTER_STATE_OBJECT)grRasterStateObject;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateColorBlendState(
    GR_DEVICE device,
    const GR_COLOR_BLEND_STATE_CREATE_INFO* pCreateInfo,
    GR_COLOR_BLEND_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

    GrColorBlendStateObject* grColorBlendStateObject =
        malloc(sizeof(GrColorBlendStateObject));

    *grColorBlendStateObject = (GrColorBlendStateObject) {
        .grObj = { GR_OBJ_TYPE_COLOR_BLEND_STATE_OBJECT, grDevice },
        .equationStates = { { 0 } }, // Initialized below
        .blendEnableStates = { false }, // Initialized below
        .blendConstants = {
            pCreateInfo->blendConst[0], pCreateInfo->blendConst[1],
            pCreateInfo->blendConst[2], pCreateInfo->blendConst[3],
        },
    };

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_COLOR_TARGET_BLEND_STATE* blendState = &pCreateInfo->target[i];

        grColorBlendStateObject->blendEnableStates[i] = blendState->blendEnable ? VK_TRUE : VK_FALSE;
        if (blendState->blendEnable) {
            grColorBlendStateObject->equationStates[i] = (VkColorBlendEquationEXT) {
                .srcColorBlendFactor = getVkBlendFactor(blendState->srcBlendColor),
                .dstColorBlendFactor = getVkBlendFactor(blendState->destBlendColor),
                .colorBlendOp = getVkBlendOp(blendState->blendFuncColor),
                .srcAlphaBlendFactor = getVkBlendFactor(blendState->srcBlendAlpha),
                .dstAlphaBlendFactor = getVkBlendFactor(blendState->destBlendAlpha),
                .alphaBlendOp = getVkBlendOp(blendState->blendFuncAlpha),
            };
        } else {
            grColorBlendStateObject->equationStates[i] = (VkColorBlendEquationEXT) {
                .srcColorBlendFactor = 0, // Ignored
                .dstColorBlendFactor = 0, // Ignored
                .colorBlendOp = 0, // Ignored
                .srcAlphaBlendFactor = 0, // Ignored
                .dstAlphaBlendFactor = 0, // Ignored
                .alphaBlendOp = 0, // Ignored
            };
        }
    }

    *pState = (GR_COLOR_BLEND_STATE_OBJECT)grColorBlendStateObject;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateDepthStencilState(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_STATE_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

    GrDepthStencilStateObject* grDepthStencilStateObject =
        malloc(sizeof(GrDepthStencilStateObject));

    *grDepthStencilStateObject = (GrDepthStencilStateObject) {
        .grObj = { GR_OBJ_TYPE_DEPTH_STENCIL_STATE_OBJECT, grDevice },
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

GR_RESULT GR_STDCALL grCreateMsaaState(
    GR_DEVICE device,
    const GR_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

    GrMsaaStateObject* grMsaaStateObject = malloc(sizeof(GrMsaaStateObject));
    *grMsaaStateObject = (GrMsaaStateObject) {
        .grObj = { GR_OBJ_TYPE_MSAA_STATE_OBJECT, grDevice },
        .sampleCountFlags = getVkSampleCountFlags(pCreateInfo->samples),
        .sampleMask = pCreateInfo->sampleMask,
    };

    *pState = (GR_MSAA_STATE_OBJECT)grMsaaStateObject;
    return GR_SUCCESS;
}
