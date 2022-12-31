#ifndef MANTLE_INTERNAL_H_
#define MANTLE_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mantle/mantle.h"
#include "mantle/mantleExt.h"
#include "mantle/mantleWsiWinExt.h"
#include "logger.h"
#include "mantle_object.h"
#include "quirk.h"
#include "version.h"
#include "vulkan_loader.h"

#define VKD \
    (grDevice->vkd)

#define CEILDIV(a, b) \
    (((a) + (b) - 1) / (b))

#define MIN(a, b) \
    ((a) < (b) ? (a) : (b))

#define MAX(a, b) \
    ((a) > (b) ? (a) : (b))

#define MIP(a, b) \
    MAX((a) >> (b), 1)

#define ALIGN(a, b) \
    (((a) + (b) - 1) & -(b))

#define COUNT_OF(array) \
    (sizeof(array) / sizeof((array)[0]))

#define OFFSET_OF(struct, member) \
    (size_t)(&((struct*)0)->member)

#define MEMBER_SIZEOF(struct, member) \
    sizeof(((struct *)0)->member)

#define OFFSET_OF_UNION(struct, unionMember, member) \
    (size_t)(&((struct*)0)->unionMember.member)

#define STACK_ARRAY(type, name, stackCount, count) \
    type _stack_##name[stackCount]; \
    type* name = (count) <= (stackCount) ? _stack_##name : malloc((count) * sizeof(type))

#define STACK_ARRAY_FINISH(name) \
   if (name != _stack_##name) free(name)

#define DESCRIPTORS_PER_SLOT (3)

GR_PHYSICAL_GPU_TYPE getGrPhysicalGpuType(
    VkPhysicalDeviceType type);

GR_RESULT getGrResult(
    VkResult result);

GR_FORMAT_FEATURE_FLAGS getGrFormatFeatureFlags(
    VkFormatFeatureFlags vkFeatureFlags);

GR_MEMORY_REQUIREMENTS getGrMemoryRequirements(
    const GrDevice* grDevice,
    VkMemoryRequirements vkMemReqs);

VkFormat getVkFormat(
    GR_FORMAT format);

VkFormat getDepthVkFormat(
    GR_FORMAT format);

VkFormat getStencilVkFormat(
    GR_FORMAT format);

unsigned getVkFormatTileSize(
    VkFormat vkFormat);

VkImageLayout getVkImageLayout(
    GR_IMAGE_STATE imageState);

VkImageTiling getVkImageTiling(
    GR_IMAGE_TILING imageTiling);

VkImageType getVkImageType(
    GR_IMAGE_TYPE imageType);

VkImageUsageFlags getVkImageUsageFlags(
    GR_IMAGE_USAGE_FLAGS imageUsageFlags);

VkAccessFlags getVkAccessFlagsImage(
    GR_IMAGE_STATE imageState);

VkPipelineStageFlags getVkPipelineStageFlagsImage(
    GR_IMAGE_STATE imageState);

VkAccessFlags getVkAccessFlagsMemory(
    GR_MEMORY_STATE memoryState);

VkPipelineStageFlags getVkPipelineStageFlagsMemory(
    GR_MEMORY_STATE memoryState);

VkImageAspectFlags getVkImageAspectFlags(
    GR_IMAGE_ASPECT imageAspect);

VkSampleCountFlags getVkSampleCountFlags(
    GR_UINT samples);

VkBlendFactor getVkBlendFactor(
    GR_BLEND blend);

VkBlendOp getVkBlendOp(
    GR_BLEND_FUNC blendFunc);

VkCompareOp getVkCompareOp(
    GR_COMPARE_FUNC compareFunc);

VkComponentSwizzle getVkComponentSwizzle(
    GR_CHANNEL_SWIZZLE channelSwizzle);

VkIndexType getVkIndexType(
    GR_INDEX_TYPE indexType);

VkLogicOp getVkLogicOp(
    GR_LOGIC_OP logicOp);

VkStencilOp getVkStencilOp(
    GR_STENCIL_OP stencilOp);

VkPolygonMode getVkPolygonMode(
    GR_FILL_MODE fillMode);

VkCullModeFlags getVkCullModeFlags(
    GR_CULL_MODE cullMode);

VkFrontFace getVkFrontFace(
    GR_FACE_ORIENTATION frontFace);

VkPrimitiveTopology getVkPrimitiveTopology(
    GR_PRIMITIVE_TOPOLOGY topology);

VkColorComponentFlags getVkColorComponentFlags(
    GR_UINT8 writeMask);

VkShaderStageFlags getVkShaderStageFlags(
    unsigned stageIndex);

VkPipelineBindPoint getVkPipelineBindPoint(
    GR_PIPELINE_BIND_POINT bindPoint);

VkFilter getVkFilterMag(
    GR_TEX_FILTER filter);

VkFilter getVkFilterMin(
    GR_TEX_FILTER filter);

VkSamplerAddressMode getVkSamplerAddressMode(
    GR_TEX_ADDRESS texAddress);

VkBorderColor getVkBorderColor(
    GR_BORDER_COLOR_TYPE borderColorType,
    int* customColorIndex);

VkQueryType getVkQueryType(
    GR_QUERY_TYPE queryType);

VkImageSubresource getVkImageSubresource(
    GR_IMAGE_SUBRESOURCE subresource);

VkImageSubresourceLayers getVkImageSubresourceLayers(
    GR_IMAGE_SUBRESOURCE subresource);

VkImageSubresourceRange getVkImageSubresourceRange(
    GR_IMAGE_SUBRESOURCE_RANGE subresourceRange,
    bool multiplyCubeLayers);

unsigned getDescriptorOffset(
    VkDescriptorType vkDescriptorType);

void grQueueAddInitialImage(
    GrImage* grImage);

void grQueueRemoveInitialImage(
    GrImage* grImage);

void grWsiDestroyImage(
    GrImage* grImage);

static inline unsigned nextPowerOfTwo(unsigned value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

#endif // MANTLE_INTERNAL_H_
