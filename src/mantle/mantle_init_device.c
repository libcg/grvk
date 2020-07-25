#include "mantle_internal.h"

static GR_ALLOC_FUNCTION mAllocFun = NULL;
static GR_FREE_FUNCTION mFreeFun = NULL;

// Initialization and Device Functions

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    VkInstance vkInstance = VK_NULL_HANDLE;

    vulkanLoaderLibraryInit();

    printf("%s: app \"%s\" (%08X), engine \"%s\" (%08X), api %08X\n", __func__,
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

    const VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = pAppInfo->pAppName,
        .applicationVersion = pAppInfo->appVersion,
        .pEngineName = pAppInfo->pEngineName,
        .engineVersion = pAppInfo->engineVersion,
        .apiVersion = VK_API_VERSION_1_2,
    };

    const char *instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    const VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]),
        .ppEnabledExtensionNames = instanceExtensions,
    };

    if (vkl.vkCreateInstance(&createInfo, NULL, &vkInstance) != VK_SUCCESS) {
        printf("%s: vkCreateInstance failed\n", __func__);
        return GR_ERROR_INITIALIZATION_FAILED;
    }

    vulkanLoaderInstanceInit(vkInstance);

    uint32_t physicalDeviceCount = 0;
    vki.vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > GR_MAX_PHYSICAL_GPUS) {
        physicalDeviceCount = GR_MAX_PHYSICAL_GPUS;
    }

    VkPhysicalDevice physicalDevices[GR_MAX_PHYSICAL_GPUS];
    vki.vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, physicalDevices);

    *pGpuCount = physicalDeviceCount;
    for (int i = 0; i < *pGpuCount; i++) {
        GrvkPhysicalGpu* grvkPhysicalGpu = malloc(sizeof(GrvkPhysicalGpu));
        *grvkPhysicalGpu = (GrvkPhysicalGpu) {
            .sType = GRVK_STRUCT_TYPE_PHYSICAL_GPU,
            .physicalDevice = physicalDevices[i],
        };

        gpus[i] = (GR_PHYSICAL_GPU)grvkPhysicalGpu;
    }

    return GR_SUCCESS;
}

GR_RESULT grCreateDevice(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice)
{
    GR_RESULT res = GR_SUCCESS;
    GrvkPhysicalGpu* grvkPhysicalGpu = (GrvkPhysicalGpu*)gpu;
    VkDevice vkDevice = VK_NULL_HANDLE;
    uint32_t universalQueueIndex = INVALID_QUEUE_INDEX;
    uint32_t universalQueueCount = 0;
    bool universalQueueRequested = false;
    VkCommandPool universalCommandPool = VK_NULL_HANDLE;
    uint32_t computeQueueIndex = INVALID_QUEUE_INDEX;
    uint32_t computeQueueCount = 0;
    bool computeQueueRequested = false;
    VkCommandPool computeCommandPool = VK_NULL_HANDLE;

    uint32_t queueFamilyPropertyCount = 0;
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grvkPhysicalGpu->physicalDevice,
                                                 &queueFamilyPropertyCount, NULL);

    VkQueueFamilyProperties* queueFamilyProperties =
        malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grvkPhysicalGpu->physicalDevice,
                                                 &queueFamilyPropertyCount, queueFamilyProperties);

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
             requestedQueue->queueCount > universalQueueCount) ||
            (requestedQueue->queueType == GR_QUEUE_COMPUTE &&
             requestedQueue->queueCount > computeQueueCount)) {
            printf("%s: can't find requested queue type %X with count %d\n", __func__,
                   requestedQueue->queueType, requestedQueue->queueCount);
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

        if (requestedQueue->queueType == GR_QUEUE_UNIVERSAL) {
            universalQueueRequested = true;
        } else if (requestedQueue->queueType == GR_QUEUE_COMPUTE) {
            computeQueueRequested = true;
        }
    }

    if (res != GR_SUCCESS) {
        goto bail;
    }

    const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = NULL,
        .extendedDynamicState = VK_TRUE,
    };

    const VkPhysicalDeviceFeatures deviceFeatures = {
        .geometryShader = VK_TRUE,
        .tessellationShader = VK_TRUE,
        .dualSrcBlend = VK_TRUE,
        .logicOp = VK_TRUE,
        .depthClamp = VK_TRUE,
        .multiViewport = VK_TRUE,
    };

    const char *deviceExtensions[] = {
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &extendedDynamicState,
        .flags = 0,
        .queueCreateInfoCount = pCreateInfo->queueRecordCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]),
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures,
    };

    if (vki.vkCreateDevice(grvkPhysicalGpu->physicalDevice, &createInfo, NULL,
                           &vkDevice) != VK_SUCCESS) {
        printf("%s: vkCreateDevice failed\n", __func__);
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    if (universalQueueRequested && universalQueueIndex != INVALID_QUEUE_INDEX) {
        const VkCommandPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = universalQueueIndex,
        };

        if (vki.vkCreateCommandPool(vkDevice, &poolCreateInfo, NULL,
                                    &universalCommandPool) != VK_SUCCESS) {
            printf("%s: vkCreateCommandPool failed\n", __func__);
            res = GR_ERROR_INITIALIZATION_FAILED;
            goto bail;
        }
    }
    if (computeQueueRequested && computeQueueIndex != INVALID_QUEUE_INDEX) {
        const VkCommandPoolCreateInfo poolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = computeQueueIndex,
        };

        if (vki.vkCreateCommandPool(vkDevice, &poolCreateInfo, NULL,
                                    &computeCommandPool) != VK_SUCCESS) {
            printf("%s: vkCreateCommandPool failed\n", __func__);
            res = GR_ERROR_INITIALIZATION_FAILED;
            goto bail;
        }
    }

    GrvkDevice* grvkDevice = malloc(sizeof(GrvkDevice));
    *grvkDevice = (GrvkDevice) {
        .sType = GRVK_STRUCT_TYPE_DEVICE,
        .device = vkDevice,
        .physicalDevice = grvkPhysicalGpu->physicalDevice,
        .universalQueueIndex = universalQueueIndex,
        .universalCommandPool = universalCommandPool,
        .computeQueueIndex = computeQueueIndex,
        .computeCommandPool = computeCommandPool,
    };

    *pDevice = (GR_DEVICE)grvkDevice;

bail:
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        free((void*)queueCreateInfos[i].pQueuePriorities);
    }
    free(queueCreateInfos);

    if (res != GR_SUCCESS) {
        if (universalCommandPool != VK_NULL_HANDLE) {
            vki.vkDestroyCommandPool(vkDevice, universalCommandPool, NULL);
        }
        if (computeCommandPool != VK_NULL_HANDLE) {
            vki.vkDestroyCommandPool(vkDevice, computeCommandPool, NULL);
        }
        if (vkDevice != VK_NULL_HANDLE) {
            vki.vkDestroyDevice(vkDevice, NULL);
        }
    }

    return res;
}
