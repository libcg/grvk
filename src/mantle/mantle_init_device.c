#include <stdio.h>
#include "mantle_internal.h"

#define NVIDIA_VENDOR_ID 0x10de

static char* getGrvkEngineName(
    const GR_CHAR* engineName)
{
    size_t engineNameLen = (engineName != NULL ? strlen(engineName) : 0);
    size_t grvkEngineNameLen = engineNameLen + 64;
    char* grvkEngineName = malloc(sizeof(char) * grvkEngineNameLen);

    if (engineName == NULL) {
        snprintf(grvkEngineName, grvkEngineNameLen, "[GRVK %s]", GRVK_VERSION);
    } else {
        snprintf(grvkEngineName, grvkEngineNameLen, "[GRVK %s] %s", GRVK_VERSION, engineName);
    }

    return grvkEngineName;
}

// Initialization and Device Functions

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    logInit();
    logPrintRaw("=== GRVK %s ===\n", GRVK_VERSION);

    LOGT("%p %p %p\n", pAppInfo, pAllocCb, pGpuCount);
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkRes;

    vulkanLoaderLibraryInit();

    LOGI("app \"%s\" (%08X), engine \"%s\" (%08X), api %08X\n",
         pAppInfo->pAppName, pAppInfo->appVersion,
         pAppInfo->pEngineName, pAppInfo->engineVersion,
         pAppInfo->apiVersion);

    if (pAllocCb != NULL) {
        LOGW("unhandled alloc callbacks\n");
    }

    char* grvkEngineName = getGrvkEngineName(pAppInfo->pEngineName);

    const VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = pAppInfo->pAppName,
        .applicationVersion = pAppInfo->appVersion,
        .pEngineName = grvkEngineName,
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

    vkRes = vkl.vkCreateInstance(&createInfo, NULL, &vkInstance);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateInstance failed (%d)\n", vkRes);

        if (vkRes == VK_ERROR_INCOMPATIBLE_DRIVER) {
            LOGE("incompatible driver detected. Vulkan 1.2 support is required\n");
        }

        return GR_ERROR_INITIALIZATION_FAILED;
    }

    free(grvkEngineName);
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
        GrPhysicalGpu* grPhysicalGpu = malloc(sizeof(GrPhysicalGpu));
        *grPhysicalGpu = (GrPhysicalGpu) {
            .sType = GR_STRUCT_TYPE_PHYSICAL_GPU,
            .physicalDevice = physicalDevices[i],
        };

        gpus[i] = (GR_PHYSICAL_GPU)grPhysicalGpu;
    }

    return GR_SUCCESS;
}

GR_RESULT grGetGpuInfo(
    GR_PHYSICAL_GPU gpu,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X %p %p\n", gpu, infoType, pDataSize, pData);
    GrPhysicalGpu* grPhysicalGpu = (GrPhysicalGpu*)gpu;

    if (grPhysicalGpu == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (grPhysicalGpu->sType != GR_STRUCT_TYPE_PHYSICAL_GPU) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES) {
        if (pData == NULL) {
            return sizeof(GR_PHYSICAL_GPU_PROPERTIES);
        }

        VkPhysicalDeviceProperties physicalDeviceProps;
        vki.vkGetPhysicalDeviceProperties(grPhysicalGpu->physicalDevice, &physicalDeviceProps);

        GR_PHYSICAL_GPU_PROPERTIES* gpuProps = (GR_PHYSICAL_GPU_PROPERTIES*)pData;
        *gpuProps = (GR_PHYSICAL_GPU_PROPERTIES) {
            .apiVersion = 0,
            .driverVersion = physicalDeviceProps.driverVersion,
            .vendorId = physicalDeviceProps.vendorID,
            .deviceId = physicalDeviceProps.deviceID,
            .gpuType = getGrPhysicalGpuType(physicalDeviceProps.deviceType),
            .gpuName = "", // Filled out below
            .maxMemRefsPerSubmission = 1024, // FIXME guess
            .reserved = 0,
            .maxInlineMemoryUpdateSize = 1024, // FIXME guess
            .maxBoundDescriptorSets = 32, // FIXME guess
            .maxThreadGroupSize = physicalDeviceProps.limits.maxComputeWorkGroupSize[0],
            .timestampFrequency = 1000000000.f / physicalDeviceProps.limits.timestampPeriod,
            .multiColorTargetClears = false,
        };
        strncpy(gpuProps->gpuName, physicalDeviceProps.deviceName, GR_MAX_PHYSICAL_GPU_NAME);
        return GR_SUCCESS;
    } else if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE) {
        if (pData == NULL) {
            return sizeof(GR_PHYSICAL_GPU_PERFORMANCE);
        }

        *(GR_PHYSICAL_GPU_PERFORMANCE*)pData = (GR_PHYSICAL_GPU_PERFORMANCE) {
            .maxGpuClock = 1000.f,
            .aluPerClock = 1.f,
            .texPerClock = 1.f,
            .primsPerClock = 1.f,
            .pixelsPerClock = 1.f,
        };
        return GR_SUCCESS;
    }

    LOGE("unsupported info type 0x%X\n", infoType);
    return GR_ERROR_INVALID_VALUE;
}

