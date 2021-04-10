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
    } else if (GET_OBJ_TYPE(grPhysicalGpu) != GR_OBJ_TYPE_PHYSICAL_GPU) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_PROPERTIES);
            return GR_SUCCESS;
        }

        VkPhysicalDeviceProperties physicalDeviceProps;
        vki.vkGetPhysicalDeviceProperties(grPhysicalGpu->physicalDevice, &physicalDeviceProps);

        GR_PHYSICAL_GPU_PROPERTIES* gpuProps = (GR_PHYSICAL_GPU_PROPERTIES*)pData;
        *gpuProps = (GR_PHYSICAL_GPU_PROPERTIES) {
            .apiVersion = 0x18000,
            .driverVersion = UINT32_MAX,
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
    } else if (infoType == GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE) {
        if (pData == NULL) {
            *pDataSize = sizeof(GR_PHYSICAL_GPU_PERFORMANCE);
            return GR_SUCCESS;
        }

        *(GR_PHYSICAL_GPU_PERFORMANCE*)pData = (GR_PHYSICAL_GPU_PERFORMANCE) {
            .maxGpuClock = 1000.f,
            .aluPerClock = 1.f,
            .texPerClock = 1.f,
            .primsPerClock = 1.f,
            .pixelsPerClock = 1.f,
        };
    } else {
        LOGE("unsupported info type 0x%X\n", infoType);
        return GR_ERROR_INVALID_VALUE;
    }

    return GR_SUCCESS;
}

static unsigned getVirtualDescriptorSetBufferMemoryType(const VkPhysicalDeviceMemoryProperties *memoryProps) {
    unsigned suitableMemoryType = memoryProps->memoryTypeCount;
    unsigned hostMemoryType = memoryProps->memoryTypeCount;
    for (unsigned i = 0; i < memoryProps->memoryTypeCount; ++i) {
        if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) & memoryProps->memoryTypes[i].propertyFlags) {
            hostMemoryType = i;
            if (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT & memoryProps->memoryTypes[i].propertyFlags) {
                suitableMemoryType = i;
            }
        }
    }
    if (suitableMemoryType < memoryProps->memoryTypeCount) {
        LOGT("found suitable memory type for descriptor sets: %d\n", suitableMemoryType);
        return suitableMemoryType;
    }
    LOGT("fallback to host memory type for descriptor sets: %d\n", hostMemoryType);
    return hostMemoryType;
}

