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
    GR_IMAGE_STATE imageState)
{
    switch (imageState) {
    case GR_IMAGE_STATE_UNINITIALIZED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case GR_IMAGE_STATE_TARGET_RENDER_ACCESS_OPTIMAL:
        return VK_IMAGE_LAYOUT_GENERAL;
    case GR_IMAGE_STATE_CLEAR:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
        break;
    }

    switch ((GR_WSI_WIN_IMAGE_STATE)imageState) {
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_WINDOWED:
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_FULLSCREEN:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    printf("%s: unsupported image state 0x%x\n", __func__, imageState);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkAccessFlags getVkAccessFlagsImage(
    GR_IMAGE_STATE imageState)
{
    switch (imageState) {
    case GR_IMAGE_STATE_UNINITIALIZED:
        return 0;
    case GR_IMAGE_STATE_TARGET_RENDER_ACCESS_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case GR_IMAGE_STATE_CLEAR:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    default:
        break;
    }

    switch ((GR_WSI_WIN_IMAGE_STATE)imageState) {
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_WINDOWED:
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_FULLSCREEN:
        return VK_ACCESS_MEMORY_READ_BIT;
    }

    printf("%s: unsupported image state 0x%x\n", __func__, imageState);
    return 0;
}

VkAccessFlags getVkAccessFlagsMemory(
    GR_MEMORY_STATE memoryState)
{
    switch (memoryState) {
    case GR_MEMORY_STATE_DATA_TRANSFER:
        return VK_ACCESS_TRANSFER_READ_BIT |
               VK_ACCESS_TRANSFER_WRITE_BIT |
               VK_ACCESS_HOST_READ_BIT |
               VK_ACCESS_HOST_WRITE_BIT;
    case GR_MEMORY_STATE_GRAPHICS_SHADER_READ_ONLY:
    case GR_MEMORY_STATE_COMPUTE_SHADER_READ_ONLY:
        return VK_ACCESS_SHADER_READ_BIT;
    case GR_MEMORY_STATE_GRAPHICS_SHADER_WRITE_ONLY:
    case GR_MEMORY_STATE_COMPUTE_SHADER_WRITE_ONLY:
        return VK_ACCESS_SHADER_WRITE_BIT;
    case GR_MEMORY_STATE_GRAPHICS_SHADER_READ_WRITE:
    case GR_MEMORY_STATE_COMPUTE_SHADER_READ_WRITE:
        return VK_ACCESS_SHADER_READ_BIT |
               VK_ACCESS_SHADER_WRITE_BIT;
    default:
        break;
    }

    printf("%s: unsupported memory state 0x%x\n", __func__, memoryState);
    return 0;
}

VkImageAspectFlags getVkImageAspectFlags(
    GR_IMAGE_ASPECT imageAspect)
{
    switch (imageAspect) {
    case GR_IMAGE_ASPECT_COLOR:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case GR_IMAGE_ASPECT_DEPTH:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case GR_IMAGE_ASPECT_STENCIL:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    printf("%s: unsupported image aspect 0x%x\n", __func__, imageAspect);
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
    GR_BLEND blend)
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

    printf("%s: unsupported blend factor 0x%x\n", __func__, blend);
    return VK_BLEND_FACTOR_ZERO;
}

VkBlendOp getVkBlendOp(
    GR_BLEND_FUNC blendFunc)
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

    printf("%s: unsupported blend func 0x%x\n", __func__, blendFunc);
    return VK_BLEND_OP_ADD;
}

VkCompareOp getVkCompareOp(
    GR_COMPARE_FUNC compareFunc)
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

    printf("%s: unsupported compare func 0x%x\n", __func__, compareFunc);
    return VK_COMPARE_OP_NEVER;
}

VkStencilOp getVkStencilOp(
    GR_STENCIL_OP stencilOp)
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

    printf("%s: unsupported stencil op 0x%x\n", __func__, stencilOp);
    return VK_STENCIL_OP_KEEP;
}

