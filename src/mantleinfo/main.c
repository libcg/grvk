#include <stdio.h>
#include <stdlib.h>
#include "mantle_loader.h"

static bool getGpuInfo(
    GR_PHYSICAL_GPU gpu,
    GR_ENUM infoType,
    GR_SIZE expectedSize,
    GR_SIZE* dataSize,
    void* data)
{
    GR_RESULT res;

    res = gri.grGetGpuInfo(gpu, infoType, dataSize, NULL);
    if (res != GR_SUCCESS) {
        printf("grGetGpuInfo failed 0x%X for type 0x%X\n", res, infoType);
        return false;
    }

    if (*dataSize > expectedSize) {
        printf("%zu > %zu for type 0x%X\n", *dataSize, expectedSize, infoType);
        *dataSize = expectedSize;
    } else if (*dataSize != expectedSize) {
        printf("%zu != %zu for type 0x%X\n", *dataSize, expectedSize, infoType);
    }

    res = gri.grGetGpuInfo(gpu, infoType, dataSize, data);
    if (res != GR_SUCCESS) {
        printf("grGetGpuInfo failed 0x%X for type 0x%X\n", res, infoType);
        return false;
    }

    return true;
}

static void printPhysicalGpuInfo(
    GR_PHYSICAL_GPU gpu)
{
    GR_SIZE dataSize;
    GR_PHYSICAL_GPU_PROPERTIES gpuProps;
    GR_PHYSICAL_GPU_PERFORMANCE gpuPerf;
    GR_PHYSICAL_GPU_QUEUE_PROPERTIES gpuQueueProps[4];
    GR_PHYSICAL_GPU_MEMORY_PROPERTIES gpuMemProps;
    GR_PHYSICAL_GPU_IMAGE_PROPERTIES gpuImgProps;

    printf(">> GR_PHYSICAL_GPU_PROPERTIES\n");
    if (getGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_PROPERTIES, sizeof(gpuProps),
                   &dataSize, &gpuProps)) {
        printf("apiVersion=0x%X\n"
               "driverVersion=0x%X\n"
               "vendorId=0x%X\n"
               "deviceId=0x%X\n"
               "gpuType=0x%X\n"
               "gpuName=\"%s\"\n"
               "maxMemRefsPerSubmission=%u\n"
               "reserved=%llu\n"
               "maxInlineMemoryUpdateSize=%llu\n"
               "maxBoundDescriptorSets=%u\n"
               "maxThreadGroupSize=%u\n"
               "timestampFrequency=%llu\n"
               "multiColorTargetClears=%d\n",
               gpuProps.apiVersion, gpuProps.driverVersion, gpuProps.vendorId, gpuProps.deviceId,
               gpuProps.gpuType, gpuProps.gpuName, gpuProps.maxMemRefsPerSubmission,
               gpuProps.reserved, gpuProps.maxInlineMemoryUpdateSize,
               gpuProps.maxBoundDescriptorSets, gpuProps.maxThreadGroupSize,
               gpuProps.timestampFrequency, !!gpuProps.multiColorTargetClears);
    }

    printf(">> GR_PHYSICAL_GPU_PERFORMANCE\n");
    if (getGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_PERFORMANCE, sizeof(gpuPerf),
                   &dataSize, &gpuPerf)) {
        printf("maxGpuClock=%g\n"
               "aluPerClock=%g\n"
               "texPerClock=%g\n"
               "primsPerClock=%g\n"
               "pixelsPerClock=%g\n",
               gpuPerf.maxGpuClock, gpuPerf.aluPerClock, gpuPerf.texPerClock,
               gpuPerf.primsPerClock, gpuPerf.pixelsPerClock);
    }

    printf(">> GR_PHYSICAL_GPU_QUEUE_PROPERTIES\n");
    if (getGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_QUEUE_PROPERTIES, sizeof(gpuQueueProps),
                   &dataSize, gpuQueueProps)) {
        for (unsigned i = 0; i < dataSize / sizeof(gpuQueueProps[0]); i++) {
            printf("queueType=0x%X\n"
                   "queueCount=%u\n"
                   "maxAtomicCounters=%u\n"
                   "supportsTimestamps=%d\n",
                   gpuQueueProps[i].queueType, gpuQueueProps[i].queueCount,
                   gpuQueueProps[i].maxAtomicCounters, gpuQueueProps[i].supportsTimestamps);
        }
    }

    printf(">> GR_PHYSICAL_GPU_MEMORY_PROPERTIES\n");
    if (getGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_MEMORY_PROPERTIES, sizeof(gpuMemProps),
                   &dataSize, &gpuMemProps)) {
        printf("flags=0x%X\n"
               "virtualMemPageSize=%llu\n"
               "maxVirtualMemSize=%llu\n"
               "maxPhysicalMemSize=%llu\n",
               gpuMemProps.flags, gpuMemProps.virtualMemPageSize, gpuMemProps.maxVirtualMemSize,
               gpuMemProps.maxPhysicalMemSize);
    }

    printf(">> GR_PHYSICAL_GPU_IMAGE_PROPERTIES\n");
    if (getGpuInfo(gpu, GR_INFO_TYPE_PHYSICAL_GPU_IMAGE_PROPERTIES, sizeof(gpuImgProps),
                   &dataSize, &gpuImgProps)) {
        printf("maxSliceWidth=%u\n"
               "maxSliceHeight=%u\n"
               "maxDepth=%u\n"
               "maxArraySlices=%u\n"
               "reserved1=%u\n"
               "reserved2=%u\n"
               "maxMemoryAlignment=%llu\n"
               "sparseImageSupportLevel=%u\n"
               "flags=0x%X\n",
               gpuImgProps.maxSliceWidth, gpuImgProps.maxSliceHeight, gpuImgProps.maxDepth,
               gpuImgProps.maxArraySlices, gpuImgProps.reserved1, gpuImgProps.reserved2,
               gpuImgProps.maxMemoryAlignment, gpuImgProps.sparseImageSupportLevel,
               gpuImgProps.flags);
    }
}

int main(int argc, char* argv[])
{
    GR_RESULT res;

    printf("\nMANTLEINFO\n\n");

    if (!mantleLoaderInit()) {
        printf("failed to load mantle64.dll\n");
        return 1;
    }

    const GR_APPLICATION_INFO appInfo = {
        .pAppName = "mantleinfo",
        .appVersion = 1,
        .pEngineName = "mantleinfo",
        .engineVersion = 1,
        .apiVersion = 1,
    };

    GR_UINT gpuCount = 0;
    GR_PHYSICAL_GPU gpus[GR_MAX_PHYSICAL_GPUS];
    res = gri.grInitAndEnumerateGpus(&appInfo, NULL, &gpuCount, gpus);
    if (res != GR_SUCCESS) {
        printf("grInitAndEnumerateGpus failed 0x%X\n", res);
        return 1;
    }

    printf("found %u physical GPU(s)\n\n", gpuCount);

    for (unsigned i = 0; i < gpuCount; i++) {
        printf(">>>> GPU %d:\n", i);
        printPhysicalGpuInfo(gpus[i]);
    }
    return 0;
}
