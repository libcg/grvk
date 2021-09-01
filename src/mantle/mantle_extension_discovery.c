#include "mantle_internal.h"

// Extension Discovery Functions

GR_RESULT GR_STDCALL grGetExtensionSupport(
    GR_PHYSICAL_GPU gpu,
    const GR_CHAR* pExtName)
{
    LOGT("%p %s\n", gpu, pExtName);

    if (strcmp(pExtName, "GR_WSI_WINDOWS") == 0) {
        return GR_SUCCESS;
    } else if (strcmp(pExtName, "GR_BORDER_COLOR_PALETTE") == 0) {
        // TODO implement
        return GR_SUCCESS;
    }

    return GR_UNSUPPORTED;
}
