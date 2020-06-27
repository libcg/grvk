#include "mantle/mantleWsiWinExt.h"
#include "vulkan/vulkan.h"
#include "mantle_internal.h"

GR_RESULT grWsiWinCreatePresentableImage(
    GR_DEVICE device,
    const GR_WSI_WIN_PRESENTABLE_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    VkDevice vkDevice = (VkDevice)device;
    VkImage vkImage = VK_NULL_HANDLE;

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = grFormatToVk(pCreateInfo->format),
        .extent = { pCreateInfo->extent.width, pCreateInfo->extent.height, 0 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(vkDevice, &createInfo, NULL, &vkImage) != VK_SUCCESS) {
        return GR_ERROR_INVALID_VALUE;
    }

    *pImage = (GR_IMAGE)vkImage;
    *pMem = (GR_GPU_MEMORY)NULL; // FIXME not sure what to do with this

    return GR_SUCCESS;
}
