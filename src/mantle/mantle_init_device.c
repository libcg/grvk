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

// Exported functions

unsigned grDeviceGetQueueFamilyIndex(
    const GrDevice* grDevice,
    GR_QUEUE_TYPE queueType)
{
    switch (queueType) {
    case GR_QUEUE_UNIVERSAL:
        return grDevice->universalQueueFamilyIndex;
    case GR_QUEUE_COMPUTE:
        // Fall back to universal queue if not available
        if (grDevice->computeQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return grDevice->computeQueueFamilyIndex;
        } else {
            return grDevice->universalQueueFamilyIndex;
        }
    }

    switch ((GR_EXT_QUEUE_TYPE)queueType) {
    case GR_EXT_QUEUE_DMA:
        // Fall back to compute or universal queue if not available
        if (grDevice->dmaQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return grDevice->dmaQueueFamilyIndex;
        } else if (grDevice->computeQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return grDevice->computeQueueFamilyIndex;
        } else {
            return grDevice->universalQueueFamilyIndex;
        }
    case GR_EXT_QUEUE_TIMER:
        break; // TODO implement
    }

    LOGE("invalid queue type %d\n", queueType);
    return INVALID_QUEUE_INDEX;
}

CRITICAL_SECTION* grDeviceGetQueueMutex(
    GrDevice* grDevice,
    GR_QUEUE_TYPE queueType)
{
    switch (queueType) {
    case GR_QUEUE_UNIVERSAL:
        return &grDevice->universalQueueMutex;
    case GR_QUEUE_COMPUTE:
        // Fall back to universal queue if not available
        if (grDevice->computeQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return &grDevice->computeQueueMutex;
        } else {
            return &grDevice->universalQueueMutex;
        }
    }

    switch ((GR_EXT_QUEUE_TYPE)queueType) {
    case GR_EXT_QUEUE_DMA:
        // Fall back to universal or compute queue if not available
        if (grDevice->dmaQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return &grDevice->dmaQueueMutex;
        } else if (grDevice->computeQueueFamilyIndex != INVALID_QUEUE_INDEX) {
            return &grDevice->computeQueueMutex;
        } else {
            return &grDevice->universalQueueMutex;
        }
    case GR_EXT_QUEUE_TIMER:
        break; // TODO implement
    }

    LOGE("invalid queue type %d\n", queueType);
    return NULL;
}

// Initialization and Device Functions

