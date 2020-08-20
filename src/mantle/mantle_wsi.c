#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"

typedef struct {
    VkImage image;
    VkExtent2D extent;
} PresentableImage;

typedef struct {
    VkImage dstImage;
    VkImage srcImage;
    VkCommandBuffer commandBuffer;
} CopyCommandBuffer;

static VkSurfaceKHR mSurface = VK_NULL_HANDLE;
static VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
static unsigned mPresentableImageCount = 0;
static PresentableImage* mPresentableImages;
static unsigned mImageCount = 0;
static VkImage* mImages;
static unsigned mCopyCommandBufferCount = 0;
static CopyCommandBuffer* mCopyCommandBuffers = 0;
static VkSemaphore mAcquireSemaphore = VK_NULL_HANDLE;
static VkSemaphore mCopySemaphore = VK_NULL_HANDLE;

static uint32_t getMemoryTypeIndex(
    uint32_t memoryTypeBits)
{
    // Index corresponds to the location of the LSB
    // TODO support MSVC
    return __builtin_ctz(memoryTypeBits);
}

static CopyCommandBuffer buildCopyCommandBuffer(
    const GrDevice* grDevice,
    VkImage dstImage,
    VkExtent2D dstExtent,
    VkImage srcImage,
    VkExtent2D srcExtent)
{
    CopyCommandBuffer copyCmdBuf = {
        .dstImage = dstImage,
        .srcImage = srcImage,
        .commandBuffer = VK_NULL_HANDLE,
    };

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = grDevice->universalCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vki.vkAllocateCommandBuffers(grDevice->device, &allocateInfo,
                                     &copyCmdBuf.commandBuffer) != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed\n");
    }

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };

    if (vki.vkBeginCommandBuffer(copyCmdBuf.commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOGE("vkBeginCommandBuffer failed\n");
    }

    const VkImageMemoryBarrier preCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = dstImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vki.vkCmdPipelineBarrier(copyCmdBuf.commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, 1, &preCopyBarrier);

    const VkImageBlit region = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets[0] = { 0, 0, 0 },
        .srcOffsets[1] = { srcExtent.width, srcExtent.height, 1 },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets[0] = { 0, 0, 0 },
        .dstOffsets[1] = { dstExtent.width, dstExtent.height, 1 },
    };

    vki.vkCmdBlitImage(copyCmdBuf.commandBuffer,
                       srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region, VK_FILTER_NEAREST);

    const VkImageMemoryBarrier postCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = dstImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vki.vkCmdPipelineBarrier(copyCmdBuf.commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, 1, &postCopyBarrier);

    if (vki.vkEndCommandBuffer(copyCmdBuf.commandBuffer) != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed\n");
    }

    return copyCmdBuf;
}

static void initSwapchain(
    GrDevice* grDevice,
    HWND hwnd,
    uint32_t queueIndex)
{
    const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = hwnd,
    };

    if (vki.vkCreateWin32SurfaceKHR(vk, &surfaceCreateInfo, NULL, &mSurface) != VK_SUCCESS) {
        LOGE("vkCreateWin32SurfaceKHR failed\n");
    }

    VkBool32 supported = VK_FALSE;
    if (vki.vkGetPhysicalDeviceSurfaceSupportKHR(grDevice->physicalDevice, queueIndex, mSurface,
                                                 &supported) != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceSurfaceSupportKHR failed\n");
    }
    if (!supported) {
        LOGW("unsupported surface\n");
    }

    // Get window dimensions
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    VkExtent2D imageExtent = { clientRect.right, clientRect.bottom };

    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .surface = mSurface,
        .minImageCount = 3,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = imageExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    if (vki.vkCreateSwapchainKHR(grDevice->device, &swapchainCreateInfo, NULL,
                                 &mSwapchain) != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed\n");
        return;
    }

    vki.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mImageCount, NULL);
    mImages = malloc(sizeof(VkImage) * mImageCount);
    vki.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mImageCount, mImages);

    // Build copy command buffers for all image combinations
    for (int i = 0; i < mPresentableImageCount; i++) {
        const PresentableImage* presentImage = &mPresentableImages[i];

        for (int j = 0; j < mImageCount; j++) {
            mCopyCommandBufferCount++;
            mCopyCommandBuffers = realloc(mCopyCommandBuffers,
                                          sizeof(CopyCommandBuffer) * mCopyCommandBufferCount);
            mCopyCommandBuffers[mCopyCommandBufferCount - 1] =
                buildCopyCommandBuffer(grDevice, mImages[j], imageExtent,
                                       presentImage->image, presentImage->extent);
        }
    }

    const VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    vki.vkCreateSemaphore(grDevice->device, &semaphoreCreateInfo, NULL, &mAcquireSemaphore);
    vki.vkCreateSemaphore(grDevice->device, &semaphoreCreateInfo, NULL, &mCopySemaphore);
}

