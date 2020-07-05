#include "mantle_internal.h"

// Image View Functions

GR_RESULT grCreateColorTargetView(
    GR_DEVICE device,
    const GR_COLOR_TARGET_VIEW_CREATE_INFO* pCreateInfo,
    GR_COLOR_TARGET_VIEW* pView)
{
    VkDevice vkDevice = ((GrvkDevice*)device)->device;
    VkImageView vkImageView = VK_NULL_HANDLE;

    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = ((GrvkImage*)pCreateInfo->image)->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = getVkFormat(pCreateInfo->format),
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = pCreateInfo->mipLevel,
            .levelCount = 1,
            .baseArrayLayer = pCreateInfo->baseArraySlice,
            .layerCount = pCreateInfo->arraySize == GR_LAST_MIP_OR_SLICE ?
                          VK_REMAINING_ARRAY_LAYERS : pCreateInfo->arraySize,
        }
    };

    if (vkCreateImageView(vkDevice, &createInfo, NULL, &vkImageView) != VK_SUCCESS) {
        printf("%s: vkCreateImageView failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrvkColorTargetView* grvkColorTargetView = malloc(sizeof(GrvkColorTargetView));
    *grvkColorTargetView = (GrvkColorTargetView) {
        .sType = GRVK_STRUCT_TYPE_COLOR_TARGET_VIEW,
        .imageView = vkImageView,
    };

    *pView = (GR_COLOR_TARGET_VIEW)grvkColorTargetView;
    return GR_SUCCESS;
}
