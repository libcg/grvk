#include "mantle/mantleWsiWinExt.h"
#include "mantle_internal.h"

#define MAX_MONITORS (16) // This ought to be enough for anybody

typedef struct {
    unsigned monitorCount;
    HMONITOR monitors[MAX_MONITORS];
} MonitorList;

typedef struct {
    VkImage dstImage;
    VkImage srcImage;
    VkCommandBuffer commandBuffer;
} CopyCommandBuffer;

typedef struct {
    GrImage* grImage;
    GrGpuMemory* grGpuMemory;
    bool isDestroyed;
} PresentableImage;

static VkSurfaceKHR mSurface = VK_NULL_HANDLE;
static VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
static bool mDirtySwapchain = true;
static unsigned mPresentInterval = 0;
static unsigned mPresentableImageCount = 0;
static PresentableImage* mPresentableImages = NULL;
static SRWLOCK mPresentableImagesLock = SRWLOCK_INIT;
static unsigned mSwapchainImageCount = 0;
static VkImage* mSwapchainImages = NULL;
static unsigned mCopyCommandBufferCount = 0;
static VkCommandPool mCommandPool = VK_NULL_HANDLE;
static CopyCommandBuffer* mCopyCommandBuffers = 0;
static VkSemaphore* mAcquireSemaphores = NULL;
static VkSemaphore* mCopySemaphores = NULL;
static unsigned mFrameIndex = 0;

static int __stdcall countDisplaysProc(
  HMONITOR hMonitor,
  HDC hdc,
  LPRECT pRect,
  LPARAM lParam)
{
    unsigned* displayCount = (unsigned*)lParam;

    (*displayCount)++;

    return TRUE;
}

static int __stdcall getDisplaysProc(
  HMONITOR hMonitor,
  HDC hdc,
  LPRECT pRect,
  LPARAM lParam)
{
    MonitorList* monitorList = (MonitorList*)lParam;

    monitorList->monitors[monitorList->monitorCount] = hMonitor;
    monitorList->monitorCount++;

    return TRUE;
}

static VkSemaphore getVkSemaphore(
    const GrDevice* grDevice)
{
    VkResult vkRes;
    VkSemaphore vkSemaphore;

    const VkSemaphoreCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    vkRes = VKD.vkCreateSemaphore(grDevice->device, &createInfo, NULL, &vkSemaphore);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateSemaphore failed (%d)\n", vkRes);
        assert(false);
    }

    return vkSemaphore;
}

static VkSurfaceKHR getVkSurface(
    const GrDevice* grDevice,
    HWND hwnd,
    unsigned queueFamilyIndex)
{
    VkResult vkRes;
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .hinstance = GetModuleHandle(NULL),
        .hwnd = hwnd,
    };

    vkRes = vki.vkCreateWin32SurfaceKHR(vk, &surfaceCreateInfo, NULL, &vkSurface);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateWin32SurfaceKHR failed (%d)\n", vkRes);
    }

    VkBool32 isSupported = VK_FALSE;
    vkRes = vki.vkGetPhysicalDeviceSurfaceSupportKHR(grDevice->physicalDevice, queueFamilyIndex,
                                                     vkSurface, &isSupported);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkGetPhysicalDeviceSurfaceSupportKHR failed (%d)\n", vkRes);
    } else if (!isSupported) {
        LOGW("unsupported surface\n");
    }

    return vkSurface;
}

static void recordCopyCommandBuffer(
    const GrDevice* grDevice,
    VkCommandBuffer commandBuffer,
    VkImage dstImage,
    VkExtent2D dstExtent,
    VkImage srcImage,
    VkExtent2D srcExtent)
{
    VkResult res;

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };

    res = VKD.vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (res != VK_SUCCESS) {
        LOGE("vkBeginCommandBuffer failed (%d)\n", res);
    }

    const VkImageMemoryBarrier preCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
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
        },
    };

    VKD.vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
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

    VKD.vkCmdBlitImage(commandBuffer,
                       srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &region, VK_FILTER_NEAREST);

    const VkImageMemoryBarrier postCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
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
        },
    };

    VKD.vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, NULL, 0, NULL, 1, &postCopyBarrier);

    res = VKD.vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS) {
        LOGE("vkEndCommandBuffer failed (%d)\n", res);
    }
}

