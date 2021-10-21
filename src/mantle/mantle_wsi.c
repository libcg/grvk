#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"

typedef struct {
    VkImage dstImage;
    VkImage srcImage;
    VkCommandBuffer commandBuffer;
} CopyCommandBuffer;

static VkSurfaceKHR mSurface = VK_NULL_HANDLE;
static VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
static unsigned mPresentableImageCount = 0;
static GrImage** mPresentableImages;
static unsigned mImageCount = 0;
static VkImage* mImages;
static unsigned mCopyCommandBufferCount = 0;
static VkCommandPool mCommandPool = VK_NULL_HANDLE;
static CopyCommandBuffer* mCopyCommandBuffers = 0;
static VkSemaphore mAcquireSemaphore = VK_NULL_HANDLE;
static VkSemaphore mCopySemaphore = VK_NULL_HANDLE;

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
    VkResult res;

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = mCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    res = VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, &copyCmdBuf.commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed (%d)\n", res);
    }

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL
    };

    res = VKD.vkBeginCommandBuffer(copyCmdBuf.commandBuffer, &beginInfo);
    if (res != VK_SUCCESS) {
        LOGE("vkBeginCommandBuffer failed (%d)\n", res);
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

    VKD.vkCmdPipelineBarrier(copyCmdBuf.commandBuffer,
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

    VKD.vkCmdBlitImage(copyCmdBuf.commandBuffer,
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

    VKD.vkCmdPipelineBarrier(copyCmdBuf.commandBuffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // TODO optimize
                             0, 0, NULL, 0, NULL, 1, &postCopyBarrier);

    res = VKD.vkEndCommandBuffer(copyCmdBuf.commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed (%d)\n", res);
    }

    return copyCmdBuf;
}

static void initSwapchain(
    GrDevice* grDevice,
    HWND hwnd,
    unsigned queueFamilyIndex)
{
    VkResult res;

    const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = hwnd,
    };

    res = vki.vkCreateWin32SurfaceKHR(vk, &surfaceCreateInfo, NULL, &mSurface);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateWin32SurfaceKHR failed (%d)\n", res);
    }

    VkBool32 supported = VK_FALSE;
    res = vki.vkGetPhysicalDeviceSurfaceSupportKHR(grDevice->physicalDevice, queueFamilyIndex,
                                                   mSurface, &supported);
    if (res != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceSurfaceSupportKHR failed (%d)\n", res);
    } else if (!supported) {
        LOGW("unsupported surface\n");
    }

    // Get window dimensions
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    const VkExtent2D imageExtent = { clientRect.right, clientRect.bottom };

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

    res = VKD.vkCreateSwapchainKHR(grDevice->device, &swapchainCreateInfo, NULL, &mSwapchain);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed (%d)\n", res);
        return;
    }

    VKD.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mImageCount, NULL);
    mImages = malloc(sizeof(VkImage) * mImageCount);
    VKD.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mImageCount, mImages);

    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = grDevice->universalQueueFamilyIndex,
    };

    res = VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &mCommandPool);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed (%d)\n", res);
        return;
    }

    // Build copy command buffers for all image combinations
    for (int i = 0; i < mPresentableImageCount; i++) {
        const GrImage* presentImage = mPresentableImages[i];

        for (int j = 0; j < mImageCount; j++) {
            const VkExtent2D presentExtent = {
                presentImage->extent.width, presentImage->extent.height
            };

            mCopyCommandBufferCount++;
            mCopyCommandBuffers = realloc(mCopyCommandBuffers,
                                          sizeof(CopyCommandBuffer) * mCopyCommandBufferCount);
            mCopyCommandBuffers[mCopyCommandBufferCount - 1] =
                buildCopyCommandBuffer(grDevice, mImages[j], imageExtent,
                                       presentImage->image, presentExtent);
        }
    }

    const VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    res = VKD.vkCreateSemaphore(grDevice->device, &semaphoreCreateInfo, NULL, &mAcquireSemaphore);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed (%d)\n", res);
        return;
    }

    res = VKD.vkCreateSemaphore(grDevice->device, &semaphoreCreateInfo, NULL, &mCopySemaphore);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed (%d)\n", res);
        return;
    }
}

