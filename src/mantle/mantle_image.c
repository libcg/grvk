#include "mantle_internal.h"

GR_RESULT grCreateImage(
    GR_DEVICE device,
    const GR_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pImage);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pImage == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    if (pCreateInfo->imageType < GR_IMAGE_1D || pCreateInfo->imageType > GR_IMAGE_3D) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (pCreateInfo->tiling != GR_LINEAR_TILING && pCreateInfo->tiling != GR_OPTIMAL_TILING) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (pCreateInfo->arraySize == 0 || (pCreateInfo->imageType == GR_IMAGE_3D && pCreateInfo->arraySize != 1)) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (pCreateInfo->samples > 1 &&
        (pCreateInfo->mipLevels > 1 || (pCreateInfo->usage & (GR_IMAGE_USAGE_COLOR_TARGET | GR_IMAGE_USAGE_DEPTH_STENCIL)) == 0)) {
        return GR_ERROR_INVALID_VALUE;
    }
    if ((pCreateInfo->usage & (GR_IMAGE_USAGE_COLOR_TARGET | GR_IMAGE_USAGE_DEPTH_STENCIL)) == (GR_IMAGE_USAGE_COLOR_TARGET | GR_IMAGE_USAGE_DEPTH_STENCIL)) {
        return GR_ERROR_INVALID_FLAGS;
    }
    const VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,//TODO: add flags
        .imageType = getImageType(pCreateInfo->imageType),
        .format = getVkFormat(pCreateInfo->format),
        .extent = {
            .width = pCreateInfo->extent.width,
            .height = pCreateInfo->extent.height,
            .depth = pCreateInfo->extent.depth,
        },
        .mipLevels = pCreateInfo->mipLevels,
        .arrayLayers = pCreateInfo->arraySize,
        .samples = getVkSampleCountFlagBits(pCreateInfo->samples),
        .tiling = getVkImageTiling(pCreateInfo->tiling),
        .usage  = getVkImageUsageFlags(pCreateInfo->usage),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (createInfo.samples == 0) {
        return GR_ERROR_INVALID_VALUE;//samples are "validated" in function
    }
    GrImage* grImage = malloc(sizeof(GrImage));
    if (grImage == NULL) {
        LOGE("malloc (GrImage) failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }
    VkImage vkImage = VK_NULL_HANDLE;
    VkResult res = vki.vkCreateImage(grDevice->device, &createInfo, NULL, &vkImage);
    if (res != VK_SUCCESS) {
        free(grImage);
        switch (res) {
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return GR_ERROR_OUT_OF_GPU_MEMORY;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return GR_ERROR_OUT_OF_MEMORY;
        default:
            return GR_ERROR_INVALID_IMAGE;// idk what to return here, vulkan api shouldn't return another error
            break;
        }
    }
    *grImage = (GrImage) {
        .sType = GR_STRUCT_TYPE_IMAGE,
        .device = grDevice,
        .image = vkImage,
        .extent = {
            .width = pCreateInfo->extent.width,
            .height = pCreateInfo->extent.height,
            .depth = pCreateInfo->extent.depth,
        },
        .layerCount = pCreateInfo->arraySize,
        .format = createInfo.format,
    };
    *pImage = (GR_IMAGE)grImage;
    return GR_SUCCESS;
}


GR_RESULT grGetImageSubresourceInfo(
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
    }
    if (grImage->sType != GR_STRUCT_TYPE_IMAGE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (infoType != GR_INFO_TYPE_SUBRESOURCE_LAYOUT) {
        return GR_ERROR_INVALID_VALUE;
    }
    if (pSubresource == NULL || pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    if (pSubresource->arraySlice >= grImage->layerCount) {
        return GR_ERROR_INVALID_ORDINAL;
    }
    if (pData == NULL) {
        *pDataSize = sizeof(GR_SUBRESOURCE_LAYOUT);
        return GR_SUCCESS;
    }
    else if (*pDataSize != sizeof(GR_SUBRESOURCE_LAYOUT)) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }
    VkImageSubresource subresource = {
        .aspectMask = getVkImageAspectFlags(pSubresource->aspect),
        .mipLevel = pSubresource->mipLevel,
        .arrayLayer = pSubresource->arraySlice,
    };
    VkSubresourceLayout subresourceLayout;
    vki.vkGetImageSubresourceLayout(grImage->device->device,
                                grImage->image,
                                &subresource,
                                &subresourceLayout);
    *(GR_SUBRESOURCE_LAYOUT*)pData = (GR_SUBRESOURCE_LAYOUT) {
        .offset = subresourceLayout.offset,
        .size = subresourceLayout.size,
        .rowPitch = subresourceLayout.rowPitch,
        .depthPitch = subresourceLayout.depthPitch,
    };
    return GR_SUCCESS;
}