VkPolygonMode getVkPolygonMode(
    GR_FILL_MODE fillMode)
{
    switch (fillMode) {
    case GR_FILL_SOLID:
        return VK_POLYGON_MODE_FILL;
    case GR_FILL_WIREFRAME:
        return VK_POLYGON_MODE_LINE;
    }

    printf("%s: unsupported fill mode 0x%x\n", __func__, fillMode);
    return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags getVkCullModeFlags(
    GR_CULL_MODE cullMode)
{
    switch (cullMode) {
    case GR_CULL_NONE:
        return VK_CULL_MODE_NONE;
    case GR_CULL_FRONT:
        return VK_CULL_MODE_FRONT_BIT;
    case GR_CULL_BACK:
        return VK_CULL_MODE_BACK_BIT;
    }

    printf("%s: unsupported cull mode 0x%x\n", __func__, cullMode);
    return VK_CULL_MODE_NONE;
}

VkFrontFace getVkFrontFace(
    GR_FACE_ORIENTATION frontFace)
{
    switch (frontFace) {
    case GR_FRONT_FACE_CCW:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case GR_FRONT_FACE_CW:
        return VK_FRONT_FACE_CLOCKWISE;
    }

    printf("%s: unsupported face orientation 0x%x\n", __func__, frontFace);
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

VkPrimitiveTopology getVkPrimitiveTopology(
    GR_PRIMITIVE_TOPOLOGY topology)
{
    switch (topology) {
    case GR_TOPOLOGY_POINT_LIST:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case GR_TOPOLOGY_LINE_LIST:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GR_TOPOLOGY_LINE_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case GR_TOPOLOGY_TRIANGLE_LIST:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GR_TOPOLOGY_TRIANGLE_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GR_TOPOLOGY_RECT_LIST:
        break; // TODO implement using VK_NV_fill_rectangle
    case GR_TOPOLOGY_QUAD_LIST:
    case GR_TOPOLOGY_QUAD_STRIP:
        break; // Unsupported
    case GR_TOPOLOGY_LINE_LIST_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case GR_TOPOLOGY_LINE_STRIP_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case GR_TOPOLOGY_TRIANGLE_LIST_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case GR_TOPOLOGY_TRIANGLE_STRIP_ADJ:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case GR_TOPOLOGY_PATCH:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }

    printf("%s: unsupported topology 0x%x\n", __func__, topology);
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

VkDescriptorType getVkDescriptorType(
    GR_DESCRIPTOR_SET_SLOT_TYPE slotObjectType)
{
    switch (slotObjectType) {
    case GR_SLOT_UNUSED:
        return 0; // Ignored
    case GR_SLOT_SHADER_RESOURCE:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case GR_SLOT_SHADER_UAV:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case GR_SLOT_SHADER_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case GR_SLOT_NEXT_DESCRIPTOR_SET:
        break; // Invalid
    }

    printf("%s: unsupported slot object type 0x%x\n", __func__, slotObjectType);
    return VK_DESCRIPTOR_TYPE_SAMPLER;
}

VkColorComponentFlags getVkColorComponentFlags(
    GR_UINT8 writeMask)
{
    return (writeMask & 1 ? VK_COLOR_COMPONENT_R_BIT : 0) |
           (writeMask & 2 ? VK_COLOR_COMPONENT_G_BIT : 0) |
           (writeMask & 4 ? VK_COLOR_COMPONENT_B_BIT : 0) |
           (writeMask & 8 ? VK_COLOR_COMPONENT_A_BIT : 0);
}

VkShaderStageFlags getVkShaderStageFlags(
    uint32_t stageIndex)
{
    switch (stageIndex) {
    case 0:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case 1:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case 2:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case 3:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case 4:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    printf("%s: unsupported stage index %d\n", __func__, stageIndex);
    return 0;
}