// Functions

GR_RESULT GR_STDCALL grWsiWinGetDisplays(
    GR_DEVICE device,
    GR_UINT* pDisplayCount,
    GR_WSI_WIN_DISPLAY* pDisplayList)
{
    LOGT("%p %p %p\n", device, pDisplayCount, pDisplayList);
    GrDevice* grDevice = (GrDevice*)device;
    unsigned displayCount = 1;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDisplayCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (pDisplayList == NULL) {
        *pDisplayCount = displayCount;
        return GR_SUCCESS;
    } else if (*pDisplayCount < displayCount) {
        LOGW("can't write display list, count %d, expected %d\n", *pDisplayCount, displayCount);
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    LOGW("semi-stub\n"); // TODO finish

    for (unsigned i = 0; i < displayCount; i++) {
        GrWsiWinDisplay* grWsiWinDisplay = malloc(sizeof(GrWsiWinDisplay));

        *grWsiWinDisplay = (GrWsiWinDisplay) {
            .grObj = { GR_OBJ_TYPE_WSI_WIN_DISPLAY, grDevice },
        };

        pDisplayList[i] = (GR_WSI_WIN_DISPLAY)grWsiWinDisplay;
    }

    *pDisplayCount = displayCount;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grWsiWinGetDisplayModeList(
    GR_WSI_WIN_DISPLAY display,
    GR_UINT* pDisplayModeCount,
    GR_WSI_WIN_DISPLAY_MODE* pDisplayModeList)
{
    LOGT("%p %p %p\n", display, pDisplayModeCount, pDisplayModeList);
    GrWsiWinDisplay* grWsiWinDisplay = (GrWsiWinDisplay*)display;
    unsigned displayModeCount = 1;

    if (grWsiWinDisplay == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grWsiWinDisplay) != GR_OBJ_TYPE_WSI_WIN_DISPLAY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDisplayModeCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (pDisplayModeList == NULL) {
        *pDisplayModeCount = displayModeCount;
        return GR_SUCCESS;
    } else if (*pDisplayModeCount < displayModeCount) {
        LOGW("can't write display mode list, count %d, expected %d\n",
             *pDisplayModeCount, displayModeCount);
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    LOGW("semi-stub\n"); // TODO finish

    *pDisplayModeList = (GR_WSI_WIN_DISPLAY_MODE) {
        .extent = { 1280, 720 },
        .format = { GR_CH_FMT_B8G8R8A8, GR_NUM_FMT_UNORM },
        .refreshRate = 60,
        .stereo = false,
        .crossDisplayPresent = true,
    };

    *pDisplayModeCount = displayModeCount;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grWsiWinTakeFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display,
    GR_IMAGE image)
{
    LOGT("%p %p\n", display, image);
    GrWsiWinDisplay* grWsiWinDisplay = (GrWsiWinDisplay*)display;

    if (grWsiWinDisplay == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grWsiWinDisplay) != GR_OBJ_TYPE_WSI_WIN_DISPLAY) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    LOGW("semi-stub\n"); // TODO finish
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grWsiWinCreatePresentableImage(
    GR_DEVICE device,
    const GR_WSI_WIN_PRESENTABLE_IMAGE_CREATE_INFO* pCreateInfo,
    GR_IMAGE* pImage,
    GR_GPU_MEMORY* pMem)
{
    LOGT("%p %p %p %p\n", device, pCreateInfo, pImage, pMem);
    GR_RESULT res;

    if (pCreateInfo->flags) {
        LOGW("unhandled flags 0x%X\n", pCreateInfo->flags);
    }

    const GR_IMAGE_CREATE_INFO grImageCreateInfo = {
        .imageType = GR_IMAGE_2D,
        .format = pCreateInfo->format,
        .extent = { pCreateInfo->extent.width, pCreateInfo->extent.height, 1 },
        .mipLevels = 1,
        .arraySize = 1,
        .samples = 1,
        .tiling = GR_OPTIMAL_TILING,
        .usage = pCreateInfo->usage,
        .flags = 0,
    };

    res = grCreateImage(device, &grImageCreateInfo, pImage);
    if (res != GR_SUCCESS) {
        return res;
    }

    GR_MEMORY_REQUIREMENTS memReqs;
    GR_SIZE memReqsSize = sizeof(memReqs);
    res = grGetObjectInfo(*pImage, GR_INFO_TYPE_MEMORY_REQUIREMENTS, &memReqsSize, &memReqs);
    if (res != GR_SUCCESS) {
        // TODO cleanup
        return res;
    }

    GR_MEMORY_ALLOC_INFO allocInfo = {
        .size = memReqs.size,
        .alignment = memReqs.alignment,
        .flags = 0,
        .heapCount = memReqs.heapCount,
        .heaps = { 0 }, // Initialized below
        .memPriority = GR_MEMORY_PRIORITY_HIGH,
    };

    for (unsigned i = 0; i < memReqs.heapCount; i++) {
        allocInfo.heaps[i] = memReqs.heaps[i];
    }

    res = grAllocMemory(device, &allocInfo, pMem);
    if (res != GR_SUCCESS) {
        // TODO cleanup
        return res;
    }

    res = grBindObjectMemory(*pImage, *pMem, 0);
    if (res != GR_SUCCESS) {
        // TODO cleanup
        return res;
    }

    // Keep track of presentable images to build copy command buffers in advance
    mPresentableImageCount++;
    mPresentableImages = realloc(mPresentableImages, sizeof(GrImage*) * mPresentableImageCount);
    mPresentableImages[mPresentableImageCount - 1] = (GrImage*)*pImage;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grWsiWinQueuePresent(
    GR_QUEUE queue,
    const GR_WSI_WIN_PRESENT_INFO* pPresentInfo)
{
    LOGT("%p %p\n", queue, pPresentInfo);
    GrQueue* grQueue = (GrQueue*)queue;
    VkResult vkRes;
    VkCommandBuffer vkCopyCommandBuffer = VK_NULL_HANDLE;

    // TODO validate args

    GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);
    GrImage* srcGrImage = (GrImage*)pPresentInfo->srcImage;

    if (mSwapchain == VK_NULL_HANDLE) {
        initSwapchain(grDevice, pPresentInfo->hWndDest, grQueue->queueFamilyIndex);
    }

    uint32_t vkImageIndex = 0;

    vkRes = VKD.vkAcquireNextImageKHR(grDevice->device, mSwapchain, UINT64_MAX,
                                      mAcquireSemaphore, VK_NULL_HANDLE, &vkImageIndex);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkAcquireNextImageKHR failed (%d)\n", vkRes);
        assert(vkRes != VK_ERROR_OUT_OF_DATE_KHR); // FIXME temporary hack to avoid log spam
        return GR_ERROR_OUT_OF_MEMORY;
    }

    // Find suitable copy command buffer for presentable and swapchain images combination
    for (int i = 0; i < mCopyCommandBufferCount; i++) {
        if (mCopyCommandBuffers[i].dstImage == mImages[vkImageIndex] &&
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

    EnterCriticalSection(grQueue->mutex);
    vkRes = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    LeaveCriticalSection(grQueue->mutex);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    const VkPresentInfoKHR vkPresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mCopySemaphore,
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &vkImageIndex,
        .pResults = NULL,
    };

    EnterCriticalSection(grQueue->mutex);
    vkRes = VKD.vkQueuePresentKHR(grQueue->queue, &vkPresentInfo);
    LeaveCriticalSection(grQueue->mutex);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR failed (%d)\n", vkRes);
        return GR_ERROR_OUT_OF_MEMORY; // TODO use better error code
    }

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grWsiWinSetMaxQueuedFrames(
    GR_DEVICE device,
    GR_UINT maxFrames)
{
    LOGT("%p %u\n", device, maxFrames);
    LOGW("semi-stub\n");

    return GR_SUCCESS;
}