// Functions

GR_RESULT grWsiWinCreatePresentableImage(
    GR_DEVICE device,
    const GR_WSI_WIN_PRESENTABLE_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    GrDevice* grDevice = (GrDevice*)device;
    VkImage vkImage = VK_NULL_HANDLE;
    VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;

    const VkImageCreateInfo createInfo = {
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
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vki.vkCreateImage(grDevice->device, &createInfo, NULL, &vkImage) != VK_SUCCESS) {
        LOGE("vkCreateImage failed\n");
        return GR_ERROR_INVALID_VALUE;
    }

    VkMemoryRequirements memoryRequirements;
    vki.vkGetImageMemoryRequirements(grDevice->device, vkImage, &memoryRequirements);

    const VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = getMemoryTypeIndex(memoryRequirements.memoryTypeBits),
    };

    if (vki.vkAllocateMemory(grDevice->device, &allocateInfo, NULL,
                             &vkDeviceMemory) != VK_SUCCESS) {
        LOGE("vkAllocateMemory failed\n");
        vki.vkDestroyImage(grDevice->device, vkImage, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    if (vki.vkBindImageMemory(grDevice->device, vkImage, vkDeviceMemory, 0) != VK_SUCCESS) {
        LOGE("vkBindImageMemory failed\n");
        vki.vkFreeMemory(grDevice->device, vkDeviceMemory, NULL);
        vki.vkDestroyImage(grDevice->device, vkImage, NULL);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    const PresentableImage presentImage = {
        .image = vkImage,
        .extent = { pCreateInfo->extent.width, pCreateInfo->extent.height },
    };

    // Keep track of presentable images to build copy command buffers in advance
    mPresentableImageCount++;
    mPresentableImages = realloc(mPresentableImages,
                                 sizeof(PresentableImage) * mPresentableImageCount);
    mPresentableImages[mPresentableImageCount - 1] = presentImage;

    GrImage* grImage = malloc(sizeof(GrImage));
    *grImage = (GrImage) {
        .sType = GR_STRUCT_TYPE_IMAGE,
        .image = vkImage,
        .extent = { createInfo.extent.width, createInfo.extent.height },
    };

    GrGpuMemory* grGpuMemory = malloc(sizeof(GrGpuMemory));
    *grGpuMemory = (GrGpuMemory) {
        .sType = GR_STRUCT_TYPE_GPU_MEMORY,
        .deviceMemory = vkDeviceMemory,
    };

    *pImage = (GR_IMAGE)grImage;
    *pMem = (GR_GPU_MEMORY)grGpuMemory;

    return GR_SUCCESS;
}

GR_RESULT grWsiWinQueuePresent(
    GR_QUEUE queue,
    const GR_WSI_WIN_PRESENT_INFO* pPresentInfo)
{
    GrQueue* grQueue = (GrQueue*)queue;
    GrImage* srcGrImage = (GrImage*)pPresentInfo->srcImage;
    VkResult vkRes;
    VkCommandBuffer vkCopyCommandBuffer = VK_NULL_HANDLE;

    if (mSwapchain == VK_NULL_HANDLE) {
        initSwapchain(grQueue->grDevice, pPresentInfo->hWndDest, grQueue->queueIndex);
    }

    uint32_t imageIndex = 0;

    vkRes = vki.vkAcquireNextImageKHR(grQueue->grDevice->device, mSwapchain, UINT64_MAX,
                                      mAcquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkAcquireNextImageKHR failed (%d)\n", vkRes);
        assert(vkRes != VK_ERROR_OUT_OF_DATE_KHR); // FIXME temporary hack to avoid log spam
        return GR_ERROR_OUT_OF_MEMORY;
    }

    // Find suitable copy command buffer for presentable and swapchain images combination
    for (int i = 0; i < mCopyCommandBufferCount; i++) {
        if (mCopyCommandBuffers[i].dstImage == mImages[imageIndex] &&
            mCopyCommandBuffers[i].srcImage == srcGrImage->image) {
            vkCopyCommandBuffer = mCopyCommandBuffers[i].commandBuffer;
            break;
        }
    }
    assert(vkCopyCommandBuffer != VK_NULL_HANDLE);

    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mAcquireSemaphore,
        .pWaitDstStageMask = &stageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &vkCopyCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &mCopySemaphore,
    };

    if (vki.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed\n");
        return GR_ERROR_OUT_OF_MEMORY;
    }

    const VkPresentInfoKHR vkPresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mCopySemaphore,
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &imageIndex,
        .pResults = NULL,
    };

    if (vki.vkQueuePresentKHR(grQueue->queue, &vkPresentInfo) != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR failed\n");
        return GR_ERROR_OUT_OF_MEMORY; // TODO use better error code
    }

    return GR_SUCCESS;
}
