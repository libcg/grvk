#include "mantle_internal.h"

bool isMsaaSupported(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkImageTiling tiling)
{
    VkResult res;

    VkImageFormatProperties formatProps;
    res = vki.vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D,
                                                       tiling, VK_IMAGE_USAGE_SAMPLED_BIT, 0,
                                                       &formatProps);
    if (res != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceImageFormatProperties failed (%d)\n", res);
        assert(false);
    }

    return formatProps.sampleCounts > VK_SAMPLE_COUNT_1_BIT;
}

// Image and Sample Functions

GR_RESULT grGetFormatInfo(
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (infoType != GR_INFO_TYPE_FORMAT_PROPERTIES) {
        return GR_ERROR_INVALID_VALUE;
    } else if ((vkFormat = getVkFormat(format)) == VK_FORMAT_UNDEFINED) {
        return GR_ERROR_INVALID_FORMAT;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (pData == NULL) {
        // Return structure size
        *pDataSize = sizeof(GR_FORMAT_PROPERTIES);
        return GR_SUCCESS;
    } else if (*pDataSize < sizeof(GR_FORMAT_PROPERTIES)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
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

GR_RESULT grCreateSampler(
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
    } else if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo == NULL || pSampler == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
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
        .borderColor = getVkBorderColor(pCreateInfo->borderColor),
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult res = vki.vkCreateSampler(grDevice->device, &createInfo, NULL, &vkSampler);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSampler failed (%d)\n", res);
        return getGrResult(res);
    }

    GrSampler* grSampler = malloc(sizeof(GrSampler));
    *grSampler = (GrSampler) {
        .sType = GR_STRUCT_TYPE_SAMPLER,
        .sampler = vkSampler,
    };

    *pSampler = (GR_SAMPLER)grSampler;
    return GR_SUCCESS;
}
