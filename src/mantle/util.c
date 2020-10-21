#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"

#define PACK_FORMAT(channel, numeric) \
    ((channel) << 16 | (numeric))

GR_PHYSICAL_GPU_TYPE getGrPhysicalGpuType(
    VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return GR_GPU_TYPE_OTHER;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return GR_GPU_TYPE_INTEGRATED;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return GR_GPU_TYPE_DISCRETE;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return GR_GPU_TYPE_VIRTUAL;
    default:
        break;
    }

    return GR_GPU_TYPE_OTHER;
}

VkFormat getVkFormat(
    GR_FORMAT format)
{
    uint32_t packed = PACK_FORMAT(format.channelFormat, format.numericFormat);

    // Table 23 in the Mantle API reference
    // Packed Vulkan formats have swapped components (higher bits go first)
    switch (packed) {
    case PACK_FORMAT(GR_CH_FMT_UNDEFINED, GR_NUM_FMT_UNDEFINED):
        return VK_FORMAT_UNDEFINED;
    case PACK_FORMAT(GR_CH_FMT_R4G4, GR_NUM_FMT_UNORM):
        break; // FIXME VK_FORMAT_G4R4_UNORM_PACK8 isn't available
    case PACK_FORMAT(GR_CH_FMT_R4G4B4A4, GR_NUM_FMT_UNORM):
        break; // FIXME VK_FORMAT_A4B4G4R4_UNORM_PACK16 isn't available
    case PACK_FORMAT(GR_CH_FMT_R5G6B5, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case PACK_FORMAT(GR_CH_FMT_B5G6R5, GR_NUM_FMT_UNORM):
        return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case PACK_FORMAT(GR_CH_FMT_B5G6R5, GR_NUM_FMT_SRGB):
        break; // FIXME VK_FORMAT_R5G6B5_SRGB_PACK16 isn't available
    case PACK_FORMAT(GR_CH_FMT_R5G5B5A1, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R8_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R8_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_UINT):
        return VK_FORMAT_R8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_SINT):
        return VK_FORMAT_R8_SINT;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_SRGB):
        return VK_FORMAT_R8_SRGB;
    case PACK_FORMAT(GR_CH_FMT_R8, GR_NUM_FMT_DS):
        return VK_FORMAT_S8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R8G8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R8G8, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R8G8_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R8G8, GR_NUM_FMT_UINT):
        return VK_FORMAT_R8G8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R8G8, GR_NUM_FMT_SINT):
        return VK_FORMAT_R8G8_SINT;
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_UINT):
        return VK_FORMAT_R8G8B8A8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_SINT):
        return VK_FORMAT_R8G8B8A8_SINT;
    case PACK_FORMAT(GR_CH_FMT_R8G8B8A8, GR_NUM_FMT_SRGB):
        return VK_FORMAT_R8G8B8A8_SRGB;
    case PACK_FORMAT(GR_CH_FMT_B8G8R8A8, GR_NUM_FMT_UNORM):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case PACK_FORMAT(GR_CH_FMT_B8G8R8A8, GR_NUM_FMT_SRGB):
        return VK_FORMAT_B8G8R8A8_SRGB;
    case PACK_FORMAT(GR_CH_FMT_R10G11B11, GR_NUM_FMT_FLOAT):
        break; // FIXME VK_FORMAT_B11G11R10_UFLOAT_PACK32 isn't available
    case PACK_FORMAT(GR_CH_FMT_R11G11B10, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case PACK_FORMAT(GR_CH_FMT_R10G10B10A2, GR_NUM_FMT_UNORM):
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case PACK_FORMAT(GR_CH_FMT_R10G10B10A2, GR_NUM_FMT_UINT):
        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R16_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R16_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_UINT):
        return VK_FORMAT_R16_UINT;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_SINT):
        return VK_FORMAT_R16_SINT;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R16, GR_NUM_FMT_DS):
        return VK_FORMAT_D16_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R16G16, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R16G16_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R16G16, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R16G16_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R16G16, GR_NUM_FMT_UINT):
        return VK_FORMAT_R16G16_UINT;
    case PACK_FORMAT(GR_CH_FMT_R16G16, GR_NUM_FMT_SINT):
        return VK_FORMAT_R16G16_SINT;
    case PACK_FORMAT(GR_CH_FMT_R16G16, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R16G16B16A16, GR_NUM_FMT_UNORM):
        return VK_FORMAT_R16G16B16A16_UNORM;
    case PACK_FORMAT(GR_CH_FMT_R16G16B16A16, GR_NUM_FMT_SNORM):
        return VK_FORMAT_R16G16B16A16_SNORM;
    case PACK_FORMAT(GR_CH_FMT_R16G16B16A16, GR_NUM_FMT_UINT):
        return VK_FORMAT_R16G16B16A16_UINT;
    case PACK_FORMAT(GR_CH_FMT_R16G16B16A16, GR_NUM_FMT_SINT):
        return VK_FORMAT_R16G16B16A16_SINT;
    case PACK_FORMAT(GR_CH_FMT_R16G16B16A16, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R32, GR_NUM_FMT_UINT):
        return VK_FORMAT_R32_UINT;
    case PACK_FORMAT(GR_CH_FMT_R32, GR_NUM_FMT_SINT):
        return VK_FORMAT_R32_SINT;
    case PACK_FORMAT(GR_CH_FMT_R32, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R32_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R32, GR_NUM_FMT_DS):
        return VK_FORMAT_D32_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R32G32, GR_NUM_FMT_UINT):
        return VK_FORMAT_R32G32_UINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32, GR_NUM_FMT_SINT):
        return VK_FORMAT_R32G32_SINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32, GR_NUM_FMT_UINT):
        return VK_FORMAT_R32G32B32_UINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32, GR_NUM_FMT_SINT):
        return VK_FORMAT_R32G32B32_SINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32A32, GR_NUM_FMT_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32A32, GR_NUM_FMT_SINT):
        return VK_FORMAT_R32G32B32A32_SINT;
    case PACK_FORMAT(GR_CH_FMT_R32G32B32A32, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case PACK_FORMAT(GR_CH_FMT_R16G8, GR_NUM_FMT_DS):
        return VK_FORMAT_D16_UNORM_S8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R32G8, GR_NUM_FMT_DS):
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case PACK_FORMAT(GR_CH_FMT_R9G9B9E5, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    case PACK_FORMAT(GR_CH_FMT_BC1, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK; // XX: maybe RGBA?
    case PACK_FORMAT(GR_CH_FMT_BC1, GR_NUM_FMT_SRGB):
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK; // XX: maybe RGBA?
    case PACK_FORMAT(GR_CH_FMT_BC2, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC2, GR_NUM_FMT_SRGB):
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC3, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC3, GR_NUM_FMT_SRGB):
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC4, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC4, GR_NUM_FMT_SNORM):
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC5, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC5, GR_NUM_FMT_SNORM):
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC6U, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC6S, GR_NUM_FMT_FLOAT):
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC7, GR_NUM_FMT_UNORM):
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case PACK_FORMAT(GR_CH_FMT_BC7, GR_NUM_FMT_SRGB):
        return VK_FORMAT_BC7_SRGB_BLOCK;
    }

    LOGW("unsupported format %u %u\n", format.channelFormat, format.numericFormat);
    return VK_FORMAT_UNDEFINED;
}

