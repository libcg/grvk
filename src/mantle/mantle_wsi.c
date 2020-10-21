#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"
#include "mantle_wsi.h"
#include <assert.h>

static uint32_t
select_memory_type(const VkPhysicalDeviceMemoryProperties *memProps,
                   VkMemoryPropertyFlags props,
                   uint32_t type_bits)
{
    for (uint32_t i = 0; i < memProps->memoryTypeCount; i++) {
        const VkMemoryType type = memProps->memoryTypes[i];
        if ((type_bits & (1 << i)) && (type.propertyFlags & props) == props)
            return i;
    }
    assert(!"No suitable memory type found");
#ifdef _MSC_VER
    __assume(0);
#else
    __builtin_unreachable();
#endif
}

static uint32_t
select_preferred_memory_type(const VkPhysicalDeviceMemoryProperties *memProps,
                             VkMemoryPropertyFlags preferredProps,
                             VkMemoryPropertyFlags props,
                             uint32_t type_bits)
{
    uint32_t requiredType = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < memProps->memoryTypeCount; i++) {
        const VkMemoryType type = memProps->memoryTypes[i];
        if ((type_bits & (1 << i)) && (type.propertyFlags & preferredProps) == preferredProps)
            return i;
        else if ((type_bits & (1 << i)) && (type.propertyFlags & props) == props)
            requiredType = i;
    }
    if (requiredType != 0xFFFFFFFFu) {
        return requiredType;
    }
    assert(!"No suitable memory type found");
#ifdef _MSC_VER
    __assume(0);
#else
    __builtin_unreachable();
#endif
}

static inline uint32_t
align_u32(uint32_t v, uint32_t a)
{
    assert(a != 0 && a == (a & -a));
    return (v + a - 1) & ~(a - 1);
}


static VkResult buildCopyCommandBuffer(
    const GrDevice* grDevice,
    VkImage srcImage,
    VkExtent2D srcExtent,
    VkBuffer dstBuffer,
    uint32_t rowLengthTexels,
    VkCommandBuffer *pCmdBuf)
{
    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = grDevice->universalCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkResult res = vki.vkAllocateCommandBuffers(grDevice->device, &allocateInfo,
                                                &cmdBuf);
    if (res != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed\n");
    }

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };

    if (vki.vkBeginCommandBuffer(cmdBuf, &beginInfo) != VK_SUCCESS) {
        LOGE("vkBeginCommandBuffer failed\n");
    }

    const VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = rowLengthTexels,
        .bufferImageHeight = srcExtent.height,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {
            .x = 0,
            .y = 0,
            .z = 0,
        },
        .imageExtent = {
            .width = srcExtent.width,
            .height = srcExtent.height,
            .depth = 1,
        }
    };

    vki.vkCmdCopyImageToBuffer(cmdBuf,
                               srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,//taken from original swapchain extension
                               dstBuffer,
                               1, &region);

    res = vki.vkEndCommandBuffer(cmdBuf);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed\n");
    }
    else {
        *pCmdBuf = cmdBuf;
    }

    return res;
}

static const VkFormat WSI_WINDOWS_FORMATS[] = {
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_A1R5G5B5_UNORM_PACK16,
    VK_FORMAT_R5G6B5_UNORM_PACK16,
    VK_FORMAT_R5G5B5A1_UNORM_PACK16,
    VK_FORMAT_B5G6R5_UNORM_PACK16,
    //all formats lower than that ain't remapped
    VK_FORMAT_B5G5R5A1_UNORM_PACK16,
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_B8G8R8_UNORM,
};

