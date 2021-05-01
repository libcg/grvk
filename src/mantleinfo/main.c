#include <stdio.h>
#include <stdlib.h>
#include "mantle_loader.h"

//static void getGpuInfo(
//    GR_PHYSICAL_GPU gpu,
//    GR_ENUM infoType,)

static void printPhysicalGpuInfo(
    GR_PHYSICAL_GPU gpu)
{
    GR_RESULT res;
    GR_SIZE dataSize;
    GR_PHYSICAL_GPU_PROPERTIES gpuProps;

    res = gri.grGetGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES, &dataSize, NULL);
    if (res != GR_SUCCESS) {
        printf("grGetGpuInfo failed\n");
        exit(1);
    }
    if (dataSize > sizeof(gpuProps)) {
        printf("%llu > %llu for info type 0x%X\n",
               dataSize, sizeof(gpuProps), GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES);
        dataSize = sizeof(gpuProps);
    }
    res = gri.grGetGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES, &dataSize, &gpuProps);
    if (res != GR_SUCCESS) {
        printf("grGetGpuInfo failed\n");
        exit(1);
    }
    printf(">> GR_PHYSICAL_GPU_PROPERTIES\n"
           "apiVersion=0x%X\ndriverVersion=0x%X\nvendorId=0x%X\ndeviceId=0x%X\n"
           "gpuType=0x%X\ngpuName=\"%s\"\nmaxMemRefsPerSubmission=%u\nreserved=%llu\n"
           "maxInlineMemoryUpdateSize=%llu\nmaxBoundDescriptorSets=%u\nmaxThreadGroupSize=%u\n"
           "timestampFrequency=%llu\nmultiColorTargetClears=%d\n",
           gpuProps.apiVersion, gpuProps.driverVersion, gpuProps.vendorId, gpuProps.deviceId,
           gpuProps.gpuType, gpuProps.gpuName, gpuProps.maxMemRefsPerSubmission, gpuProps.reserved,
           gpuProps.maxInlineMemoryUpdateSize, gpuProps.maxBoundDescriptorSets,
           gpuProps.maxThreadGroupSize, gpuProps.timestampFrequency,
           gpuProps.multiColorTargetClears);

    //GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE
    //GR_INFO_TYPE_PHYSICAL_GPU_QUEUE_PROPERTIES
    //GR_INFO_TYPE_PHYSICAL_GPU_MEMORY_PROPERTIES
    //GR_INFO_TYPE_PHYSICAL_GPU_IMAGE_PROPERTIES
}

int main(
    int argc,
    char* argv[])
{
    GR_RESULT res;

    printf("\nMANTLEINFO\n\n");

    bool init = mantleLoaderInit();

    if (!init) {
        printf("failed to load mantle64.dll\n");
        return 1;
    }

    const GR_APPLICATION_INFO appInfo = {
        .pAppName = "mantleinfo",
        .appVersion = 0,
        .pEngineName = "mantleinfo",
        .engineVersion = 0,
        .apiVersion = 0,
    };

    GR_UINT gpuCount = 0;
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS];
    res = gri.grInitAndEnumerateGpus(&appInfo, NULL, &gpuCount, gpus);

    printf("found %u physical GPU(s)\n\n", gpuCount);

    for (unsigned i = 0; i < gpuCount; i++) {
        printf(">>>> GPU %d:\n", i);
        printPhysicalGpuInfo(gpus[i]);
    }
    return 0;
}