VkImageLayout getVkImageLayout(
    GR_IMAGE_STATE imageState)
{
    switch (imageState) {
    case GR_IMAGE_STATE_UNINITIALIZED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case GR_IMAGE_STATE_TARGET_RENDER_ACCESS_OPTIMAL:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case GR_IMAGE_STATE_CLEAR:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
        break;
    }

    switch ((GR_WSI_WIN_IMAGE_STATE)imageState) {
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_WINDOWED:
    case GR_WSI_WIN_IMAGE_STATE_PRESENT_FULLSCREEN:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    LOGW("unsupported image state 0x%x\n", imageState);
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
        return VK_ACCESS_TRANSFER_READ_BIT;
    }

    LOGW("unsupported image state 0x%x\n", imageState);
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

    LOGW("unsupported memory state 0x%x\n", memoryState);
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

    LOGW("unsupported image aspect 0x%x\n", imageAspect);
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

    LOGW("unsupported sample count %d\n", samples);
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

    LOGW("unsupported blend factor 0x%x\n", blend);
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

    LOGW("unsupported blend func 0x%x\n", blendFunc);
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

    LOGW("unsupported compare func 0x%x\n", compareFunc);
    return VK_COMPARE_OP_NEVER;
}

VkLogicOp getVkLogicOp(
    GR_LOGIC_OP logicOp)
{
    switch (logicOp) {
    case GR_LOGIC_OP_COPY:
        return VK_LOGIC_OP_COPY;
    case GR_LOGIC_OP_CLEAR:
        return VK_LOGIC_OP_CLEAR;
    case GR_LOGIC_OP_AND:
        return VK_LOGIC_OP_AND;
    case GR_LOGIC_OP_AND_REVERSE:
        return VK_LOGIC_OP_AND_REVERSE;
    case GR_LOGIC_OP_AND_INVERTED:
        return VK_LOGIC_OP_AND_INVERTED;
    case GR_LOGIC_OP_NOOP:
        return VK_LOGIC_OP_NO_OP;
    case GR_LOGIC_OP_XOR:
        return VK_LOGIC_OP_XOR;
    case GR_LOGIC_OP_OR:
        return VK_LOGIC_OP_OR;
    case GR_LOGIC_OP_NOR:
        return VK_LOGIC_OP_NOR;
    case GR_LOGIC_OP_EQUIV:
        return VK_LOGIC_OP_EQUIVALENT;
    case GR_LOGIC_OP_INVERT:
        return VK_LOGIC_OP_INVERT;
    case GR_LOGIC_OP_OR_REVERSE:
        return VK_LOGIC_OP_OR_REVERSE;
    case GR_LOGIC_OP_COPY_INVERTED:
        return VK_LOGIC_OP_COPY_INVERTED;
    case GR_LOGIC_OP_OR_INVERTED:
        return VK_LOGIC_OP_OR_INVERTED;
    case GR_LOGIC_OP_NAND:
        return VK_LOGIC_OP_NAND;
    case GR_LOGIC_OP_SET:
        return VK_LOGIC_OP_SET;
    }

    LOGW("unsupported logic op 0x%x\n", logicOp);
    return VK_LOGIC_OP_COPY;
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

    LOGW("unsupported stencil op 0x%x\n", stencilOp);
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

    LOGW("unsupported fill mode 0x%x\n", fillMode);
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

    LOGW("unsupported cull mode 0x%x\n", cullMode);
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

    LOGW("unsupported face orientation 0x%x\n", frontFace);
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

    LOGW("unsupported topology 0x%x\n", topology);
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

VkDescriptorType getVkDescriptorType(
    GR_DESCRIPTOR_SET_SLOT_TYPE slotObjectType)
{
    switch (slotObjectType) {
    case GR_SLOT_UNUSED:
        return 0; // Ignored
    case GR_SLOT_SHADER_RESOURCE:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case GR_SLOT_SHADER_UAV:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case GR_SLOT_SHADER_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case GR_SLOT_NEXT_DESCRIPTOR_SET:
        break; // Invalid
    }

    LOGW("unsupported slot object type 0x%x\n", slotObjectType);
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

    LOGW("unsupported stage index %d\n", stageIndex);
    return 0;
}

VkPipelineBindPoint getVkPipelineBindPoint(
    GR_PIPELINE_BIND_POINT bindPoint)
{
    switch (bindPoint) {
    case GR_PIPELINE_BIND_POINT_COMPUTE:
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    case GR_PIPELINE_BIND_POINT_GRAPHICS:
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    LOGW("unsupported pipeline bind point 0x%x\n", bindPoint);
    return GR_PIPELINE_BIND_POINT_GRAPHICS;
}

VkFilter getVkFilterMag(
    GR_TEX_FILTER filter)
{
    switch (filter) {
    case GR_TEX_FILTER_MAG_POINT_MIN_POINT_MIP_POINT:
    case GR_TEX_FILTER_MAG_POINT_MIN_LINEAR_MIP_POINT:
    case GR_TEX_FILTER_MAG_POINT_MIN_POINT_MIP_LINEAR:
    case GR_TEX_FILTER_MAG_POINT_MIN_LINEAR_MIP_LINEAR:
        return VK_FILTER_NEAREST;
    case GR_TEX_FILTER_MAG_LINEAR_MIN_POINT_MIP_POINT:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_LINEAR_MIP_POINT:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_POINT_MIP_LINEAR:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_LINEAR_MIP_LINEAR:
    case GR_TEX_FILTER_ANISOTROPIC:
        return VK_FILTER_LINEAR;
    }

    LOGW("unsupported mag filter 0x%X\n", filter);
    return VK_FILTER_NEAREST;
}

VkFilter getVkFilterMin(
    GR_TEX_FILTER filter)
{
    switch (filter) {
    case GR_TEX_FILTER_MAG_POINT_MIN_POINT_MIP_POINT:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_POINT_MIP_POINT:
    case GR_TEX_FILTER_MAG_POINT_MIN_POINT_MIP_LINEAR:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_POINT_MIP_LINEAR:
        return VK_FILTER_NEAREST;
    case GR_TEX_FILTER_MAG_POINT_MIN_LINEAR_MIP_POINT:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_LINEAR_MIP_POINT:
    case GR_TEX_FILTER_MAG_POINT_MIN_LINEAR_MIP_LINEAR:
    case GR_TEX_FILTER_MAG_LINEAR_MIN_LINEAR_MIP_LINEAR:
    case GR_TEX_FILTER_ANISOTROPIC:
        return VK_FILTER_LINEAR;
    }

    LOGW("unsupported min filter 0x%X\n", filter);
    return VK_FILTER_NEAREST;
}

VkSamplerAddressMode getVkSamplerAddressMode(
    GR_TEX_ADDRESS texAddress)
{
    switch (texAddress) {
    case GR_TEX_ADDRESS_WRAP:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case GR_TEX_ADDRESS_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case GR_TEX_ADDRESS_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case GR_TEX_ADDRESS_MIRROR_ONCE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    case GR_TEX_ADDRESS_CLAMP_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }

    LOGW("unsupported tex address 0x%X\n", texAddress);
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkBorderColor getVkBorderColor(
    GR_BORDER_COLOR_TYPE borderColorType)
{
    switch (borderColorType) {
    case GR_BORDER_COLOR_WHITE:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case GR_BORDER_COLOR_TRANSPARENT_BLACK:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case GR_BORDER_COLOR_OPAQUE_BLACK:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }

    LOGW("unsupported border color type 0x%X\n", borderColorType);
    return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
}
