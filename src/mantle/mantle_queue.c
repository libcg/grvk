#include "mantle_internal.h"

// Keep track of images that need transition to the initial data transfer state
static unsigned mInitialImageCount = 0;
static GrImage** mInitialImages = NULL;
static SRWLOCK mInitialImagesLock = SRWLOCK_INIT;

static void prepareImagesForDataTransfer(
    GrQueue* grQueue,
    unsigned imageCount,
    GrImage** images)
{
    const GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

    // Pick the next command buffer (and hope it's not pending...)
    VkCommandBuffer vkCommandBuffer = grQueue->commandBuffers[grQueue->commandBufferIndex];
    grQueue->commandBufferIndex = (grQueue->commandBufferIndex + 1) % IMAGE_PREP_CMD_BUFFER_COUNT;

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    STACK_ARRAY(VkImageMemoryBarrier, barriers, 1024, imageCount);

    for (unsigned i = 0; i < imageCount; i++) {
        barriers[i] = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = getVkAccessFlagsImage(GR_IMAGE_STATE_DATA_TRANSFER, false),
            .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
            .newLayout = getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER, false),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[i]->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, // TODO handle depth-stencil
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
    }

    VKD.vkBeginCommandBuffer(vkCommandBuffer, &beginInfo);
    VKD.vkCmdPipelineBarrier(vkCommandBuffer,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             getVkPipelineStageFlagsImage(GR_IMAGE_STATE_DATA_TRANSFER),
                             0, 0, NULL, 0, NULL, imageCount, barriers);
    VKD.vkEndCommandBuffer(vkCommandBuffer);

    STACK_ARRAY_FINISH(barriers);

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &vkCommandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    AcquireSRWLockExclusive(grQueue->lock);
    VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    ReleaseSRWLockExclusive(grQueue->lock);
}

static void checkMemoryReferences(
    GrQueue* grQueue,
    unsigned memRefCount,
    const GR_MEMORY_REF* memRefs)
{
    AcquireSRWLockExclusive(&mInitialImagesLock);

    unsigned imageCount = 0;
    STACK_ARRAY(GrImage*, images, 1024, mInitialImageCount);

    // Check if the initial images are bound to the memory references of this submission
    for (unsigned i = 0; i < mInitialImageCount; i++) {
        GrImage* grImage = mInitialImages[i];
        bool found = false;

        for (unsigned j = 0; !found && j < memRefCount; j++) {
            if (grImage->grObj.grGpuMemory == memRefs[j].mem) {
                found = true;
            }
        }
        for (unsigned j = 0; !found && j < grQueue->globalMemRefCount; j++) {
            if (grImage->grObj.grGpuMemory == grQueue->globalMemRefs[j].mem) {
                found = true;
            }
        }

        if (found) {
            images[imageCount] = grImage;
            imageCount++;

            // Remove initial image entry (skip realloc)
            mInitialImageCount--;
            memmove(&mInitialImages[i], &mInitialImages[i + 1],
                    (mInitialImageCount - i) * sizeof(GrImage*));
            i--;
        }
    }

    ReleaseSRWLockExclusive(&mInitialImagesLock);

    // Perform data transfer state transition
    if (imageCount > 0) {
        prepareImagesForDataTransfer(grQueue, imageCount, images);
    }

    STACK_ARRAY_FINISH(images);
}

// Exported functions

void grQueueAddInitialImage(
    GrImage* grImage)
{
    AcquireSRWLockExclusive(&mInitialImagesLock);

    mInitialImageCount++;
    mInitialImages = realloc(mInitialImages, mInitialImageCount * sizeof(GrImage*));
    mInitialImages[mInitialImageCount - 1] = grImage;

    ReleaseSRWLockExclusive(&mInitialImagesLock);
}

void grQueueRemoveInitialImage(
    GrImage* grImage)
{
    AcquireSRWLockExclusive(&mInitialImagesLock);

    for (unsigned i = 0; i < mInitialImageCount; i++) {
        if (grImage == mInitialImages[i]) {
            // Remove initial image entry (skip realloc)
            mInitialImageCount--;
            memmove(&mInitialImages[i], &mInitialImages[i + 1],
                    (mInitialImageCount - i) * sizeof(GrImage*));
            break;
        }
    }

    ReleaseSRWLockExclusive(&mInitialImagesLock);
}

// Queue Functions