GR_RESULT GR_STDCALL grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    LOGT("%p %p %p\n", pAppInfo, pAllocCb, pGpuCount);
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkRes;

    vulkanLoaderLibraryInit(&vkl);

    LOGI("app \"%s\" (%08X), engine \"%s\" (%08X), api %08X\n",
         pAppInfo->pAppName, pAppInfo->appVersion,
         pAppInfo->pEngineName, pAppInfo->engineVersion,
         pAppInfo->apiVersion);

    quirkInit(pAppInfo);

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
        .enabledExtensionCount = COUNT_OF(instanceExtensions),
        .ppEnabledExtensionNames = instanceExtensions,
    };

    vkRes = vkl.vkCreateInstance(&createInfo, NULL, &vkInstance);
    free(grvkEngineName);

    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateInstance failed (%d)\n", vkRes);

        if (vkRes == VK_ERROR_INCOMPATIBLE_DRIVER) {
            LOGE("incompatible driver detected. Vulkan 1.2 support is required\n");
        }

        return GR_ERROR_INITIALIZATION_FAILED;
    }

    vulkanLoaderInstanceInit(&vki, vkInstance);

    uint32_t vkPhysicalDeviceCount = 0;
    vki.vkEnumeratePhysicalDevices(vkInstance, &vkPhysicalDeviceCount, NULL);
    if (vkPhysicalDeviceCount > GR_MAX_PHYSICAL_GPUS) {
        vkPhysicalDeviceCount = GR_MAX_PHYSICAL_GPUS;
    }

    VkPhysicalDevice physicalDevices[GR_MAX_PHYSICAL_GPUS];
    vki.vkEnumeratePhysicalDevices(vkInstance, &vkPhysicalDeviceCount, physicalDevices);

    *pGpuCount = vkPhysicalDeviceCount;
    for (int i = 0; i < *pGpuCount; i++) {
        GrPhysicalGpu* grPhysicalGpu = malloc(sizeof(GrPhysicalGpu));
        *grPhysicalGpu = (GrPhysicalGpu) {
            .grBaseObj = { GR_OBJ_TYPE_PHYSICAL_GPU },
            .physicalDevice = physicalDevices[i],
            .physicalDeviceProps = { 0 }, // Initialized below
        };

        vki.vkGetPhysicalDeviceProperties(physicalDevices[i], &grPhysicalGpu->physicalDeviceProps);

        gpus[i] = (GR_PHYSICAL_GPU)grPhysicalGpu;
    }

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grGetGpuInfo(
    GR_PHYSICAL_GPU gpu,
    GR_ENUM infoType,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p 0x%X %p %p\n", gpu, infoType, pDataSize, pData);
    GrPhysicalGpu* grPhysicalGpu = (GrPhysicalGpu*)gpu;

    if (grPhysicalGpu == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grPhysicalGpu) != GR_OBJ_TYPE_PHYSICAL_GPU) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const VkPhysicalDeviceProperties* props = &grPhysicalGpu->physicalDeviceProps;
    unsigned expectedSize = 0;

    switch (infoType) {
    case GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES:
        expectedSize = sizeof(GR_PHYSICAL_GPU_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        GR_PHYSICAL_GPU_PROPERTIES* gpuProps = (GR_PHYSICAL_GPU_PROPERTIES*)pData;
        *gpuProps = (GR_PHYSICAL_GPU_PROPERTIES) {
            .apiVersion = 0x19000, // 19.4.3
            .driverVersion = 0x49C00000, // 19.4.3
            .vendorId = props->vendorID,
            .deviceId = props->deviceID,
            .gpuType = getGrPhysicalGpuType(props->deviceType),
            .gpuName = "", // Initialized below
            .maxMemRefsPerSubmission = 16384, // 19.4.3
            .reserved = 4200043, // 19.4.3
            .maxInlineMemoryUpdateSize = 32768, // 19.4.3
            .maxBoundDescriptorSets = 2, // 19.4.3
            .maxThreadGroupSize = props->limits.maxComputeWorkGroupSize[0],
            .timestampFrequency = 1000000000.f / props->limits.timestampPeriod,
            .multiColorTargetClears = true, // 19.4.3
        };
        strncpy(gpuProps->gpuName, props->deviceName, GR_MAX_PHYSICAL_GPU_NAME);
        break;
    case GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE:
        expectedSize = sizeof(GR_PHYSICAL_GPU_PERFORMANCE);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_PERFORMANCE, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        *(GR_PHYSICAL_GPU_PERFORMANCE*)pData = (GR_PHYSICAL_GPU_PERFORMANCE) {
            .maxGpuClock = 1000.f,
            .aluPerClock = 16.f, // 19.4.3
            .texPerClock = 16.f, // 19.4.3
            .primsPerClock = 2.f, // 19.4.3
            .pixelsPerClock = 16.f, // 19.4.3
        };
        break;
    case GR_INFO_TYPE_PHYSICAL_GPU_QUEUE_PROPERTIES:
        expectedSize = 4 * sizeof(GR_PHYSICAL_GPU_QUEUE_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_QUEUE_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        // Values from 19.4.3 driver
        ((GR_PHYSICAL_GPU_QUEUE_PROPERTIES*)pData)[0] = (GR_PHYSICAL_GPU_QUEUE_PROPERTIES) {
            .queueType = GR_QUEUE_UNIVERSAL,
            .queueCount = 1,
            .maxAtomicCounters = 512,
            .supportsTimestamps = false, // TODO implement timestamps
        };
        ((GR_PHYSICAL_GPU_QUEUE_PROPERTIES*)pData)[1] = (GR_PHYSICAL_GPU_QUEUE_PROPERTIES) {
            .queueType = GR_QUEUE_COMPUTE,
            .queueCount = 1,
            .maxAtomicCounters = 1024,
            .supportsTimestamps = false, // TODO implement timestamps
        };
        ((GR_PHYSICAL_GPU_QUEUE_PROPERTIES*)pData)[2] = (GR_PHYSICAL_GPU_QUEUE_PROPERTIES) {
            .queueType = GR_EXT_QUEUE_DMA,
            .queueCount = 1,
            .maxAtomicCounters = 0,
            .supportsTimestamps = false, // TODO implement timestamps
        };
        ((GR_PHYSICAL_GPU_QUEUE_PROPERTIES*)pData)[3] = (GR_PHYSICAL_GPU_QUEUE_PROPERTIES) {
            .queueType = GR_EXT_QUEUE_TIMER,
            .queueCount = 0, // TODO implement
            .maxAtomicCounters = 0,
            .supportsTimestamps = false,
        };
        break;
    case GR_INFO_TYPE_PHYSICAL_GPU_MEMORY_PROPERTIES:
        expectedSize = sizeof(GR_PHYSICAL_GPU_MEMORY_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_MEMORY_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        // TODO don't expose unsupported features?
        *(GR_PHYSICAL_GPU_MEMORY_PROPERTIES*)pData = (GR_PHYSICAL_GPU_MEMORY_PROPERTIES) {
            .flags = GR_MEMORY_VIRTUAL_REMAPPING_SUPPORT | // 19.4.3
                     GR_MEMORY_PINNING_SUPPORT | // 19.4.3
                     GR_MEMORY_PREFER_GLOBAL_REFS, // 19.4.3
            .virtualMemPageSize = 4096, // 19.4.3
            .maxVirtualMemSize = 1086626725888ull, // 19.4.3
            .maxPhysicalMemSize = 10354294784ull, // 19.4.3
        };
        break;
    case GR_INFO_TYPE_PHYSICAL_GPU_IMAGE_PROPERTIES:
        expectedSize = sizeof(GR_PHYSICAL_GPU_IMAGE_PROPERTIES);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_IMAGE_PROPERTIES, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        *(GR_PHYSICAL_GPU_IMAGE_PROPERTIES*)pData = (GR_PHYSICAL_GPU_IMAGE_PROPERTIES) {
            .maxSliceWidth = props->limits.maxImageDimension1D,
            .maxSliceHeight = props->limits.maxImageDimension2D,
            .maxDepth = props->limits.maxImageDimension3D,
            .maxArraySlices = props->limits.maxImageArrayLayers,
            .reserved1 = 0, // 19.4.3
            .reserved2 = 0, // 19.4.3
            .maxMemoryAlignment = 262144, // 19.4.3
            .sparseImageSupportLevel = 0, // 19.4.3
            .flags = 0x0, // 19.4.3
        };
        break;
    case GR_EXT_INFO_TYPE_PHYSICAL_GPU_SUPPORTED_AXL_VERSION:
        expectedSize = sizeof(GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION);

        if (pData == NULL) {
            break;
        } else if (*pDataSize < expectedSize) {
            LOGW("can't write GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION, got size %d, expected %d\n",
                 *pDataSize, expectedSize);
            return GR_ERROR_INVALID_MEMORY_SIZE;
        }

        *(GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION*)pData = (GR_PHYSICAL_GPU_SUPPORTED_AXL_VERSION) {
            .minVersion = 0,
            .maxVersion = UINT32_MAX,
        };
        break;
    default:
        LOGE("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    assert(expectedSize != 0);
    *pDataSize = expectedSize;

    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grCreateDevice(
    GR_PHYSICAL_GPU gpu,
    const GR_DEVICE_CREATE_INFO* pCreateInfo,
    GR_DEVICE* pDevice)
{
    LOGT("%p %p %p\n", gpu, pCreateInfo, pDevice);
    GR_RESULT res = GR_SUCCESS;
    VkResult vkRes;
    GrPhysicalGpu* grPhysicalGpu = (GrPhysicalGpu*)gpu;
    VULKAN_DEVICE vkd;
    VkDevice vkDevice = VK_NULL_HANDLE;
    unsigned universalQueueFamilyIndex = INVALID_QUEUE_INDEX;
    unsigned universalQueueCount = 0;
    unsigned computeQueueFamilyIndex = INVALID_QUEUE_INDEX;
    unsigned computeQueueCount = 0;
    unsigned dmaQueueFamilyIndex = INVALID_QUEUE_INDEX;
    unsigned dmaQueueCount = 0;
    uint32_t driverVersion;

    const VkPhysicalDeviceProperties* props = &grPhysicalGpu->physicalDeviceProps;

    if (props->vendorID == NVIDIA_VENDOR_ID) {
        // Fix up driver version
        driverVersion = VK_MAKE_VERSION(
            VK_VERSION_MAJOR(props->driverVersion),
            VK_VERSION_MINOR(props->driverVersion >> 0) >> 2,
            VK_VERSION_PATCH(props->driverVersion >> 2) >> 4);
    } else {
        driverVersion = props->driverVersion;
    }

    LOGI("%04X:%04X \"%s\" (Vulkan %d.%d.%d, driver %d.%d.%d)\n",
         props->vendorID, props->deviceID, props->deviceName,
         VK_VERSION_MAJOR(props->apiVersion),
         VK_VERSION_MINOR(props->apiVersion),
         VK_VERSION_PATCH(props->apiVersion),
         VK_VERSION_MAJOR(driverVersion),
         VK_VERSION_MINOR(driverVersion),
         VK_VERSION_PATCH(driverVersion));

    uint32_t vkQueueFamilyPropertyCount = 0;
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grPhysicalGpu->physicalDevice,
                                                 &vkQueueFamilyPropertyCount, NULL);

    VkQueueFamilyProperties* queueFamilyProperties =
        malloc(sizeof(VkQueueFamilyProperties) * vkQueueFamilyPropertyCount);
    vki.vkGetPhysicalDeviceQueueFamilyProperties(grPhysicalGpu->physicalDevice,
                                                 &vkQueueFamilyPropertyCount,
                                                 queueFamilyProperties);

    for (int i = 0; i < vkQueueFamilyPropertyCount; i++) {
        const VkQueueFamilyProperties* queueFamilyProperty = &queueFamilyProperties[i];

        if ((queueFamilyProperty->queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            universalQueueFamilyIndex = i;
            universalQueueCount = queueFamilyProperty->queueCount;
        } else if (queueFamilyProperty->queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeQueueFamilyIndex = i;
            computeQueueCount = queueFamilyProperty->queueCount;
        } else if (queueFamilyProperty->queueFlags & VK_QUEUE_TRANSFER_BIT) {
            dmaQueueFamilyIndex = i;
            dmaQueueCount = queueFamilyProperty->queueCount;
        }
    }

    unsigned queueCreateInfoCount = 0;
    VkDeviceQueueCreateInfo* queueCreateInfos =
        malloc(sizeof(VkDeviceQueueCreateInfo) * pCreateInfo->queueRecordCount);
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        const GR_DEVICE_QUEUE_CREATE_INFO* requestedQueue = &pCreateInfo->pRequestedQueues[i];

        float* queuePriorities = malloc(sizeof(float) * requestedQueue->queueCount);

        for (int j = 0; j < requestedQueue->queueCount; j++) {
            queuePriorities[j] = 1.0f; // Max priority
        }

        unsigned requestedQueueFamilyIndex = INVALID_QUEUE_INDEX;
        switch (requestedQueue->queueType) {
        case GR_QUEUE_UNIVERSAL:
            if (requestedQueue->queueCount <= universalQueueCount) {
                requestedQueueFamilyIndex = universalQueueFamilyIndex;
            }
            break;
        case GR_QUEUE_COMPUTE:
            if (requestedQueue->queueCount <= computeQueueCount) {
                requestedQueueFamilyIndex = computeQueueFamilyIndex;
            }
            break;
        case GR_EXT_QUEUE_DMA:
            if (requestedQueue->queueCount <= dmaQueueCount) {
                requestedQueueFamilyIndex = dmaQueueFamilyIndex;
            }
            break;
        }

        if (requestedQueueFamilyIndex == INVALID_QUEUE_INDEX) {
            if (requestedQueue->queueType == GR_QUEUE_UNIVERSAL) {
                LOGE("can't find requested queue type 0x%X with count %d\n",
                     requestedQueue->queueType, requestedQueue->queueCount);
                res = GR_ERROR_INVALID_VALUE;
                // Bail after the loop to properly release memory
            } else {
                LOGW("can't find requested queue type 0x%X with count %d, "
                     "falling back to universal or compute queue...\n",
                     requestedQueue->queueType, requestedQueue->queueCount);
                continue;
            }
        }

        queueCreateInfos[queueCreateInfoCount] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = requestedQueueFamilyIndex,
            .queueCount = requestedQueue->queueCount,
            .pQueuePriorities = queuePriorities,
        };
        queueCreateInfoCount++;
    }

    if (res != GR_SUCCESS) {
        goto bail;
    }

    VkPhysicalDeviceCustomBorderColorFeaturesEXT customBorderColor = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
        .pNext = NULL,
        .customBorderColors = VK_TRUE,
        .customBorderColorWithoutFormat = VK_TRUE,
    };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &customBorderColor,
        .extendedDynamicState = VK_TRUE,
    };
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demoteToHelperInvocation = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT,
        .pNext = &extendedDynamicState,
        .shaderDemoteToHelperInvocation = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features vulkan12DeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &demoteToHelperInvocation,
        .samplerMirrorClampToEdge = VK_TRUE,
        .separateDepthStencilLayouts = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan12DeviceFeatures,
        .features = {
            .imageCubeArray = VK_TRUE,
            .independentBlend = VK_TRUE,
            .geometryShader = VK_TRUE,
            .tessellationShader = VK_TRUE,
            .sampleRateShading = VK_TRUE,
            .dualSrcBlend = VK_TRUE,
            .logicOp = VK_TRUE,
            .depthClamp = VK_TRUE,
            .fillModeNonSolid = VK_TRUE,
            .multiViewport = VK_TRUE,
            .samplerAnisotropy = VK_TRUE,
            .fragmentStoresAndAtomics = VK_TRUE,
            .occlusionQueryPrecise = VK_TRUE,
        },
    };

    const char *deviceExtensions[] = {
        VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME,
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .flags = 0,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = COUNT_OF(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = NULL,
    };

    vkRes = vki.vkCreateDevice(grPhysicalGpu->physicalDevice, &createInfo, NULL, &vkDevice);
    if (vkRes != VK_SUCCESS) {
        LOGE("vkCreateDevice failed (%d)\n", vkRes);

        if (vkRes == VK_ERROR_EXTENSION_NOT_PRESENT) {
            LOGE("missing extension. make sure your Vulkan driver supports:\n");
            for (unsigned i = 0; i < COUNT_OF(deviceExtensions); i++) {
                LOGE("- %s\n", deviceExtensions[i]);
            }
        }

        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    vulkanLoaderDeviceInit(&vkd, vkDevice);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vki.vkGetPhysicalDeviceMemoryProperties(grPhysicalGpu->physicalDevice, &memoryProperties);

    GrDevice* grDevice = malloc(sizeof(GrDevice));
    *grDevice = (GrDevice) {
        .grBaseObj = { GR_OBJ_TYPE_DEVICE },
        .vkd = vkd,
        .device = vkDevice,
        .physicalDevice = grPhysicalGpu->physicalDevice,
        .memoryProperties = memoryProperties,
        .universalQueueFamilyIndex = universalQueueFamilyIndex,
        .computeQueueFamilyIndex = computeQueueFamilyIndex,
        .dmaQueueFamilyIndex = dmaQueueFamilyIndex,
        .universalQueueMutex = { 0 }, // Initialized below
        .computeQueueMutex = { 0 }, // Initialized below
        .dmaQueueMutex = { 0 }, // Initialized below
        .grBorderColorPalette = NULL,
    };

    // Allow queue muxing when the driver doesn't expose certain queue types
    InitializeCriticalSectionAndSpinCount(&grDevice->universalQueueMutex, 0);
    InitializeCriticalSectionAndSpinCount(&grDevice->computeQueueMutex, 0);
    InitializeCriticalSectionAndSpinCount(&grDevice->dmaQueueMutex, 0);

    *pDevice = (GR_DEVICE)grDevice;

bail:
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        free((void*)queueCreateInfos[i].pQueuePriorities);
    }
    free(queueCreateInfos);

    if (res != GR_SUCCESS) {
        vkd.vkDestroyDevice(vkDevice, NULL);
    }

    return res;
}

GR_RESULT GR_STDCALL grDestroyDevice(
    GR_DEVICE device)
{
    LOGT("%p\n", device);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }

    VKD.vkDestroyDevice(grDevice->device, NULL);
    free(grDevice);

    return GR_SUCCESS;
}
