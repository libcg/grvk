#include "mantle_internal.h"

// Image View Functions

GR_RESULT grCreateColorTargetView(
    GR_DEVICE device,
    const GR_COLOR_TARGET_VIEW_CREATE_INFO* pCreateInfo,
    GR_COLOR_TARGET_VIEW* pView)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pView);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pView == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    GrImage* grImage = (GrImage*)pCreateInfo->image;

    if (grImage == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grImage->sType != GR_STRUCT_TYPE_IMAGE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    if (pCreateInfo->arraySize == 0 ||
        pCreateInfo->baseArraySlice + pCreateInfo->arraySize > grImage->layerCount) {
        return GR_ERROR_INVALID_VALUE;
    }

    VkImageView vkImageView = VK_NULL_HANDLE;

    // TODO validate parameters

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = pCreateInfo->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D ,
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
    GrColorTargetView* grColorTargetView = malloc(sizeof(GrColorTargetView));
    if (grColorTargetView == NULL) {
        LOGE("malloc (grColorTargetView) failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }
    VkResult res = vki.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        free(grColorTargetView);
        return getGrResult(res);
    }
    *grColorTargetView = (GrColorTargetView) {
        .sType = GR_STRUCT_TYPE_COLOR_TARGET_VIEW,
        .imageView = vkImageView,
        .extent2D = { grImage->extent.width, grImage->extent.height },
        .layerCount = pCreateInfo->arraySize,
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
    const GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pView == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const GrImage* grImage = (GrImage*)pCreateInfo->image;

    if (grImage == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grImage->sType != GR_STRUCT_TYPE_IMAGE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkFlags aspectMask = 0;
    switch (grImage->format) {
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case VK_FORMAT_S8_UINT:
            aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        default:
            return GR_ERROR_INVALID_IMAGE;
            break;
    }

    if (pCreateInfo->arraySize == 0 || pCreateInfo->baseArraySlice + pCreateInfo->arraySize > grImage->layerCount) {
        return GR_ERROR_INVALID_VALUE;
    }

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = pCreateInfo->arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
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
    VkImageView vkImageView = VK_NULL_HANDLE;
    GrDepthTargetView* grDepthTargetView = malloc(sizeof(GrDepthTargetView));
    if (grDepthTargetView == NULL) {
        LOGE("malloc (GrDepthTargetview) failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }
    VkResult res = vki.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        free(grDepthTargetView);
        return getGrResult(res);
    }

    *grDepthTargetView = (GrDepthTargetView) {
        .sType = GR_STRUCT_TYPE_DEPTH_STENCIL_TARGET_VIEW,
        .imageView = vkImageView,
        .extent2D = { grImage->extent.width, grImage->extent.height },
        .layerCount = pCreateInfo->arraySize,
    };

    *pView = (GR_DEPTH_STENCIL_VIEW)grDepthTargetView;
    return GR_SUCCESS;
}


VkImageAspectFlags getImageViewAspectFlags(GR_IMAGE_ASPECT aspect)
{
    switch (aspect) {
    case GR_IMAGE_ASPECT_COLOR:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case GR_IMAGE_ASPECT_DEPTH:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case GR_IMAGE_ASPECT_STENCIL:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return GR_IMAGE_ASPECT_COLOR;
    }
}

VkComponentSwizzle getComponentSwizzle(GR_CHANNEL_SWIZZLE swizzle)
{
    switch (swizzle) {
    case GR_CHANNEL_SWIZZLE_ZERO:
        return VK_COMPONENT_SWIZZLE_ZERO;
    case GR_CHANNEL_SWIZZLE_ONE:
        return VK_COMPONENT_SWIZZLE_ONE;
    case GR_CHANNEL_SWIZZLE_R:
        return VK_COMPONENT_SWIZZLE_R;
    case GR_CHANNEL_SWIZZLE_G:
        return VK_COMPONENT_SWIZZLE_G;
    case GR_CHANNEL_SWIZZLE_B:
        return VK_COMPONENT_SWIZZLE_B;
    case GR_CHANNEL_SWIZZLE_A:
        return VK_COMPONENT_SWIZZLE_A;
    default:
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
}

uint32_t validateComponentSwizzle(GR_CHANNEL_SWIZZLE swizzle)
{
    return swizzle >= GR_CHANNEL_SWIZZLE_ZERO && swizzle <= GR_CHANNEL_SWIZZLE_A;
}

GR_RESULT grCreateImageView(
    GR_DEVICE device,
    const GR_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pView);
    const GrDevice* grDevice = (const GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pView == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const GrImage* grImage = (const GrImage*)pCreateInfo->image;

    if (grImage == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grImage->sType != GR_STRUCT_TYPE_IMAGE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkImageViewType imageViewType = 0;

    if (pCreateInfo->subresourceRange.arraySize == 0 ||
        pCreateInfo->subresourceRange.baseArraySlice + pCreateInfo->subresourceRange.arraySize > grImage->layerCount) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (!(validateComponentSwizzle(pCreateInfo->channels.r) &&
          validateComponentSwizzle(pCreateInfo->channels.g) &&
          validateComponentSwizzle(pCreateInfo->channels.b) &&
          validateComponentSwizzle(pCreateInfo->channels.a))) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (pCreateInfo->subresourceRange.arraySize > 1) {//since mantle api doesn't specify view array type explicitely, then just return array type if arraySize isn't 1
        switch (pCreateInfo->viewType) {
        case GR_IMAGE_VIEW_1D:
            imageViewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            break;
        case GR_IMAGE_VIEW_2D:
            imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case GR_IMAGE_VIEW_CUBE:
            imageViewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            break;
        default:
            return GR_ERROR_INVALID_VALUE;
        }
    }
    else {
        switch (pCreateInfo->viewType) {
        case GR_IMAGE_VIEW_1D:
            imageViewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case GR_IMAGE_VIEW_2D:
            imageViewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case GR_IMAGE_VIEW_3D:
            imageViewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case GR_IMAGE_VIEW_CUBE:
            imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        default:
            return GR_ERROR_INVALID_VALUE;
        }
    }

    const VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = grImage->image,
        .viewType = imageViewType,
        .format = getVkFormat(pCreateInfo->format),
        .components = {
            .r = getComponentSwizzle(pCreateInfo->channels.r),
            .g = getComponentSwizzle(pCreateInfo->channels.g),
            .b = getComponentSwizzle(pCreateInfo->channels.b),
            .a = getComponentSwizzle(pCreateInfo->channels.a),
        },
        .subresourceRange = {
            .aspectMask = getVkImageAspectFlags(pCreateInfo->subresourceRange.aspect),
            .baseMipLevel = pCreateInfo->subresourceRange.baseMipLevel,
            .levelCount = pCreateInfo->subresourceRange.mipLevels,
            .baseArrayLayer = pCreateInfo->subresourceRange.baseArraySlice,
            .layerCount = pCreateInfo->subresourceRange.arraySize,
        }
    };
    VkImageView vkImageView = VK_NULL_HANDLE;
    GrImageView* grImageView = malloc(sizeof(GrImageView));
    if (grImageView == NULL) {
        LOGE("malloc (GrDepthTargetview) failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkResult res = vki.vkCreateImageView(grDevice->device, &createInfo, NULL, &vkImageView);
    if (res != VK_SUCCESS) {
        free(grImageView);
        return getGrResult(res);
    }

    *grImageView = (GrImageView) {
        .sType = GR_STRUCT_TYPE_IMAGE_VIEW,
        .imageView = vkImageView,
        .extent = { grImage->extent.width, grImage->extent.height, grImage->extent.depth },
        .layerCount = pCreateInfo->subresourceRange.arraySize,
    };

    *pView = (GR_IMAGE_VIEW)grImageView;
    return GR_SUCCESS;
}
