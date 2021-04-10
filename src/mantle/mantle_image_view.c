#include "mantle_internal.h"

// Image View Functions

GR_RESULT grCreateImageView(
    GR_DEVICE device,
    const GR_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pView);
    GrDevice* grDevice = (GrDevice*)device;
    VkImageView vkImageView = VK_NULL_HANDLE;

    // TODO validate parameters

    GrImage* grImage = (GrImage*)pCreateInfo->image;

    bool isArrayed = pCreateInfo->subresourceRange.arraySize > 1;
    VkImageViewType imageViewType = 0;
    if (pCreateInfo->viewType == GR_IMAGE_VIEW_1D) {
        imageViewType = isArrayed ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
    } else if (pCreateInfo->viewType == GR_IMAGE_VIEW_2D) {
        imageViewType = isArrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    } else if (pCreateInfo->viewType == GR_IMAGE_VIEW_3D) {
        imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    } else if (pCreateInfo->viewType == GR_IMAGE_VIEW_CUBE) {
        imageViewType = isArrayed ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
    } else {
        LOGE("unexpected image type 0x%X\n", grImage->imageType);
        assert(false);
    }

    if (grImage->isCube && pCreateInfo->viewType != GR_IMAGE_VIEW_CUBE) {
        // There's a mismatch between Vulkan and Mantle ImageSubresourceRange for cubemap layers.
        // See getVkImageSubresourceRange() and its usage.
        LOGW("non-cube image view created for cube image %p\n", grImage);
    }

    if (pCreateInfo->minLod != 0.f) {
        LOGW("unhandled minLod %g\n", pCreateInfo->minLod);
    }

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = imageViewType,
        .format = getVkFormat(pCreateInfo->format),
        .components = {
            .r = getVkComponentSwizzle(pCreateInfo->channels.r),
            .g = getVkComponentSwizzle(pCreateInfo->channels.g),
            .b = getVkComponentSwizzle(pCreateInfo->channels.b),
            .a = getVkComponentSwizzle(pCreateInfo->channels.a),
        },
        .subresourceRange = getVkImageSubresourceRange(pCreateInfo->subresourceRange,
                                                       pCreateInfo->viewType == GR_IMAGE_VIEW_CUBE),
    };

    VkResult res = VKD.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateImageView failed (%d)\n", res);
        return getGrResult(res);
    }

    GrImageView* grImageView = malloc(sizeof(GrImageView));
    *grImageView = (GrImageView) {
        .grObj = { GR_OBJ_TYPE_IMAGE_VIEW, grDevice },
        .imageView = vkImageView,
    };

    *pView = (GR_IMAGE_VIEW)grImageView;
    return GR_SUCCESS;
}

GR_RESULT grCreateColorTargetView(
    GR_DEVICE device,
    const GR_COLOR_TARGET_VIEW_CREATE_INFO* pCreateInfo,
    GR_COLOR_TARGET_VIEW* pView)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pView);
    GrDevice* grDevice = (GrDevice*)device;
    VkImageView vkImageView = VK_NULL_HANDLE;

    // TODO validate parameters

    GrImage* grImage = (GrImage*)pCreateInfo->image;

    bool isArrayed = pCreateInfo->arraySize > 1;
    VkImageViewType imageViewType = 0;
    if (grImage->imageType == VK_IMAGE_TYPE_2D) {
        imageViewType = isArrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    } else if (grImage->imageType == VK_IMAGE_TYPE_3D) {
        imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    } else {
        LOGE("unexpected image type 0x%X\n", grImage->imageType);
        assert(false);
    }

    if (grImage->isCube) {
        LOGW("color target view created for cube image %p\n", grImage);
    }

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = imageViewType,
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
            .layerCount = pCreateInfo->arraySize,
        }
    };

    VkResult res = VKD.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateImageView failed (%d)\n", res);
        return getGrResult(res);
    }

    GrColorTargetView* grColorTargetView = malloc(sizeof(GrColorTargetView));
    *grColorTargetView = (GrColorTargetView) {
        .grObj = { GR_OBJ_TYPE_COLOR_TARGET_VIEW, grDevice },
        .imageView = vkImageView,
        .extent = {
            MAX(grImage->extent.width >> pCreateInfo->mipLevel, 1),
            MAX(grImage->extent.height >> pCreateInfo->mipLevel, 1),
            pCreateInfo->arraySize,
        },
    };

    *pView = (GR_COLOR_TARGET_VIEW)grColorTargetView;
    return GR_SUCCESS;
}

GR_RESULT grCreateDepthStencilView(
    GR_DEVICE device,
    const GR_DEPTH_STENCIL_VIEW_CREATE_INFO* pCreateInfo,
    GR_DEPTH_STENCIL_VIEW* pView)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pView);
    GrDevice* grDevice = (GrDevice*)device;
    VkImageView vkImageView = VK_NULL_HANDLE;

    // TODO validate parameters

    GrImage* grImage = (GrImage*)pCreateInfo->image;

    bool isArrayed = pCreateInfo->arraySize > 1;
    VkImageViewType imageViewType = 0;
    if (grImage->imageType == VK_IMAGE_TYPE_2D) {
        imageViewType = isArrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    } else if (grImage->imageType == VK_IMAGE_TYPE_3D) {
        imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    } else {
        LOGE("unexpected image type 0x%X\n", grImage->imageType);
        assert(false);
    }

    if (grImage->isCube) {
        LOGW("depth stencil view created for cube image %p\n", grImage);
    }

    if (pCreateInfo->flags != 0) {
        LOGW("unhandled flags 0x%X\n", pCreateInfo->flags);
    }

    VkImageAspectFlags aspectMask = 0;
    switch (grImage->format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    case VK_FORMAT_S8_UINT:
        aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    default:
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    }

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = imageViewType,
        .format = grImage->format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = pCreateInfo->mipLevel,
            .levelCount = 1,
            .baseArrayLayer = pCreateInfo->baseArraySlice,
            .layerCount = pCreateInfo->arraySize,
        }
    };

    VkResult res = VKD.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateImageView failed (%d)\n", res);
        return getGrResult(res);
    }

    GrDepthStencilView* grDepthStencilView = malloc(sizeof(GrDepthStencilView));
    *grDepthStencilView = (GrDepthStencilView) {
        .grObj = { GR_OBJ_TYPE_DEPTH_STENCIL_VIEW, grDevice },
        .imageView = vkImageView,
        .extent = {
            MAX(grImage->extent.width >> pCreateInfo->mipLevel, 1),
            MAX(grImage->extent.height >> pCreateInfo->mipLevel, 1),
            pCreateInfo->arraySize,
        },
    };

    *pView = (GR_DEPTH_STENCIL_VIEW)grDepthStencilView;
    return GR_SUCCESS;
}