static const uint32_t WSI_WINDOWS_FORMATS_PIXEL_SIZE[] = {4, 2, 2, 2, 2, 2, 4, 3};
static const DWORD WSI_WINDOWS_BITMASK_FORMATS_MASK[][4] = {
    {0x000000FF,0x0000FF00,0x00FF0000,0},//in case of 32 bits Windows expects layout to be BGR instead of RGB in case of 16 bits
    //{0b0111110000000000, 0b0000001111100000, 0b0000000000011111, 0},//ARGB
    {0x7c00, 0x03e0, 0x001f, 0},
    //{0b1111100000000000, 0b0000011111100000, 0b0000000000011111, 0},//R5G6B5
    {0xf800, 0x7e0, 0x1f, 0},
    //{0b1111100000000000, 0b0000011111000000, 0b0000000000111110, 0},//R5G5B5A1
    {0xf800, 0x7c0, 0x3e, 0},
    //{0b0000000000011111, 0b0000011111100000, 0b1111100000000000, 0} //B5G6R5
    {0x1f, 0x7e0, 0xf800, 0}
};
static const size_t FORMATS_SIZE = sizeof(WSI_WINDOWS_FORMATS) / sizeof(VkFormat);
static const size_t FORMATS_MASKS_SIZE = sizeof(WSI_WINDOWS_BITMASK_FORMATS_MASK) / sizeof(DWORD) / 4;
void wsiPresentBufferToHWND(HWND hwnd, void* img_buffer, VkExtent2D dstExtent, uint32_t rowPitchTexels, uint32_t bufferLength, VkFormat format)
{
    BYTE bm_info_bytes[sizeof(BITMAPINFO) + 4];
    BITMAPINFO* bm_info = (BITMAPINFO*)bm_info_bytes;

    *bm_info = (BITMAPINFO) {
        .bmiHeader = {
            .biSize = sizeof(BITMAPINFOHEADER) + 4,
            .biWidth = rowPitchTexels,
            .biHeight = -(int)dstExtent.height,//negative value indicates top-down DIB
            .biPlanes = 1,
            .biBitCount = 0,
            .biCompression = BI_RGB,
            .biSizeImage = bufferLength,
            .biXPelsPerMeter = (LONG)0,
            .biYPelsPerMeter = (LONG)0,
            .biClrUsed = 0,
            .biClrImportant = 0
        },
        .bmiColors = {}
    };

    for (uint32_t i = 0; i < FORMATS_SIZE;++i) {
        if (format == WSI_WINDOWS_FORMATS[i]) {
            bm_info->bmiHeader.biBitCount = WSI_WINDOWS_FORMATS_PIXEL_SIZE[i] * 8;//adjust to bits
            if (i < FORMATS_MASKS_SIZE) {
                //TODO: handle bitmask
                memcpy(&bm_info->bmiColors, WSI_WINDOWS_BITMASK_FORMATS_MASK[i], sizeof(DWORD) * 4);
                bm_info->bmiHeader.biCompression = BI_BITFIELDS;
            }
            break;
        }
    }

    if (bm_info->bmiHeader.biBitCount == 0) {
        LOGE("Couldn't find compatible format for image during presenting");
        return;
    }

    HDC dc = GetDC(hwnd);
    SetDIBitsToDevice(
        dc,
        0, 0,//dst
        dstExtent.width,
        dstExtent.height,
        0, 0,//src
        0,
        dstExtent.height,
        img_buffer,
        bm_info,
        DIB_RGB_COLORS
        );
    ReleaseDC(hwnd,dc);
}

// Functions

GR_RESULT grWsiWinCreatePresentableImage(
    GR_DEVICE device,
    const GR_WSI_WIN_PRESENTABLE_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    LOGT("%p %p %p %p\n", device, pCreateInfo, pImage, pMem);
    if (device == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (pCreateInfo == NULL || pImage == NULL || pMem == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    GrDevice* grDevice = (GrDevice*)device;
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    VkImage vkImage = VK_NULL_HANDLE;
    VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
    VkBuffer copyBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bufferDeviceMemory = VK_NULL_HANDLE;

    VkFormat format = getVkFormat(pCreateInfo->format);

    uint32_t bytesPerPixel = 0;
    for (uint32_t i = 0; i < FORMATS_SIZE;++i) {
        if (format == WSI_WINDOWS_FORMATS[i]) {
            bytesPerPixel = WSI_WINDOWS_FORMATS_PIXEL_SIZE[i];
        }
    }
    if (bytesPerPixel == 0) {
        return GR_ERROR_INVALID_FORMAT;
    }

    const VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { pCreateInfo->extent.width, pCreateInfo->extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkResult res = vki.vkCreateImage(grDevice->device, &createInfo, NULL, &vkImage);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateImage failed\n");
        goto create_fail;
    }

    VkMemoryRequirements memoryRequirements;
    vki.vkGetImageMemoryRequirements(grDevice->device, vkImage, &memoryRequirements);

    const VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = select_memory_type( &grDevice->memoryProperties,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                               memoryRequirements.memoryTypeBits),
    };

    if ((res = vki.vkAllocateMemory(grDevice->device, &allocateInfo, NULL,
                                    &vkDeviceMemory)) != VK_SUCCESS) {
        LOGE("vkAllocateMemory failed\n");
        goto create_fail;
    }

    if ((res = vki.vkBindImageMemory(grDevice->device, vkImage, vkDeviceMemory, 0)) != VK_SUCCESS) {
        LOGE("vkBindImageMemory failed\n");
        goto create_fail;
    }

    const uint32_t rowPitchSize = align_u32(pCreateInfo->extent.width, 256);//TODO: calculate this
    const uint32_t bufferSize = align_u32(rowPitchSize * bytesPerPixel * pCreateInfo->extent.height, 4096);
    //create buffer to copy
    const VkBufferCreateInfo bufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };

    if ((res = vki.vkCreateBuffer(grDevice->device, &bufCreateInfo, NULL, &copyBuffer)) != VK_SUCCESS) {
        LOGE("vkCreateBuffer failed\n");
        goto create_fail;
    }

    VkMemoryRequirements bufferMemoryRequirements;
    vki.vkGetBufferMemoryRequirements(grDevice->device, copyBuffer , &bufferMemoryRequirements);

    const VkMemoryAllocateInfo bufMemoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = select_preferred_memory_type( &grDevice->memoryProperties,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                         bufferMemoryRequirements.memoryTypeBits),
    };

    if ((res = vki.vkAllocateMemory(grDevice->device, &bufMemoryAllocateInfo, NULL,
                                    &bufferDeviceMemory)) != VK_SUCCESS) {
        LOGE("vkAllocateMemory failed for copy buffer\n");
        goto create_fail;
    }
    if ((res = vki.vkBindBufferMemory(grDevice->device, copyBuffer, bufferDeviceMemory, 0)) != VK_SUCCESS) {
        LOGE("vkBindBufferMemory failed\n");
        goto create_fail;
    }
    void* bufferMemoryPtr = NULL;
    if ((res = vki.vkMapMemory(
             grDevice->device,
             bufferDeviceMemory,
             0,
             bufferSize,
             0,
             &bufferMemoryPtr)) != VK_SUCCESS) {
        LOGE("vkMapMemory failed\n");
        goto create_fail;
    }

    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    if ((res = buildCopyCommandBuffer(
             grDevice,
             vkImage,
             (VkExtent2D) {
                 .width = createInfo.extent.width,
                 .height = createInfo.extent.height,
             },
             copyBuffer,
             rowPitchSize,
             &cmdBuf)) != VK_SUCCESS) {
        LOGE("Copy command buffer creation failed");
        goto create_fail;
    }
    GrWsiImage* grImage = malloc(sizeof(GrWsiImage));
    *grImage = (GrWsiImage) {
        .sType = GR_STRUCT_TYPE_IMAGE,
        .image = vkImage,
        .extent = { createInfo.extent.width, createInfo.extent.height },
        .imageMemory = vkDeviceMemory,
        .fence = VK_NULL_HANDLE,
        .copyCmdBuf = cmdBuf,
        .copyBuffer = copyBuffer,
        .bufferMemory = bufferDeviceMemory,
        .bufferMemoryPtr = bufferMemoryPtr,
        .bufferSize = bufferSize,
        .bufferRowPitch = rowPitchSize,
        .format = format,
    };

    GrGpuMemory* grGpuMemory = malloc(sizeof(GrGpuMemory));
    *grGpuMemory = (GrGpuMemory) {
        .sType = GR_STRUCT_TYPE_GPU_MEMORY,
        .deviceMemory = vkDeviceMemory,
    };

    *pImage = (GR_IMAGE)grImage;
    *pMem = (GR_GPU_MEMORY)grGpuMemory;

    return GR_SUCCESS;
