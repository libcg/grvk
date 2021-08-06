#include "mantle_internal.h"

// Multi-Device Management Functions

GR_RESULT grGetMultiGpuCompatibility(
    GR_PHYSICAL_GPU gpu0,
    GR_PHYSICAL_GPU gpu1,
    GR_GPU_COMPATIBILITY_INFO* pInfo)
{
    LOGT("%p %p %p\n", gpu0, gpu1, pInfo);

    if (gpu0 == NULL || gpu1 == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(gpu0) != GR_OBJ_TYPE_PHYSICAL_GPU ||
               GET_OBJ_TYPE(gpu1) != GR_OBJ_TYPE_PHYSICAL_GPU) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pInfo == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    LOGW("semi-stub\n");
    *pInfo = (GR_GPU_COMPATIBILITY_INFO) {
        .compatibilityFlags = 0,
    };

    return GR_SUCCESS;
}