GR_RESULT GR_STDCALL grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    LOGT("%p 0x%X %u %p\n", device, queueType, queueId, pQueue);
    GrDevice* grDevice = (GrDevice*)device;
    VkQueue vkQueue = VK_NULL_HANDLE;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;

    unsigned queueFamilyIndex = grDeviceGetQueueFamilyIndex(grDevice, queueType);
    if (queueFamilyIndex == INVALID_QUEUE_INDEX) {
        LOGE("invalid index %d for queue type %d\n", queueFamilyIndex, queueType);
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

    VKD.vkGetDeviceQueue(grDevice->device, queueFamilyIndex, queueId, &vkQueue);

    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &vkCommandPool);

    // FIXME technically, this object should be created in advance so it doesn't get duplicated
    // when the application requests it multiple times
    GrQueue* grQueue = malloc(sizeof(GrQueue));
    *grQueue = (GrQueue) {
        .grObj = { GR_OBJ_TYPE_QUEUE, grDevice },
        .queue = vkQueue,
        .queueFamilyIndex = queueFamilyIndex,
        .globalMemRefCount = 0,
        .globalMemRefs = NULL,
        .lock = grDeviceGetQueueLock(grDevice, queueType),
        .commandPool = vkCommandPool,
        .commandBuffers = { 0 }, // Initialized below
        .commandBufferIndex = 0,
    };

    // Allocate a command buffer pool for transitioning images to the initial data transfer state
    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = grQueue->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = IMAGE_PREP_CMD_BUFFER_COUNT,
    };

    VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, grQueue->commandBuffers);

    *pQueue = (GR_QUEUE)grQueue;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grQueueSubmit(
    GR_QUEUE queue,
    GR_UINT cmdBufferCount,
    const GR_CMD_BUFFER* pCmdBuffers,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs,
    GR_FENCE fence)
{
    LOGT("%p %u %p %u %p %p\n", queue, cmdBufferCount, pCmdBuffers, memRefCount, pMemRefs, fence);
    GrQueue* grQueue = (GrQueue*)queue;
    GrFence* grFence = (GrFence*)fence;
    VkFence vkFence = VK_NULL_HANDLE;
    VkResult res;

    // TODO validate args

    GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

    checkMemoryReferences(grQueue, memRefCount, pMemRefs);

    if (grFence != NULL) {
        vkFence = grFence->fence;

        res = VKD.vkResetFences(grDevice->device, 1, &vkFence);
        if (res != VK_SUCCESS) {
            LOGE("vkResetFences failed (%d)\n", res);
            return getGrResult(res);
        }

        grFence->submitted = true;
    }

    STACK_ARRAY(VkCommandBuffer, vkCommandBuffers, 1024, cmdBufferCount);

    for (unsigned i = 0; i < cmdBufferCount; i++) {
        GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)pCmdBuffers[i];

        grCmdBuffer->submitFence = grFence;
        vkCommandBuffers[i] = grCmdBuffer->commandBuffer;
    }

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = cmdBufferCount,
        .pCommandBuffers = vkCommandBuffers,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    AcquireSRWLockExclusive(grQueue->lock);
    res = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, vkFence);
    ReleaseSRWLockExclusive(grQueue->lock);

    STACK_ARRAY_FINISH(vkCommandBuffers);

    if (res != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grQueueWaitIdle(
    GR_QUEUE queue)
{
    LOGT("%p\n", queue);
    GrQueue* grQueue = (GrQueue*)queue;
    GrDevice* grDevice = GET_OBJ_DEVICE(grQueue);

    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    AcquireSRWLockExclusive(grQueue->lock);
    VkResult res = VKD.vkQueueWaitIdle(grQueue->queue);
    ReleaseSRWLockExclusive(grQueue->lock);
    if (res != VK_SUCCESS) {
        LOGE("vkQueueWaitIdle failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grDeviceWaitIdle(
    GR_DEVICE device)
{
    LOGT("%p\n", device);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VkResult res = VKD.vkDeviceWaitIdle(grDevice->device);
    if (res != VK_SUCCESS) {
        LOGE("vkDeviceWaitIdle failed (%d)\n", res);
    }

    return getGrResult(res);
}

GR_RESULT GR_STDCALL grQueueSetGlobalMemReferences(
    GR_QUEUE queue,
    GR_UINT memRefCount,
    const GR_MEMORY_REF* pMemRefs)
{
    LOGT("%p %u %p\n", queue, memRefCount, pMemRefs);
    GrQueue* grQueue = (GrQueue*)queue;

    if (grQueue == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grQueue) != GR_OBJ_TYPE_QUEUE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (memRefCount > 0 && pMemRefs == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    grQueue->globalMemRefCount = memRefCount;
    grQueue->globalMemRefs = (GR_MEMORY_REF*)pMemRefs;

    return GR_SUCCESS;
}
