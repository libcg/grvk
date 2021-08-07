#include "mantle_internal.h"

// State Object Functions

GR_RESULT grCreateViewportState(
    GR_DEVICE device,
    const GR_VIEWPORT_STATE_CREATE_INFO* pCreateInfo,
    GR_VIEWPORT_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

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

GR_RESULT grCreateRasterState(
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

GR_RESULT grCreateColorBlendState(
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
        .states = { { 0 } }, // Initialized below
        .blendConstants = {
            pCreateInfo->blendConst[0], pCreateInfo->blendConst[1],
            pCreateInfo->blendConst[2], pCreateInfo->blendConst[3],
        },
    };

    for (unsigned i = 0; i < GR_MAX_COLOR_TARGETS; i++) {
        const GR_COLOR_TARGET_BLEND_STATE* blendState = &pCreateInfo->target[i];

        if (blendState->blendEnable) {
            grColorBlendStateObject->states[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = getVkBlendFactor(blendState->srcBlendColor),
                .dstColorBlendFactor = getVkBlendFactor(blendState->destBlendColor),
                .colorBlendOp = getVkBlendOp(blendState->blendFuncColor),
                .srcAlphaBlendFactor = getVkBlendFactor(blendState->srcBlendAlpha),
                .dstAlphaBlendFactor = getVkBlendFactor(blendState->destBlendAlpha),
                .alphaBlendOp = getVkBlendOp(blendState->blendFuncAlpha),
                .colorWriteMask = 0, // Defined at pipeline creation
            };
        } else {
            grColorBlendStateObject->states[i] = (VkPipelineColorBlendAttachmentState) {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = 0, // Ignored
                .dstColorBlendFactor = 0, // Ignored
                .colorBlendOp = 0, // Ignored
                .srcAlphaBlendFactor = 0, // Ignored
                .dstAlphaBlendFactor = 0, // Ignored
                .alphaBlendOp = 0, // Ignored
                .colorWriteMask = 0, // Defined at pipeline creation
            };
        }
    }

    *pState = (GR_COLOR_BLEND_STATE_OBJECT)grColorBlendStateObject;
    return GR_SUCCESS;
}

GR_RESULT grCreateDepthStencilState(
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

GR_RESULT grCreateMsaaState(
    GR_DEVICE device,
    const GR_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pState);
    GrDevice* grDevice = (GrDevice*)device;

    // TODO validate args

    if (pCreateInfo->samples != 1) {
        // TODO implement (don't forget samplingMask)
        LOGW("unsupported MSAA level %d\n", pCreateInfo->samples);
    }

    GrMsaaStateObject* grMsaaStateObject = malloc(sizeof(GrMsaaStateObject));
    *grMsaaStateObject = (GrMsaaStateObject) {
        .grObj = { GR_OBJ_TYPE_MSAA_STATE_OBJECT, grDevice },
    };

    *pState = (GR_MSAA_STATE_OBJECT)grMsaaStateObject;
    return GR_SUCCESS;
}