GR_RESULT grCreateDevice(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice)
{
    LOGT("%p %p %p\n", gpu, pCreateInfo, pDevice);
    GR_RESULT res = GR_SUCCESS;
    VkResult vkRes;
    GrPhysicalGpu* grPhysicalGpu = (GrPhysicalGpu*)gpu;
    VkDevice vkDevice = VK_NULL_HANDLE;
    uint32_t universalQueueIndex = INVALID_QUEUE_INDEX;
    uint32_t universalQueueCount = 0;
    bool universalQueueRequested = false;
    VkCommandPool universalCommandPool = VK_NULL_HANDLE;
    uint32_t computeQueueIndex = INVALID_QUEUE_INDEX;
    uint32_t computeQueueCount = 0;
    bool computeQueueRequested = false;
    VkCommandPool computeCommandPool = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties physicalDeviceProps;
    vki.vkGetPhysicalDeviceProperties(grPhysicalGpu->physicalDevice, &physicalDeviceProps);

    if (physicalDeviceProps.vendorID == NVIDIA_VENDOR_ID) {
        // Fixup driver version
        physicalDeviceProps.driverVersion =
            VK_MAKE_VERSION(VK_VERSION_MAJOR(physicalDeviceProps.driverVersion),
                            VK_VERSION_MINOR(physicalDeviceProps.driverVersion >> 0) >> 2,
                            VK_VERSION_PATCH(physicalDeviceProps.driverVersion >> 2) >> 4);
    }

    LOGI("%04X:%04X \"%s\" (Vulkan %d.%d.%d, driver %d.%d.%d)\n",
         physicalDeviceProps.vendorID,
         physicalDeviceProps.deviceID,
         physicalDeviceProps.deviceName,
         VK_VERSION_MAJOR(physicalDeviceProps.apiVersion),
         VK_VERSION_MINOR(physicalDeviceProps.apiVersion),
         VK_VERSION_PATCH(physicalDeviceProps.apiVersion),
         VK_VERSION_MAJOR(physicalDeviceProps.driverVersion),
         VK_VERSION_MINOR(physicalDeviceProps.driverVersion),
         VK_VERSION_PATCH(physicalDeviceProps.driverVersion));

    uint32_t queueFamilyPropertyCount = 0;
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grPhysicalGpu->physicalDevice,
                                                 &queueFamilyPropertyCount, NULL);

    VkQueueFamilyProperties* queueFamilyProperties =
        malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grPhysicalGpu->physicalDevice,
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
            LOGE("can't find requested queue type %X with count %d\n",
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
        .samplerAnisotropy = VK_TRUE,
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

    vkRes = vki.vkCreateDevice(grPhysicalGpu->physicalDevice, &createInfo, NULL, &vkDevice);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateDevice failed\n");

        if (vkRes == VK_ERROR_EXTENSION_NOT_PRESENT) {
            LOGE("missing extension. make sure your Vulkan driver supports "
                 "VK_EXT_extended_dynamic_state\n");
        }

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
            LOGE("vkCreateCommandPool failed\n");
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
            LOGE("vkCreateCommandPool failed\n");
            res = GR_ERROR_INITIALIZATION_FAILED;
            goto bail;
        }
    }

    GrDevice* grDevice = malloc(sizeof(GrDevice));
    *grDevice = (GrDevice) {
        .sType = GR_STRUCT_TYPE_DEVICE,
        .device = vkDevice,
        .physicalDevice = grPhysicalGpu->physicalDevice,
        .universalQueueIndex = universalQueueIndex,
        .universalCommandPool = universalCommandPool,
        .computeQueueIndex = computeQueueIndex,
        .computeCommandPool = computeCommandPool,
    };
    vki.vkGetPhysicalDeviceMemoryProperties(
        grPhysicalGpu->physicalDevice,
        &grDevice->memoryProperties);
    *pDevice = (GR_DEVICE)grDevice;

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
