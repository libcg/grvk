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
            .dstAccessMask = getVkAccessFlagsImage(GR_IMAGE_STATE_DATA_TRANSFER),
            .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
            .newLayout = getVkImageLayout(GR_IMAGE_STATE_DATA_TRANSFER),
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

    AcquireSRWLockExclusive(&grQueue->queueLock);
    VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
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

GrQueue* grQueueCreate(
    GrDevice* grDevice,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex)
{
    VkQueue vkQueue = VK_NULL_HANDLE;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[IMAGE_PREP_CMD_BUFFER_COUNT];

    VKD.vkGetDeviceQueue(grDevice->device, queueFamilyIndex, queueIndex, &vkQueue);

    // Allocate a pool of command buffers.
    // This will be used to transition images to the initial data transfer state
    const VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VKD.vkCreateCommandPool(grDevice->device, &poolCreateInfo, NULL, &vkCommandPool);

    const VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = COUNT_OF(commandBuffers),
    };

    VKD.vkAllocateCommandBuffers(grDevice->device, &allocateInfo, commandBuffers);

    GrQueue* grQueue = malloc(sizeof(GrQueue));
    *grQueue = (GrQueue) {
        .grObj = { GR_OBJ_TYPE_QUEUE, grDevice },
        .queue = vkQueue,
        .queueLock = SRWLOCK_INIT,
        .queueFamilyIndex = queueFamilyIndex,
        .globalMemRefCount = 0,
        .globalMemRefSize = 0,
        .globalMemRefs = NULL,
        .commandPool = vkCommandPool,
        .commandBuffers = { 0 }, // Initialized below
        .commandBufferIndex = 0,
    };
    memcpy(grQueue->commandBuffers, commandBuffers, sizeof(grQueue->commandBuffers));

    return grQueue;
}

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

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (queueId > 0) {
        // We don't expose more than one queue per type, just like the official driver
        return GR_ERROR_INVALID_ORDINAL;
    } else if (pQueue == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    *pQueue = NULL;

    switch ((GR_QUEUE_TYPE)queueType) {
    case GR_QUEUE_UNIVERSAL:
        *pQueue = (GR_QUEUE)grDevice->grUniversalQueue;
        break;
    case GR_QUEUE_COMPUTE:
        *pQueue = (GR_QUEUE)grDevice->grComputeQueue;
        break;
    }

    switch ((GR_EXT_QUEUE_TYPE)queueType) {
    case GR_EXT_QUEUE_DMA:
        *pQueue = (GR_QUEUE)grDevice->grDmaQueue;
        break;
    case GR_EXT_QUEUE_TIMER:
        break; // TODO implement
    }

    if (*pQueue == NULL) {
        return GR_ERROR_INVALID_QUEUE_TYPE;
    }

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

    AcquireSRWLockExclusive(&grQueue->queueLock);
    res = VKD.vkQueueSubmit(grQueue->queue, 1, &submitInfo, vkFence);
    ReleaseSRWLockExclusive(&grQueue->queueLock);

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

    AcquireSRWLockExclusive(&grQueue->queueLock);
    VkResult res = VKD.vkQueueWaitIdle(grQueue->queue);
    ReleaseSRWLockExclusive(&grQueue->queueLock);
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
    unsigned expectedSize = grQueue->globalMemRefCount * sizeof(GR_MEMORY_REF);
    if (expectedSize > grQueue->globalMemRefSize || expectedSize <= grQueue->globalMemRefSize / 2) {
        unsigned size = nextPowerOfTwo(grQueue->globalMemRefCount * sizeof(GR_MEMORY_REF));
        if (size != grQueue->globalMemRefSize) {
            grQueue->globalMemRefSize = size;
            grQueue->globalMemRefs = realloc(grQueue->globalMemRefs, grQueue->globalMemRefSize);
        }
    }

    memcpy(grQueue->globalMemRefs, pMemRefs, sizeof(GR_MEMORY_REF) * memRefCount);

    return GR_SUCCESS;
}
