#include "mantle_internal.h"

// Image and Sample Functions

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