create_fail:
    if (vkImage != VK_NULL_HANDLE) {
        vki.vkDestroyImage(grDevice->device, vkImage, NULL);
    }
    if (vkDeviceMemory != VK_NULL_HANDLE) {
        vki.vkFreeMemory(grDevice->device, vkDeviceMemory, NULL);
    }
    if ( copyBuffer != VK_NULL_HANDLE) {
        vki.vkDestroyBuffer(grDevice->device, copyBuffer, NULL);
    }
    if (bufferDeviceMemory != VK_NULL_HANDLE) {
        vki.vkFreeMemory(grDevice->device, bufferDeviceMemory, NULL);
    }
    if (cmdBuf != VK_NULL_HANDLE) {
        vki.vkFreeCommandBuffers(grDevice->device,
                                 grDevice->universalCommandPool,
                                 1, &cmdBuf);
    }
    switch (res) {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    default:
        return GR_ERROR_UNKNOWN;
    }
}

GR_RESULT grWsiWinQueuePresent(
    GR_QUEUE queue,
    const GR_WSI_WIN_PRESENT_INFO* pPresentInfo)
{
    LOGT("%p %p\n", queue, pPresentInfo);
    GrQueue* grQueue = (GrQueue*)queue;
    GrWsiImage* srcGrImage = (GrWsiImage*)pPresentInfo->srcImage;
    VkResult vkRes;

    if (srcGrImage->fence == VK_NULL_HANDLE) {
        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        vkRes = vki.vkCreateFence(grQueue->grDevice->device, &fence_info,
                                  NULL,
                                  &srcGrImage->fence);
        if (vkRes != VK_SUCCESS)
            goto fail_present;
    } else {
        vkRes =
            vki.vkWaitForFences(grQueue->grDevice->device, 1, &srcGrImage->fence,
                                true, ~0ull);
        if (vkRes != VK_SUCCESS)
            goto fail_present;

        vkRes =
            vki.vkResetFences(grQueue->grDevice->device, 1, &srcGrImage->fence);
        if (vkRes != VK_SUCCESS)
            goto fail_present;
    }

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &srcGrImage->copyCmdBuf,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    vkRes = vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, srcGrImage->fence);
    if (vkRes != VK_SUCCESS) {
        goto fail_present;
    }

    wsiPresentBufferToHWND(pPresentInfo->hWndDest, srcGrImage->bufferMemoryPtr, srcGrImage->extent, srcGrImage->bufferRowPitch, srcGrImage->bufferSize, srcGrImage->format);

    return GR_SUCCESS;
fail_present:
    switch (vkRes) {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return GR_ERROR_OUT_OF_MEMORY;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return GR_ERROR_OUT_OF_GPU_MEMORY;
    case VK_ERROR_DEVICE_LOST:
        return GR_ERROR_DEVICE_LOST;
    case VK_TIMEOUT:
        return GR_TIMEOUT;
    default:
        break;
    }
    return GR_ERROR_UNKNOWN;
}
