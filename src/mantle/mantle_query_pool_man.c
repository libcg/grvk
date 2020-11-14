#include "mantle_internal.h"
const static VkQueryPipelineStatisticFlags allPipelineStatisticFlags =
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

GR_RESULT grCreateQueryPool(
    GR_DEVICE device,
    const GR_QUERY_POOL_CREATE_INFO* pCreateInfo,
    GR_QUERY_POOL* pQueryPool)
{
    LOGW("%p %p %p\n", device, pCreateInfo, pQueryPool);
    GrDevice* grDevice = (GrDevice*)device;
    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grDevice->sType != GR_STRUCT_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pCreateInfo == NULL || pQueryPool == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    if (pCreateInfo->slots == 0) {
        return GR_ERROR_INVALID_VALUE;
    }
    VkQueryType queryType;
    switch (pCreateInfo->queryType) {
    case GR_QUERY_OCCLUSION:
        queryType = VK_QUERY_TYPE_OCCLUSION;
        break;
    case GR_QUERY_PIPELINE_STATISTICS:
        queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        break;
    default:
        return GR_ERROR_INVALID_VALUE;
    }
    VkQueryPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queryType = queryType,
        .queryCount = pCreateInfo->slots,
        .pipelineStatistics = queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS ? allPipelineStatisticFlags : 0,
    };
    VkQueryPool pool = VK_NULL_HANDLE;
    GrQueryPool *grPool = (GrQueryPool*)malloc(sizeof(GrQueryPool));
    if (grPool == NULL) {
        return GR_ERROR_OUT_OF_MEMORY;
    }
    VkResult vkRes = vki.vkCreateQueryPool(grDevice->device, &createInfo, NULL, &pool);
    if (vkRes != VK_SUCCESS) {
        free(pool);
        return getGrResult(vkRes);
    }
    *grPool = (GrQueryPool) {
        .sType = GR_STRUCT_TYPE_QUERY_POOL,
        .grDevice = grDevice,
        .pool = pool,
        .queryType = queryType,
        .queryCount = pCreateInfo->slots,
    };
    *pQueryPool = (GR_QUERY_POOL)grPool;
    return GR_SUCCESS;
}

GR_RESULT grGetQueryPoolResults(
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_SIZE* pDataSize,
    GR_VOID* pData)
{
    LOGT("%p %u %u %p %p\n", queryPool, startQuery, queryCount, pDataSize, pData);
    GrQueryPool* grPool = (GrQueryPool*)queryPool;
    if (grPool == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    }
    if (grPool->sType != GR_STRUCT_TYPE_QUERY_POOL) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    }
    if (pDataSize == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }
    GR_SIZE stride = grPool->queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS ? sizeof(GR_PIPELINE_STATISTICS_DATA) : sizeof(uint64_t);
    GR_SIZE dataSize = stride * queryCount;
    if (pData == NULL) {
        *pDataSize = dataSize;
        return GR_SUCCESS;
    }
    else if (dataSize > *pDataSize) {
        return GR_ERROR_INVALID_MEMORY_SIZE;
    }
    VkResult vkRes = vki.vkGetQueryPoolResults(grPool->grDevice->device,
                              grPool->pool,
                              startQuery,
                              queryCount,
                              dataSize,
                              pData,
                              stride, VK_QUERY_RESULT_64_BIT);
    if (vkRes != VK_SUCCESS) {
        return getGrResult(vkRes);
    }
    // now simply remap fields in PIPELINE_STATISTICS_DATA if query contains stats
    // Vulkan API's query result for statistics contains 11 uint64_t values for each query if all bits are set just like GR_PIPELINE_STATISTICS_DATA structure does
    if (grPool->queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
        GR_PIPELINE_STATISTICS_DATA cache;
        GR_PIPELINE_STATISTICS_DATA *statsData = (GR_PIPELINE_STATISTICS_DATA*)pData;
        for (GR_UINT i = 0; i < queryCount; ++i) {
            cache = statsData[i];
            statsData[i] = (GR_PIPELINE_STATISTICS_DATA) {
                .psInvocations = cache.iaVertices,   // offset 7
                .cPrimitives   = cache.iaPrimitives, // offset 6
                .cInvocations  = cache.gsPrimitives, // offset 5
                .vsInvocations = cache.cInvocations, // offset 2
                .gsInvocations = cache.vsInvocations,// offset 3
                .gsPrimitives  = cache.gsInvocations,// offset 4
                .iaPrimitives  = cache.cPrimitives,  // offset 1
                .iaVertices    = cache.psInvocations,// offset 0
                .hsInvocations = cache.hsInvocations,// same offsets here I guess
                .dsInvocations = cache.dsInvocations,
                .csInvocations = cache.csInvocations,

            };
        }
    }
    return GR_SUCCESS;
}
