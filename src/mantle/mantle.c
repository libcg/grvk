#include <stdio.h>
#include <stdlib.h>
#include "mantle/mantle.h"
#include "vulkan/vulkan.h"

static GR_ALLOC_FUNCTION mAllocFun = NULL;
static GR_FREE_FUNCTION mFreeFun = NULL;
static VkInstance mVkInstance = NULL;

static GR_VOID* grvkAlloc(
    GR_SIZE size,
    GR_SIZE alignment,
    GR_ENUM allocType)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#endif
}

static GR_VOID grvkFree(
    GR_VOID* pMem)
{
    free(pMem);
}

GR_RESULT grInitAndEnumerateGpus(
    const GR_APPLICATION_INFO* pAppInfo,
    const GR_ALLOC_CALLBACKS* pAllocCb,
    GR_UINT* pGpuCount,
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS])
{
    printf("grInitAndEnumerateGpus\n"
           "- app: %s (%08X)\n"
           "- engine: %s (%08X)\n"
           "- api: %08X\n",
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

    if (mVkInstance != NULL) {
        vkDestroyInstance(mVkInstance, NULL);
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
        .pApplicationInfo = &appInfo,
    };

    if (vkCreateInstance(&createInfo, NULL, &mVkInstance) != VK_SUCCESS) {
        return GR_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > GR_MAX_PHYSICAL_GPUS) {
        physicalDeviceCount = GR_MAX_PHYSICAL_GPUS;
    }

    VkPhysicalDevice physicalDevices[GR_MAX_PHYSICAL_GPUS];
    vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount,
                               (VkPhysicalDevice*)physicalDevices);

    *pGpuCount = physicalDeviceCount;
    for (int i = 0; i < *pGpuCount; i++) {
        gpus[i] = (GR_PHYSICAL_GPU)physicalDevices[i];
    }

    return GR_SUCCESS;
}