static void recreateSwapchain(
    GrDevice* grDevice,
    HWND hwnd,
    unsigned queueFamilyIndex,
    VkPresentModeKHR presentMode)
{
    VkResult res;

    // Get surface
    if (mSurface == VK_NULL_HANDLE) {
        mSurface = getVkSurface(grDevice, hwnd, queueFamilyIndex);
    }

    // Get window dimensions
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    const VkExtent2D imageExtent = { clientRect.right, clientRect.bottom };

    // Recreate swapchain
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
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = mSwapchain,
    };

    res = VKD.vkCreateSwapchainKHR(grDevice->device, &swapchainCreateInfo, NULL, &mSwapchain);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed (%d)\n", res);
        return;
    }

    // Get swapchain images
    unsigned lastImageCount = mSwapchainImageCount;
    VKD.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mSwapchainImageCount, NULL);
    mSwapchainImages = realloc(mSwapchainImages, sizeof(VkImage) * mSwapchainImageCount);
    VKD.vkGetSwapchainImagesKHR(grDevice->device, mSwapchain, &mSwapchainImageCount,
                                mSwapchainImages);

    // Recreate command pool
    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VKD.vkDestroyCommandPool(grDevice->device, mCommandPool, NULL);
    res = VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &mCommandPool);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed (%d)\n", res);
        return;
    }

    // Free up destroyed presentable images memory
    AcquireSRWLockExclusive(&mPresentableImagesLock);

    for (unsigned i = 0; i < mPresentableImageCount; i++) {
        if (mPresentableImages[i].isDestroyed) {
            grFreeMemory((GR_GPU_MEMORY)mPresentableImages[i].grGpuMemory);

            // Remove slot
            mPresentableImageCount--;
            memmove(&mPresentableImages[i], &mPresentableImages[i + 1],
                    (mPresentableImageCount - i) * sizeof(PresentableImage));
            i--;
        }
    }

    // Build copy command buffers for all image combinations
    mCopyCommandBufferCount = mSwapchainImageCount * mPresentableImageCount;
    mCopyCommandBuffers = realloc(mCopyCommandBuffers,
                                  mCopyCommandBufferCount * sizeof(CopyCommandBuffer));
    STACK_ARRAY(VkCommandBuffer, commandBuffers, 16, mCopyCommandBufferCount);

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = mCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = mCopyCommandBufferCount,
    };

    res = VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, commandBuffers);
    if (res != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed (%d)\n", res);
    }

    for (unsigned i = 0; i < mPresentableImageCount; i++) {
        const GrImage* presentImage = mPresentableImages[i].grImage;
        const VkExtent2D presentExtent = {
            presentImage->extent.width, presentImage->extent.height
        };

        for (unsigned j = 0; j < mSwapchainImageCount; j++) {
            unsigned idx = i * mSwapchainImageCount + j;

            recordCopyCommandBuffer(grDevice, commandBuffers[idx],
                                    mSwapchainImages[j], imageExtent,
                                    presentImage->image, presentExtent);

            mCopyCommandBuffers[idx] = (CopyCommandBuffer) {
                .dstImage = mSwapchainImages[j],
                .srcImage = presentImage->image,
                .commandBuffer = commandBuffers[idx],
            };
        }
    }

    ReleaseSRWLockExclusive(&mPresentableImagesLock);
    STACK_ARRAY_FINISH(commandBuffers);

    // Create one acquire/copy semaphore per image. Old semaphores are reused when recreating the
    // swapchain which can cause conflicts, but it resolves itself quickly
    mAcquireSemaphores = realloc(mAcquireSemaphores, mSwapchainImageCount * sizeof(VkSemaphore));
    mCopySemaphores = realloc(mCopySemaphores, mSwapchainImageCount * sizeof(VkSemaphore));
    for (unsigned i = lastImageCount; i < mSwapchainImageCount; i++) {
        mAcquireSemaphores[i] = getVkSemaphore(grDevice);
        mCopySemaphores[i] = getVkSemaphore(grDevice);
    }
}

// Exported Functions

void grWsiDestroyImage(
    GrImage* grImage)
{
    AcquireSRWLockExclusive(&mPresentableImagesLock);
    for (unsigned i = 0; i < mPresentableImageCount; i++) {
        if (grImage == mPresentableImages[i].grImage) {
            // FIXME we'd want to free up memory here, but grFreeMemory crashes..?
            mPresentableImages[i].isDestroyed = true;
            mDirtySwapchain = true;
            break;
        }
    }
    ReleaseSRWLockExclusive(&mPresentableImagesLock);
}

// Functions

