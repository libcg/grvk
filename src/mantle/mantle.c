#include <stdio.h>
#include <stdlib.h>
#include "mantle_internal.h"

static GR_ALLOC_FUNCTION mAllocFun = NULL;
static GR_FREE_FUNCTION mFreeFun = NULL;
static VkInstance mVkInstance = VK_NULL_HANDLE;

// Initialization and Device Functions

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    printf("%s: app %s (%08X), engine %s (%08X), api %08X\n", __func__,
           pAppInfo->pAppName, pAppInfo->appVersion,
           pAppInfo->pEngineName, pAppInfo->engineVersion,
           pAppInfo->apiVersion);

    if (pAllocCb == NULL) {
        mAllocFun = grvkAlloc;
        mFreeFun = grvkFree;
    } else {
        mAllocFun = pAllocCb->pfnAlloc;
        mFreeFun = pAllocCb->pfnFree;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = pAppInfo->pAppName,
        .applicationVersion = pAppInfo->appVersion,
        .pEngineName = pAppInfo->pEngineName,
        .engineVersion = pAppInfo->engineVersion,
        .apiVersion = VK_API_VERSION_1_1,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
    };

    if (vkCreateInstance(&createInfo, NULL, &mVkInstance) != VK_SUCCESS) {
        printf("%s: vkCreateInstance failed\n", __func__);
        return GR_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > GR_MAX_PHYSICAL_GPUS) {
        physicalDeviceCount = GR_MAX_PHYSICAL_GPUS;
    }

    VkPhysicalDevice physicalDevices[GR_MAX_PHYSICAL_GPUS];
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, physicalDevices);

    *pGpuCount = physicalDeviceCount;
    for (int i = 0; i < *pGpuCount; i++) {
        gpus[i] = (GR_PHYSICAL_GPU)physicalDevices[i];
    }

    return GR_SUCCESS;
}

GR_RESULT grCreateDevice(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice)
{
    GR_RESULT res = GR_SUCCESS;
    VkPhysicalDevice physicalDevice = (VkPhysicalDevice)gpu;
    uint32_t universalQueueIndex = -1;
    uint32_t universalQueueCount = 0;
    uint32_t computeQueueIndex = -1;
    uint32_t computeQueueCount = 0;

    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, NULL);

    VkQueueFamilyProperties* queueFamilyProperties =
        malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount,
                                             queueFamilyProperties);

    for (int i = 0; i < queueFamilyPropertyCount; i++) {
        const VkQueueFamilyProperties* queueFamilyProperty = &queueFamilyProperties[i];

        if ((queueFamilyProperty->queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            universalQueueIndex = i;
            universalQueueCount = queueFamilyProperty->queueCount;
        } else if ((queueFamilyProperty->queueFlags & VK_QUEUE_COMPUTE_BIT) ==
                   VK_QUEUE_COMPUTE_BIT) {
            computeQueueIndex = i;
            computeQueueCount = queueFamilyProperty->queueCount;
        }
    }

    VkDeviceQueueCreateInfo* queueCreateInfos =
        malloc(sizeof(VkDeviceQueueCreateInfo) * pCreateInfo->queueRecordCount);
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        const GR_DEVICE_QUEUE_CREATE_INFO* requestedQueue = &pCreateInfo->pRequestedQueues[i];

        float* queuePriorities = malloc(sizeof(float) * requestedQueue->queueCount);

        for (int j = 0; j < requestedQueue->queueCount; j++) {
            queuePriorities[j] = 1.0f; // Max priority
        }

        if ((requestedQueue->queueType == GR_QUEUE_UNIVERSAL &&
             requestedQueue->queueCount != universalQueueCount) ||
            (requestedQueue->queueType == GR_QUEUE_COMPUTE &&
             requestedQueue->queueCount != computeQueueCount)) {
            printf("%s: can't find requested queue type %X\n", __func__, requestedQueue->queueType);
            res = GR_ERROR_INVALID_VALUE;
            // Bail after the loop to properly release memory
        }

        queueCreateInfos[i] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = requestedQueue->queueType == GR_QUEUE_UNIVERSAL ?
                                universalQueueIndex : computeQueueIndex,
            .queueCount = requestedQueue->queueCount,
            .pQueuePriorities = queuePriorities,
        };
    }

    if (res != GR_SUCCESS) {
        goto bail;
    }

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = pCreateInfo->queueRecordCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .pEnabledFeatures = NULL,
    };

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice((VkPhysicalDevice)gpu, &createInfo, NULL, &device) != VK_SUCCESS) {
        printf("%s: vkCreateDevice failed\n", __func__);
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    *pDevice = (GR_DEVICE)device;

bail:
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        free((void*)queueCreateInfos[i].pQueuePriorities);
    }
    free(queueCreateInfos);

    return res;
}

