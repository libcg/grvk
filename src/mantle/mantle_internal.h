#ifndef MANTLE_INTERNAL_H_
#define MANTLE_INTERNAL_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mantle/mantle.h"
#include "vulkan/vulkan.h"

GR_VOID* grvkAlloc(
    GR_SIZE size,
    GR_SIZE alignment,
    GR_ENUM allocType);

GR_VOID grvkFree(
    GR_VOID* pMem);

VkFormat getVkFormat(
    GR_FORMAT format);

VkImageLayout getVkImageLayout(
    GR_ENUM imageState);

VkAccessFlags getVkAccessFlags(
    GR_ENUM imageState);

VkImageAspectFlags getVkImageAspectFlags(
    GR_ENUM imageAspect);

VkSampleCountFlagBits getVkSampleCountFlagBits(
    GR_UINT samples);

VkBlendFactor getVkBlendFactor(
    GR_ENUM blend);

VkBlendOp getVkBlendOp(
    GR_ENUM blendFunc);

VkCompareOp getVkCompareOp(
    GR_ENUM compareFunc);

VkStencilOp getVkStencilOp(
    GR_ENUM stencilOp);

VkPolygonMode getVkPolygonMode(
    GR_ENUM fillMode);

VkCullModeFlags getVkCullModeFlags(
    GR_ENUM cullMode);

VkFrontFace getVkFrontFace(
    GR_ENUM frontFace);

VkPrimitiveTopology getVkPrimitiveTopology(
    GR_ENUM topology);

VkDescriptorType getVkDescriptorType(
    GR_ENUM slotObjectType);

VkColorComponentFlags getVkColorComponentFlags(
    GR_UINT8 writeMask);

uint32_t getVkQueueFamilyIndex(
    GR_ENUM queueType);

#endif // MANTLE_INTERNAL_H_