GR_RESULT GR_STDCALL grWsiWinGetDisplays(
    GR_DEVICE device,
    GR_UINT* pDisplayCount,
    GR_WSI_WIN_DISPLAY* pDisplayList)
{
    LOGT("%p %p %p\n", device, pDisplayCount, pDisplayList);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDisplayCount == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    unsigned displayCount = 0;
    EnumDisplayMonitors(NULL, NULL, countDisplaysProc, (LPARAM)&displayCount);

    if (pDisplayList == NULL) {
        *pDisplayCount = displayCount;
        return GR_SUCCESS;
    } else if (*pDisplayCount < displayCount) {
        LOGW("can't write display list, count %d, expected %d\n", *pDisplayCount, displayCount);
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }

    MonitorList monitorList = { 0 };
    EnumDisplayMonitors(NULL, NULL, getDisplaysProc, (LPARAM)&monitorList);

    for (unsigned i = 0; i < monitorList.monitorCount; i++) {
        GrWsiWinDisplay* grWsiWinDisplay = malloc(sizeof(GrWsiWinDisplay));

        *grWsiWinDisplay = (GrWsiWinDisplay) {
            .grObj = { GR_OBJ_TYPE_WSI_WIN_DISPLAY, grDevice },
            .hMonitor = monitorList.monitors[i],
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

GR_RESULT GR_STDCALL grWsiWinReleaseFullscreenOwnership(
    GR_WSI_WIN_DISPLAY display)
{
    LOGT("%p\n", display);
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
        .flags = GR_IMAGE_CREATE_VIEW_FORMAT_CHANGE,
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
    AcquireSRWLockExclusive(&mPresentableImagesLock);
    mPresentableImageCount++;
    mPresentableImages = realloc(mPresentableImages,
                                 mPresentableImageCount * sizeof(PresentableImage));
    mPresentableImages[mPresentableImageCount - 1] = (PresentableImage) {
        .grImage = (GrImage*)*pImage,
        .grGpuMemory = (GrGpuMemory*)*pMem,
        .isDestroyed = false,
    };
    ReleaseSRWLockExclusive(&mPresentableImagesLock);

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

    // TODO handle presentInterval > 1 properly
    if (mDirtySwapchain || pPresentInfo->presentInterval != mPresentInterval) {
        recreateSwapchain(grDevice, pPresentInfo->hWndDest, grQueue->queueFamilyIndex,
                          pPresentInfo->presentInterval == 0 ? VK_PRESENT_MODE_IMMEDIATE_KHR
                                                             : VK_PRESENT_MODE_FIFO_KHR);
        mDirtySwapchain = false;
        mPresentInterval = pPresentInfo->presentInterval;
    }

    VkSemaphore acquireSemaphore = mAcquireSemaphores[mFrameIndex % mSwapchainImageCount];
    VkSemaphore copySemaphore = mCopySemaphores[mFrameIndex % mSwapchainImageCount];
    mFrameIndex++;

    uint32_t vkImageIndex = 0;
    vkRes = VKD.vkAcquireNextImageKHR(grDevice->device, mSwapchain, UINT64_MAX,
                                      acquireSemaphore, VK_NULL_HANDLE, &vkImageIndex);
    if (vkRes == VK_SUBOPTIMAL_KHR) {
        // The swapchain needs to be recreated, but we can still present
        mDirtySwapchain = true;
    } else if (vkRes == VK_ERROR_OUT_OF_DATE_KHR) {
        // The swapchain needs to be recreated, skip this present
        mDirtySwapchain = true;
        return GR_SUCCESS;
    } else if (vkRes != VK_SUCCESS) {
        LOGE("vkAcquireNextImageKHR failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    // Find suitable copy command buffer for this combination of presentable and swapchain images
    for (unsigned i = 0; i < mCopyCommandBufferCount; i++) {
        if (mCopyCommandBuffers[i].dstImage == mSwapchainImages[vkImageIndex] &&
            mCopyCommandBuffers[i].srcImage == srcGrImage->image) {
            vkCopyCommandBuffer = mCopyCommandBuffers[i].commandBuffer;
            break;
        }
    }
    if (vkCopyCommandBuffer == VK_NULL_HANDLE) {
        // This presentable image isn't known, skip this present
        mDirtySwapchain = true;
        return GR_SUCCESS;
    }

    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquireSemaphore,
        .pWaitDstStageMask = &stageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &vkCopyCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &copySemaphore,
    };

    AcquireSRWLockExclusive(&grQueue->queueLock);
    vkRes = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", vkRes);
        return getGrResult(vkRes);
    }

    const VkPresentInfoKHR vkPresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &copySemaphore,
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &vkImageIndex,
        .pResults = NULL,
    };

    AcquireSRWLockExclusive(&grQueue->queueLock);
    vkRes = VKD.vkQueuePresentKHR(grQueue->queue, &vkPresentInfo);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
    if (vkRes == VK_SUBOPTIMAL_KHR || vkRes == VK_ERROR_OUT_OF_DATE_KHR) {
        mDirtySwapchain = true;
    } else if (vkRes != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR failed (%d)\n", vkRes);
        return getGrResult(vkRes);
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
