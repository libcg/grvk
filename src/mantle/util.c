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

uint32_t getVkQueueFamilyIndex(
    GR_ENUM queueType)
{
    // FIXME this will break
    return queueType - GR_QUEUE_UNIVERSAL;
}
