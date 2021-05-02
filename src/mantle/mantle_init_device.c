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
    LOGT("%p %p %p\n", pAppInfo, pAllocCb, pGpuCount);
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkRes;

    vulkanLoaderLibraryInit(&vkl);

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
    } else if (GET_OBJ_TYPE(grPhysicalGpu) != GR_OBJ_TYPE_PHYSICAL_GPU) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    const VkPhysicalDeviceProperties* props = &grPhysicalGpu->physicalDeviceProps;

    if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_PROPERTIES);
            return GR_SUCCESS;
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
    } else if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_PERFORMANCE);
            return GR_SUCCESS;
        }

        *(GR_PHYSICAL_GPU_PERFORMANCE*)pData = (GR_PHYSICAL_GPU_PERFORMANCE) {
            .maxGpuClock = 1000.f,
            .aluPerClock = 16.f, // 19.4.3
            .texPerClock = 16.f, // 19.4.3
            .primsPerClock = 2.f, // 19.4.3
            .pixelsPerClock = 16.f, // 19.4.3
        };
    } else if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_MEMORY_PROPERTIES) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_MEMORY_PROPERTIES);
            return GR_SUCCESS;
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
    } else if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_IMAGE_PROPERTIES) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_IMAGE_PROPERTIES);
            return GR_SUCCESS;
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
    } else {
        LOGE("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    return GR_SUCCESS;
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
    VULKAN_DEVICE vkd;
    VkDevice vkDevice = VK_NULL_HANDLE;
    unsigned universalQueueIndex = INVALID_QUEUE_INDEX;
    unsigned universalQueueCount = 0;
    unsigned computeQueueIndex = INVALID_QUEUE_INDEX;
    unsigned computeQueueCount = 0;
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
    }

    if (res != GR_SUCCESS) {
        goto bail;
    }

    const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDsLayouts = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES,
        .pNext = NULL,
        .separateDepthStencilLayouts = VK_TRUE,
    };
    const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = (void*)&separateDsLayouts,
        .extendedDynamicState = VK_TRUE,
    };
    const VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demoteToHelperInvocation = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT,
        .pNext = (void*)&extendedDynamicState,
        .shaderDemoteToHelperInvocation = VK_TRUE,
    };

    const VkPhysicalDeviceFeatures deviceFeatures = {
        .imageCubeArray = VK_TRUE,
        .geometryShader = VK_TRUE,
        .tessellationShader = VK_TRUE,
        .dualSrcBlend = VK_TRUE,
        .logicOp = VK_TRUE,
        .depthClamp = VK_TRUE,
        .multiViewport = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE,
    };

    const char *deviceExtensions[] = {
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    const VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &demoteToHelperInvocation,
        .flags = 0,
        .queueCreateInfoCount = pCreateInfo->queueRecordCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = COUNT_OF(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = &deviceFeatures,
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
        .universalQueueIndex = universalQueueIndex,
        .computeQueueIndex = computeQueueIndex,
    };

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

GR_RESULT grDestroyDevice(
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
