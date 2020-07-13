#include <stdio.h>
#include "mantle/mantleWsiWinExt.h"
#include "vulkan/vulkan.h"
#include "mantle_internal.h"

GR_RESULT grWsiWinCreatePresentableImage(
    GR_DEVICE device,
    const GR_WSI_WIN_PRESENTABLE_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    VkDevice vkDevice = ((GrvkDevice*)device)->device;
    VkImage vkImage = VK_NULL_HANDLE;
    VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = getVkFormat(pCreateInfo->format),
        .extent = { pCreateInfo->extent.width, pCreateInfo->extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(vkDevice, &createInfo, NULL, &vkImage) != VK_SUCCESS) {
        printf("%s: vkCreateImage failed\n", __func__);
        return GR_ERROR_INVALID_VALUE;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkDevice, vkImage, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = 0, // FIXME don't hardcode
    };

    if (vkAllocateMemory(vkDevice, &allocateInfo, NULL, &vkDeviceMemory) != VK_SUCCESS) {
        printf("%s: vkAllocateMemory failed\n", __func__);
        vkDestroyImage(vkDevice, vkImage, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    if (vkBindImageMemory(vkDevice, vkImage, vkDeviceMemory, 0) != VK_SUCCESS) {
        printf("%s: vkBindImageMemory failed\n", __func__);
        vkFreeMemory(vkDevice, vkDeviceMemory, NULL);
        vkDestroyImage(vkDevice, vkImage, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    GrvkImage* grvkImage = malloc(sizeof(GrvkImage));
    *grvkImage = (GrvkImage) {
        .sType = GRVK_STRUCT_TYPE_IMAGE,
        .image = vkImage,
    };

    GrvkGpuMemory* grvkGpuMemory = malloc(sizeof(GrvkGpuMemory));
    *grvkGpuMemory = (GrvkGpuMemory) {
        .sType = GRVK_STRUCT_TYPE_GPU_MEMORY,
        .deviceMemory = vkDeviceMemory,
    };

    *pImage = (GR_IMAGE)grvkImage;
    *pMem = (GR_GPU_MEMORY)grvkGpuMemory;

    return GR_SUCCESS;
}
