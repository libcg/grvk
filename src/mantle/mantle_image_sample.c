#include "mantle_internal.h"

static bool isMsaaSupported(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkImageTiling tiling)
{
    VkResult res;

    VkImageFormatProperties formatProps;
    res = vki.vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D,
                                                       tiling, VK_IMAGE_USAGE_SAMPLED_BIT, 0,
                                                       &formatProps);
    if (res == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        return false;
    } else if (res != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceImageFormatProperties failed (%d)\n", res);
        assert(false);
    }

    return formatProps.sampleCounts > VK_SAMPLE_COUNT_1_BIT;
}

// Image and Sample Functions

GR_RESULT GR_STDCALL grGetFormatInfo(
    GR_DEVICE device,
    GR_FORMAT format,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X 0x%X %p %p\n", device, format, infoType, pDataSize, pData);
    GrDevice* grDevice = (GrDevice*)device;
    GR_FORMAT_PROPERTIES* formatProps = (GR_FORMAT_PROPERTIES*)pData;
    VkFormat vkFormat;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (infoType != GR_INFO_TYPE_FORMAT_PROPERTIES) {
        return GR_ERROR_INVALID_VALUE;
    } else if ((vkFormat = getVkFormat(format)) == VK_FORMAT_UNDEFINED) {
        return GR_ERROR_INVALID_FORMAT;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pData != NULL && *pDataSize < sizeof(GR_FORMAT_PROPERTIES)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    *pDataSize = sizeof(GR_FORMAT_PROPERTIES);

    if (pData == NULL) {
        return GR_SUCCESS;
    }

    VkFormatProperties vkFormatProps;
    vki.vkGetPhysicalDeviceFormatProperties(grDevice->physicalDevice, vkFormat, &vkFormatProps);

    formatProps->linearTilingFeatures =
        getGrFormatFeatureFlags(vkFormatProps.linearTilingFeatures) |
        (isMsaaSupported(grDevice->physicalDevice, vkFormat, VK_IMAGE_TILING_LINEAR) ?
         GR_FORMAT_MSAA_TARGET : 0);
    formatProps->optimalTilingFeatures =
        getGrFormatFeatureFlags(vkFormatProps.optimalTilingFeatures) |
        (isMsaaSupported(grDevice->physicalDevice, vkFormat, VK_IMAGE_TILING_OPTIMAL) ?
         GR_FORMAT_MSAA_TARGET : 0);

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateImage(
    GR_DEVICE device,
    const GR_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pImage);
    GrDevice* grDevice = (GrDevice*)device;
    VkImage vkImage = VK_NULL_HANDLE;
    VkResult vkRes;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo->imageType < GR_IMAGE_1D || pCreateInfo->imageType > GR_IMAGE_3D ||
               pCreateInfo->tiling < GR_LINEAR_TILING || pCreateInfo->tiling > GR_OPTIMAL_TILING) {
        return GR_ERROR_INVALID_VALUE;
    } else if ((pCreateInfo->imageType == GR_IMAGE_1D &&
                (pCreateInfo->extent.height != 1 || pCreateInfo->extent.depth != 1)) ||
               (pCreateInfo->imageType == GR_IMAGE_2D &&
                (pCreateInfo->extent.depth != 1))) {
        return GR_ERROR_INVALID_VALUE;
    } else if (0) {
        // TODO check image dimensions for compressed formats
        // TODO check number of samples for the given image type and format
    } else if (pCreateInfo->samples > 1 &&
               (pCreateInfo->usage &
                (GR_IMAGE_USAGE_COLOR_TARGET | GR_IMAGE_USAGE_DEPTH_STENCIL)) == 0) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pCreateInfo->samples > 1 && pCreateInfo->mipLevels != 1) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pCreateInfo->arraySize == 0 ||
               (pCreateInfo->imageType == GR_IMAGE_3D && pCreateInfo->arraySize != 1)) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pCreateInfo->mipLevels == 0) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pCreateInfo == NULL || pImage == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (0) {
        // TODO check format
        // TODO check invalid image creation flags or image usage flags
    } else if ((pCreateInfo->usage & GR_IMAGE_USAGE_COLOR_TARGET) != 0 &&
               (pCreateInfo->usage & GR_IMAGE_USAGE_DEPTH_STENCIL) != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (pCreateInfo->imageType == GR_IMAGE_1D &&
               (pCreateInfo->usage & GR_IMAGE_USAGE_COLOR_TARGET) != 0) {
        return GR_ERROR_INVALID_FLAGS;
    } else if (pCreateInfo->imageType != GR_IMAGE_2D &&
               (pCreateInfo->usage & GR_IMAGE_USAGE_DEPTH_STENCIL) != 0) {
        return GR_ERROR_INVALID_FLAGS;
    }

    bool isTarget = pCreateInfo->usage & GR_IMAGE_USAGE_COLOR_TARGET ||
                    pCreateInfo->usage & GR_IMAGE_USAGE_DEPTH_STENCIL;
    bool isCubic = pCreateInfo->imageType == GR_IMAGE_2D &&
                   pCreateInfo->extent.width == pCreateInfo->extent.height &&
                   pCreateInfo->extent.depth == 1 &&
                   pCreateInfo->arraySize % 6 == 0 &&
                   pCreateInfo->samples == 1;

    if ((pCreateInfo->flags & ~GR_IMAGE_CREATE_VIEW_FORMAT_CHANGE) != 0) {
        LOGW("unhandled flags 0x%X\n", pCreateInfo->flags);
    }

    const VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = (pCreateInfo->flags & GR_IMAGE_CREATE_VIEW_FORMAT_CHANGE ?
                  VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0) |
                 (isCubic ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0),
        .imageType = getVkImageType(pCreateInfo->imageType),
        .format = getVkFormat(pCreateInfo->format),
        .extent = {
            .width = pCreateInfo->extent.width,
            .height = pCreateInfo->extent.height,
            .depth = pCreateInfo->extent.depth,
        },
        .mipLevels = pCreateInfo->mipLevels,
        .arrayLayers = pCreateInfo->arraySize,
        .samples = getVkSampleCountFlags(pCreateInfo->samples),
        .tiling = getVkImageTiling(pCreateInfo->tiling),
        .usage  = getVkImageUsageFlags(pCreateInfo->usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = isTarget ? getVkImageLayout(GR_IMAGE_STATE_UNINITIALIZED)
                                  : VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    VkImageFormatProperties imageFormatProperties;
    vkRes = vki.vkGetPhysicalDeviceImageFormatProperties(grDevice->physicalDevice,
                                                         createInfo.format, createInfo.imageType,
                                                         createInfo.tiling, createInfo.usage,
                                                         createInfo.flags, &imageFormatProperties);
    if (vkRes == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        LOGW("unsupported format %d for image type %d, tiling %d, usage 0x%X and flags 0x%X\n",
             createInfo.format, createInfo.imageType, createInfo.tiling, createInfo.usage,
             createInfo.flags);
    } else if (vkRes != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceImageFormatProperties failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    vkRes = VKD.vkCreateImage(grDevice->device, &createInfo, NULL, &vkImage);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateImage failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    GrImage* grImage = malloc(sizeof(GrImage));
    *grImage = (GrImage) {
        .grObj = { GR_OBJ_TYPE_IMAGE, grDevice },
        .image = vkImage,
        .imageType = createInfo.imageType,
        .extent = createInfo.extent,
        .arrayLayers = createInfo.arrayLayers,
        .format = createInfo.format,
        .usage = createInfo.usage,
        .multiplyCubeLayers = quirkHas(QUIRK_CUBEMAP_LAYER_DIV_6) && isCubic,
        .isOpaque = pCreateInfo->tiling == GR_OPTIMAL_TILING,
    };

    // Mantle spec: "When [...] non-target images are bound to memory, they are assumed
    //               to be in the [...] GR_IMAGE_STATE_DATA_TRANSFER state."
    if (!isTarget) {
        // Queue image for transition to initial data transfer state
        grQueueAddInitialImage(grImage);
    }

    *pImage = (GR_IMAGE)grImage;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetImageSubresourceInfo(
    GR_IMAGE image,
    const GR_IMAGE_SUBRESOURCE* pSubresource,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p %p 0x%X %p %p\n", image, pSubresource, infoType, pDataSize, pData);
    GrImage* grImage = (GrImage*)image;

    if (grImage == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grImage) != GR_OBJ_TYPE_IMAGE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (infoType != GR_INFO_TYPE_SUBRESOURCE_LAYOUT) {
        return GR_ERROR_INVALID_VALUE;
    } else if (0) {
        // TODO check subresource ID ordinal
        return GR_ERROR_INVALID_ORDINAL;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pData != NULL && *pDataSize < sizeof(GR_SUBRESOURCE_LAYOUT)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    *pDataSize = sizeof(GR_SUBRESOURCE_LAYOUT);

    if (pData == NULL) {
        return GR_SUCCESS;
    }

    if (grImage->isOpaque) {
        // Mantle spec: "For opaque images, the returned pitch values are zero."
        *(GR_SUBRESOURCE_LAYOUT*)pData = (GR_SUBRESOURCE_LAYOUT) {
            .offset = 0,
            .size = 0,
            .rowPitch = 0,
            .depthPitch = 0,
        };
    } else {
        GrDevice* grDevice = GET_OBJ_DEVICE(grImage);
        VkImageSubresource subresource = getVkImageSubresource(*pSubresource);

        VkSubresourceLayout subresourceLayout;
        VKD.vkGetImageSubresourceLayout(grDevice->device, grImage->image, &subresource,
                                        &subresourceLayout);

        *(GR_SUBRESOURCE_LAYOUT*)pData = (GR_SUBRESOURCE_LAYOUT) {
            .offset = subresourceLayout.offset,
            .size = subresourceLayout.size,
            .rowPitch = subresourceLayout.rowPitch,
            .depthPitch = subresourceLayout.depthPitch,
        };
    }

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateSampler(
    GR_DEVICE device,
    const GR_SAMPLER_CREATE_INFO* pCreateInfo,
    GR_SAMPLER* pSampler)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pSampler);
    GrDevice* grDevice = (GrDevice*)device;
    VkSampler vkSampler = VK_NULL_HANDLE;

    // TODO check invalid values
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pSampler == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    int customColorIndex = -1;
    VkBorderColor vkBorderColor = getVkBorderColor(pCreateInfo->borderColor, &customColorIndex);
    VkClearColorValue colorValue = { 0 };

    if (customColorIndex >= 0) {
        float* data = &grDevice->grBorderColorPalette->data[4 * customColorIndex];

        memcpy(colorValue.float32, data, 4 * sizeof(float));
    }

    const VkSamplerCustomBorderColorCreateInfoEXT borderColorCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
        .pNext = NULL,
        .customBorderColor = colorValue,
        .format = VK_FORMAT_UNDEFINED,
    };

    const VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = customColorIndex >= 0 ? &borderColorCreateInfo : NULL,
        .flags = 0,
        .magFilter = getVkFilterMag(pCreateInfo->filter),
        .minFilter = getVkFilterMin(pCreateInfo->filter),
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = getVkSamplerAddressMode(pCreateInfo->addressU),
        .addressModeV = getVkSamplerAddressMode(pCreateInfo->addressV),
        .addressModeW = getVkSamplerAddressMode(pCreateInfo->addressW),
        .mipLodBias = pCreateInfo->mipLodBias,
        .anisotropyEnable = pCreateInfo->filter == GR_TEX_FILTER_ANISOTROPIC,
        .maxAnisotropy = pCreateInfo->maxAnisotropy,
        .compareEnable = VK_TRUE,
        .compareOp = getVkCompareOp(pCreateInfo->compareFunc),
        .minLod = pCreateInfo->minLod,
        .maxLod = pCreateInfo->maxLod,
        .borderColor = vkBorderColor,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult res = VKD.vkCreateSampler(grDevice->device, &createInfo, NULL, &vkSampler);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSampler failed (%d)\n", res);
        return getGrResult(res);
    }

    GrSampler* grSampler = malloc(sizeof(GrSampler));
    *grSampler = (GrSampler) {
        .grObj = { GR_OBJ_TYPE_SAMPLER, grDevice },
        .sampler = vkSampler,
    };

    *pSampler = (GR_SAMPLER)grSampler;
    return GR_SUCCESS;
}