// Queue Functions

GR_RESULT grGetDeviceQueue(
    GR_DEVICE device,
    GR_ENUM queueType,
    GR_UINT queueId,
    GR_QUEUE* pQueue)
{
    VkDevice vkDevice = (VkDevice)device;
    VkQueue vkQueue = VK_NULL_HANDLE;

    vkGetDeviceQueue(vkDevice, getVkQueueFamilyIndex(queueType), queueId, &vkQueue);

    *pQueue = (GR_QUEUE)vkQueue;
    return GR_SUCCESS;
}

// Command Buffer Management Functions

GR_RESULT grCreateCommandBuffer(
    GR_DEVICE device,
    const GR_CMD_BUFFER_CREATE_INFO* pCreateInfo,
    GR_CMD_BUFFER* pCmdBuffer)
{
    VkDevice vkDevice = (VkDevice)device;
    VkCommandPool vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer vkCommandBuffer = VK_NULL_HANDLE;

    // FIXME we shouldn't create one command pool per command buffer :)
    VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = getVkQueueFamilyIndex(pCreateInfo->queueType),
    };

    if (vkCreateCommandPool(vkDevice, &poolCreateInfo, NULL, &vkCommandPool) != VK_SUCCESS) {
        printf("%s: vkCreateCommandPool failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(vkDevice, &allocateInfo, &vkCommandBuffer) != VK_SUCCESS) {
        printf("%s: vkAllocateCommandBuffers failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    *pCmdBuffer = (GR_QUEUE)vkCommandBuffer;

    return GR_SUCCESS;
}

GR_RESULT grBeginCommandBuffer(
    GR_CMD_BUFFER cmdBuffer,
    GR_FLAGS flags)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;
    VkCommandBufferUsageFlags vkUsageFlags = 0;

    if ((flags & GR_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT) != 0) {
        vkUsageFlags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = vkUsageFlags,
        .pInheritanceInfo = NULL,
    };

    if (vkBeginCommandBuffer(vkCommandBuffer, &beginInfo) != VK_SUCCESS) {
        printf("%s: vkBeginCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

GR_RESULT grEndCommandBuffer(
    GR_CMD_BUFFER cmdBuffer)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;

    if (vkEndCommandBuffer(vkCommandBuffer) != VK_SUCCESS) {
        printf("%s: vkEndCommandBuffer failed\n", __func__);
        return GR_ERROR_OUT_OF_MEMORY;
    }

    return GR_SUCCESS;
}

// Command Buffer Building Functions

GR_VOID grCmdPrepareImages(
    GR_CMD_BUFFER cmdBuffer,
    GR_UINT transitionCount,
    const GR_IMAGE_STATE_TRANSITION* pStateTransitions)
{
    VkCommandBuffer vkCommandBuffer = (VkCommandBuffer)cmdBuffer;

    for (int i = 0; i < transitionCount; i++) {
        const GR_IMAGE_STATE_TRANSITION* stateTransition = &pStateTransitions[i];
        const GR_IMAGE_SUBRESOURCE_RANGE* range = &stateTransition->subresourceRange;

        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = getVkAccessFlags(stateTransition->oldState),
            .dstAccessMask = getVkAccessFlags(stateTransition->newState),
            .oldLayout = getVkImageLayout(stateTransition->oldState),
            .newLayout = getVkImageLayout(stateTransition->newState),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = (VkImage)stateTransition->image,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask = getVkImageAspectFlags(range->aspect),
                .baseMipLevel = range->baseMipLevel,
                .levelCount = range->mipLevels == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_MIP_LEVELS : range->mipLevels,
                .baseArrayLayer = range->baseArraySlice,
                .layerCount = range->arraySize == GR_LAST_MIP_OR_SLICE ?
                              VK_REMAINING_ARRAY_LAYERS : range->arraySize,
            }
        };

        vkCmdPipelineBarrier(vkCommandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }
}
