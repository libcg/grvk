#ifndef MANTLE_INTERNAL_H_
#define MANTLE_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mantle/mantle.h"
#include "mantle_object.h"
#include "vulkan_loader.h"

#define INVALID_QUEUE_INDEX (~0u)

GR_VOID* grAlloc(
    GR_SIZE size,
    GR_SIZE alignment,
    GR_ENUM allocType);

GR_VOID grFree(
    GR_VOID* pMem);

VkFormat getVkFormat(
    GR_FORMAT format);

VkImageLayout getVkImageLayout(
    GR_IMAGE_STATE imageState);

VkAccessFlags getVkAccessFlagsImage(
    GR_IMAGE_STATE imageState);

VkAccessFlags getVkAccessFlagsMemory(
    GR_MEMORY_STATE memoryState);

VkImageAspectFlags getVkImageAspectFlags(
    GR_IMAGE_ASPECT imageAspect);

VkSampleCountFlagBits getVkSampleCountFlagBits(
    GR_UINT samples);

VkBlendFactor getVkBlendFactor(
    GR_BLEND blend);

VkBlendOp getVkBlendOp(
    GR_BLEND_FUNC blendFunc);

VkCompareOp getVkCompareOp(
    GR_COMPARE_FUNC compareFunc);

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

VkDescriptorType getVkDescriptorType(
    GR_DESCRIPTOR_SET_SLOT_TYPE slotObjectType);

VkColorComponentFlags getVkColorComponentFlags(
    GR_UINT8 writeMask);

VkShaderStageFlags getVkShaderStageFlags(
    uint32_t stageIndex);

VkPipelineBindPoint getVkPipelineBindPoint(
    GR_PIPELINE_BIND_POINT bindPoint);

#endif // MANTLE_INTERNAL_H_