VkResult getVkPipelineLayout(VkDevice vkDevice,
                             const VULKAN_DEVICE* vkd,
                             VkDescriptorSetLayout globalDescriptorSetLayout,
                             VkDescriptorSetLayout graphicsDynamicMemoryLayout,
                             VkDescriptorSetLayout computeDynamicMemoryLayout,
                             GrGlobalPipelineLayouts* outLayouts
    )
{
    const VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0,
        .size = sizeof(uint64_t) * GR_MAX_DESCRIPTOR_SETS
    };

    const VkPushConstantRange computePushRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(uint64_t) * GR_MAX_DESCRIPTOR_SETS
    };

    const VkDescriptorSetLayout graphicsLayouts[2] = {globalDescriptorSetLayout, graphicsDynamicMemoryLayout};
    VkPipelineLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 2,
        .pSetLayouts = graphicsLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange,
    };
    VkResult res = vkd->vkCreatePipelineLayout(vkDevice, &createInfo, NULL, &outLayouts->graphicsPipelineLayout);
    if (res != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed for graphics pipeline layout\n");
        return res;
    }

    const VkDescriptorSetLayout computeLayouts[2] = {globalDescriptorSetLayout, computeDynamicMemoryLayout};
    // now create compute ones
    createInfo.pPushConstantRanges = &computePushRange;
    createInfo.pSetLayouts = computeLayouts;
    res = vkd->vkCreatePipelineLayout(vkDevice, &createInfo, NULL, &outLayouts->computePipelineLayout);
    if (res != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed for compute pipeline layout\n");
        return res;
    }
    return VK_SUCCESS;
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

    const VkPhysicalDeviceVulkan12Features vk12DeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = NULL,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        .descriptorIndexing = VK_TRUE,
        .separateDepthStencilLayouts = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
    };
    const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicState = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = (void*)&vk12DeviceFeatures,
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
        .shaderInt64 = VK_TRUE,//TODO: try to fallback to 32 bit version if not present
    };

    unsigned extensionCount = 0;
    if (vki.vkEnumerateDeviceExtensionProperties(grPhysicalGpu->physicalDevice, NULL, &extensionCount, NULL) != VK_SUCCESS) {
        LOGE("vkEnumerateDeviceExtensionProperties failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    VkExtensionProperties* extensionProperties = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * extensionCount);
    if (extensionProperties == NULL) {
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    if (vki.vkEnumerateDeviceExtensionProperties(grPhysicalGpu->physicalDevice, NULL, &extensionCount, extensionProperties) != VK_SUCCESS) {
        LOGE("vkEnumerateDeviceExtensionProperties failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    const char *deviceExtensions[] = {
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        NULL,
    };
    unsigned deviceExtensionCount = 4;// required extensions
    bool pushDescriptorsSupported = false;
    for (unsigned i = 0; i < extensionCount; ++i) {
        if (strcmp(extensionProperties[i].extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) == 0) {
            deviceExtensions[deviceExtensionCount++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
            pushDescriptorsSupported = true;
            LOGT("push descriptor set is supported\n");
            break;
        }
    }

    const VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &demoteToHelperInvocation,
        .flags = 0,
        .queueCreateInfoCount = pCreateInfo->queueRecordCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = deviceExtensionCount,
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

    unsigned descriptorCount = 10240;
    const VkDescriptorSetLayoutBinding globalLayoutBindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = NULL,
        }
    };

    const VkDescriptorPoolSize poolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = descriptorCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = descriptorCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptorCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = descriptorCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = descriptorCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorCount,
        }
    };
    const VkDescriptorBindingFlags globalLayoutBindingFlags[] = {
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    };
    const VkDescriptorSetLayoutBindingFlagsCreateInfo layoutFlags = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = NULL,
        .bindingCount = COUNT_OF(globalLayoutBindingFlags),
        .pBindingFlags = globalLayoutBindingFlags,
    };
    const VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &layoutFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        .bindingCount = COUNT_OF(globalLayoutBindings),
        .pBindings = globalLayoutBindings,
    };
    const VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        .maxSets = 1,
        .poolSizeCount = COUNT_OF(poolSizes),
        .pPoolSizes = poolSizes,
    };
    VkDescriptorSetLayout globalLayout;
    VkDescriptorSetLayout graphicsDynamicMemoryLayout, computeDynamicMemoryLayout;
    GrGlobalPipelineLayouts globalPipelineLayouts = {};
    if (vkd.vkCreateDescriptorSetLayout(vkDevice, &layoutCreateInfo, NULL,
                                        &globalLayout) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    const VkDescriptorSetLayoutBinding graphicsDynamicLayoutBindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = NULL,
        }
    };
    VkDescriptorSetLayoutCreateInfo dynamicMemoryLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = pushDescriptorsSupported ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0,
        .bindingCount = COUNT_OF(graphicsDynamicLayoutBindings),
        .pBindings = graphicsDynamicLayoutBindings,
    };

    if (vkd.vkCreateDescriptorSetLayout(vkDevice, &dynamicMemoryLayoutCreateInfo, NULL, &graphicsDynamicMemoryLayout) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    const VkDescriptorSetLayoutBinding computeLayoutBindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL,
        }
    };
    dynamicMemoryLayoutCreateInfo.pBindings = computeLayoutBindings;
    if (vkd.vkCreateDescriptorSetLayout(vkDevice, &dynamicMemoryLayoutCreateInfo, NULL, &computeDynamicMemoryLayout) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    if (getVkPipelineLayout(vkDevice, &vkd, globalLayout, graphicsDynamicMemoryLayout, computeDynamicMemoryLayout, &globalPipelineLayouts) != VK_SUCCESS) {
        LOGE("failed to create pipeline layouts\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    VkDescriptorPool globalPool;
    if (vkd.vkCreateDescriptorPool(vkDevice, &poolCreateInfo, NULL,
                                   &globalPool) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    const VkDescriptorSetAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = globalPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &globalLayout,
    };
    VkDescriptorSet globalDescSet;
    if (vkd.vkAllocateDescriptorSets(vkDevice, &allocateInfo,
                                   &globalDescSet) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool failed\n");
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }

    GrDevice* grDevice = malloc(sizeof(GrDevice));
    if (grDevice == NULL) {
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    bool* descriptorHandleBuffer = (bool*)malloc(sizeof(bool) * 3 * descriptorCount);
    if (descriptorHandleBuffer == NULL) {
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    memset(descriptorHandleBuffer, 0, sizeof(bool) * 3 * descriptorCount);
    if (descriptorHandleBuffer == NULL) {
        res = GR_ERROR_INITIALIZATION_FAILED;
        goto bail;
    }
    *grDevice = (GrDevice) {
        .grBaseObj = { GR_OBJ_TYPE_DEVICE },
        .vkd = vkd,
        .device = vkDevice,
        .physicalDevice = grPhysicalGpu->physicalDevice,
        .memoryProperties = memoryProperties,
        .universalQueueIndex = universalQueueIndex,
        .computeQueueIndex = computeQueueIndex,
        .globalDescriptorSet = {
            .descriptorTableLayout = globalLayout,
            .graphicsDynamicMemoryLayout = graphicsDynamicMemoryLayout,
            .computeDynamicMemoryLayout = computeDynamicMemoryLayout,
            .descriptorPool = globalPool,
            .descriptorTable = globalDescSet,
            .samplers = descriptorHandleBuffer,
            .samplerPtr = descriptorHandleBuffer,
            .bufferViews = descriptorHandleBuffer + descriptorCount,
            .bufferViewPtr = descriptorHandleBuffer + descriptorCount,//TODO: support mutable descriptors
            .images = descriptorHandleBuffer + 2 * descriptorCount,
            .imagePtr = descriptorHandleBuffer + 2 * descriptorCount,
            .descriptorCount = descriptorCount,
        },
        .pipelineLayouts = globalPipelineLayouts,
        .vDescriptorSetMemoryTypeIndex = 2,
        .pushDescriptorSetSupported = pushDescriptorsSupported,
    };

    grDevice->vDescriptorSetMemoryTypeIndex = getVirtualDescriptorSetBufferMemoryType(&grDevice->memoryProperties);
    *pDevice = (GR_DEVICE)grDevice;

bail:
    for (int i = 0; i < pCreateInfo->queueRecordCount; i++) {
        free((void*)queueCreateInfos[i].pQueuePriorities);
    }
    if (queueCreateInfos != NULL) {
        free(queueCreateInfos);
    }
    if (extensionProperties != NULL) {
        free(extensionProperties);
    }
    if (res != GR_SUCCESS) {
        if (globalPipelineLayouts.graphicsPipelineLayout != VK_NULL_HANDLE) {
            vkd.vkDestroyPipelineLayout(vkDevice, globalPipelineLayouts.graphicsPipelineLayout, NULL);
        }
        if (globalPipelineLayouts.computePipelineLayout != VK_NULL_HANDLE) {
            vkd.vkDestroyPipelineLayout(vkDevice, globalPipelineLayouts.computePipelineLayout, NULL);
        }
        if (graphicsDynamicMemoryLayout != VK_NULL_HANDLE) {
            vkd.vkDestroyDescriptorSetLayout(vkDevice, graphicsDynamicMemoryLayout, NULL);
        }
        if (computeDynamicMemoryLayout != VK_NULL_HANDLE) {
            vkd.vkDestroyDescriptorSetLayout(vkDevice, computeDynamicMemoryLayout, NULL);
        }
        if (globalLayout != VK_NULL_HANDLE) {
            vkd.vkDestroyDescriptorSetLayout(vkDevice, globalLayout, NULL);
        }
        if (globalPool != VK_NULL_HANDLE) {
            vkd.vkDestroyDescriptorPool(vkDevice, globalPool, NULL);
        }
        if (vkDevice != VK_NULL_HANDLE) {
            vkd.vkDestroyDevice(vkDevice, NULL);
        }
        if (descriptorHandleBuffer != NULL) {
            free(descriptorHandleBuffer);
        }
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
    if (grDevice->pipelineLayouts.graphicsPipelineLayout != VK_NULL_HANDLE) {
        VKD.vkDestroyPipelineLayout(grDevice->device, grDevice->pipelineLayouts.graphicsPipelineLayout, NULL);
    }
    if (grDevice->pipelineLayouts.computePipelineLayout != VK_NULL_HANDLE) {
        VKD.vkDestroyPipelineLayout(grDevice->device, grDevice->pipelineLayouts.computePipelineLayout, NULL);
    }
    if (grDevice->globalDescriptorSet.graphicsDynamicMemoryLayout != VK_NULL_HANDLE) {
        VKD.vkDestroyDescriptorSetLayout(grDevice->device, grDevice->globalDescriptorSet.graphicsDynamicMemoryLayout, NULL);
    }
    if (grDevice->globalDescriptorSet.computeDynamicMemoryLayout != VK_NULL_HANDLE) {
        VKD.vkDestroyDescriptorSetLayout(grDevice->device, grDevice->globalDescriptorSet.computeDynamicMemoryLayout, NULL);
    }
    if (grDevice->globalDescriptorSet.descriptorTableLayout != VK_NULL_HANDLE) {
        VKD.vkDestroyDescriptorSetLayout(grDevice->device, grDevice->globalDescriptorSet.descriptorTableLayout, NULL);
    }
    if (grDevice->globalDescriptorSet.descriptorPool != VK_NULL_HANDLE) {
        VKD.vkDestroyDescriptorPool(grDevice->device, grDevice->globalDescriptorSet.descriptorPool, NULL);
    }
    if (grDevice->globalDescriptorSet.samplers != NULL) {
        free(grDevice->globalDescriptorSet.samplers); // single allocation, samplers are first in the buffer
    }
    VKD.vkDestroyDevice(grDevice->device, NULL);
    free(grDevice);

    return GR_SUCCESS;
}
