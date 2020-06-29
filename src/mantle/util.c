#include <stdio.h>
#include <stdlib.h>
#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"

#define PACK_FORMAT(channel, numeric) \
    ((channel) << 16 | (numeric))

GR_VOID* grvkAlloc(
    GR_SIZE size,
    GR_SIZE alignment,
    GR_ENUM allocType)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#endif
}

GR_VOID grvkFree(
    GR_VOID* pMem)
{
    free(pMem);
}

VkFormat getVkFormat(
    GR_FORMAT format)
{
    uint32_t packed = PACK_FORMAT(format.channelFormat, format.numericFormat);

    switch (packed) {
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    printf("%s: unsupported format %u %u\n", __func__, format.channelFormat, format.numericFormat);
    return VK_FORMAT_UNDEFINED;
}

VkImageLayout getVkImageLayout(
    GR_ENUM imageState)
{
    switch (imageState) {
    case GR_IMAGE_STATE_UNINITIALIZED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case GR_IMAGE_STATE_TARGET_RENDER_ACCESS_OPTIMAL:
        return VK_IMAGE_LAYOUT_GENERAL;
    case GR_IMAGE_STATE_CLEAR:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_WINDOWED:
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_FULLSCREEN:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    printf("%s: unsupported image state %d\n", __func__, imageState);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkAccessFlags getVkAccessFlags(
    GR_ENUM imageState)
{
    switch (imageState) {
    case GR_IMAGE_STATE_UNINITIALIZED:
        return 0;
    case GR_IMAGE_STATE_TARGET_RENDER_ACCESS_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case GR_IMAGE_STATE_CLEAR:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_WINDOWED:
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_FULLSCREEN:
        return VK_ACCESS_MEMORY_READ_BIT;
    }

    printf("%s: unsupported image state %d\n", __func__, imageState);
    return 0;
}

VkImageAspectFlags getVkImageAspectFlags(
    GR_ENUM imageAspect)
{
    switch (imageAspect) {
    case GR_IMAGE_ASPECT_COLOR:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case GR_IMAGE_ASPECT_DEPTH:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case GR_IMAGE_ASPECT_STENCIL:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    printf("%s: unsupported image aspect %d\n", __func__, imageAspect);
    return 0;
}

VkSampleCountFlagBits getVkSampleCountFlagBits(
    GR_UINT samples)
{
    switch (samples) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    }

    printf("%s: unsupported sample count %d\n", __func__, samples);
    return VK_SAMPLE_COUNT_1_BIT;
}

VkBlendFactor getVkBlendFactor(
    GR_ENUM blend)
{
    switch (blend) {
    case GR_BLEND_ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case GR_BLEND_ONE:
        return VK_BLEND_FACTOR_ONE;
    case GR_BLEND_SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case GR_BLEND_ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GR_BLEND_DEST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case GR_BLEND_ONE_MINUS_DEST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GR_BLEND_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GR_BLEND_DEST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case GR_BLEND_ONE_MINUS_DEST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GR_BLEND_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case GR_BLEND_ONE_MINUS_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case GR_BLEND_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case GR_BLEND_ONE_MINUS_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case GR_BLEND_SRC_ALPHA_SATURATE:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case GR_BLEND_SRC1_COLOR:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case GR_BLEND_ONE_MINUS_SRC1_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case GR_BLEND_SRC1_ALPHA:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case GR_BLEND_ONE_MINUS_SRC1_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }

    printf("%s: unsupported blend factor %d\n", __func__, blend);
    return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp getVkBlendOp(
    GR_ENUM blendFunc)
{
    switch (blendFunc) {
    case GR_BLEND_FUNC_ADD:
        return VK_BLEND_OP_ADD;
    case GR_BLEND_FUNC_SUBTRACT:
        return VK_BLEND_OP_SUBTRACT;
    case GR_BLEND_FUNC_REVERSE_SUBTRACT:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case GR_BLEND_FUNC_MIN:
        return VK_BLEND_OP_MIN;
    case GR_BLEND_FUNC_MAX:
        return VK_BLEND_OP_MAX;
    }

    printf("%s: unsupported blend func %d\n", __func__, blendFunc);
    return VK_BLEND_OP_ADD;
}

VkCompareOp getVkCompareOp(
    GR_ENUM compareFunc)
{
    switch (compareFunc) {
    case GR_COMPARE_NEVER:
        return VK_COMPARE_OP_NEVER;
    case GR_COMPARE_LESS:
        return VK_COMPARE_OP_LESS;
    case GR_COMPARE_EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case GR_COMPARE_LESS_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case GR_COMPARE_GREATER:
        return VK_COMPARE_OP_GREATER;
    case GR_COMPARE_NOT_EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case GR_COMPARE_GREATER_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case GR_COMPARE_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
    }

    printf("%s: unsupported compare func %d\n", __func__, compareFunc);
    return VK_COMPARE_OP_NEVER;
}

VkStencilOp getVkStencilOp(
    GR_ENUM stencilOp)
{
    switch (stencilOp) {
    case GR_STENCIL_OP_KEEP:
        return VK_STENCIL_OP_KEEP;
    case GR_STENCIL_OP_ZERO:
        return VK_STENCIL_OP_ZERO;
    case GR_STENCIL_OP_REPLACE:
        return VK_STENCIL_OP_REPLACE;
    case GR_STENCIL_OP_INC_CLAMP:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case GR_STENCIL_OP_DEC_CLAMP:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case GR_STENCIL_OP_INVERT:
        return VK_STENCIL_OP_INVERT;
    case GR_STENCIL_OP_INC_WRAP:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case GR_STENCIL_OP_DEC_WRAP:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }

    printf("%s: unsupported stencil op %d\n", __func__, stencilOp);
    return VK_STENCIL_OP_KEEP;
}

VkPolygonMode getVkPolygonMode(
    GR_ENUM fillMode)
{
    switch (fillMode) {
    case GR_FILL_SOLID:
        return VK_POLYGON_MODE_FILL;
    case GR_FILL_WIREFRAME:
        return VK_POLYGON_MODE_LINE;
    }

    printf("%s: unsupported fill mode %d\n", __func__, fillMode);
    return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags getVkCullModeFlags(
    GR_ENUM cullMode)
{
    switch (cullMode) {
    case GR_CULL_NONE:
        return VK_CULL_MODE_NONE;
    case GR_CULL_FRONT:
        return VK_CULL_MODE_FRONT_BIT;
    case GR_CULL_BACK:
        return VK_CULL_MODE_BACK_BIT;
    }

    printf("%s: unsupported cull mode %d\n", __func__, cullMode);
    return VK_CULL_MODE_NONE;
}

VkFrontFace getVkFrontFace(
    GR_ENUM frontFace)
{
    switch (frontFace) {
    case GR_FRONT_FACE_CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case GR_FRONT_FACE_CW:
        return VK_FRONT_FACE_CLOCKWISE;
    }

    printf("%s: unsupported face orientation %d\n", __func__, frontFace);
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

uint32_t getVkQueueFamilyIndex(
    GR_ENUM queueType)
{
    // FIXME this will break
    return queueType - GR_QUEUE_UNIVERSAL;
}
