#include "mantle/mantleExt.h"
#include "mantle_object.h"
#include "logger.h"

// Border Color Palette Extension Functions

GR_RESULT grCreateBorderColorPalette(
    GR_DEVICE device,
    const GR_BORDER_COLOR_PALETTE_CREATE_INFO* pCreateInfo,
    GR_BORDER_COLOR_PALETTE* pPalette)
{
    LOGT("%p %p %p\n", device, pCreateInfo, pPalette);
    GrDevice* grDevice = (GrDevice*)device;

    if (grDevice == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grDevice) != GR_OBJ_TYPE_DEVICE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pCreateInfo->paletteSize == 0) {
        return GR_ERROR_INVALID_VALUE;
    } else if (pCreateInfo == NULL || pPalette == NULL) {
        return GR_ERROR_INVALID_POINTER;
    }

    LOGW("semi-stub\n");
    GrBorderColorPalette* grBorderColorPalette = malloc(sizeof(GrBorderColorPalette));
    *grBorderColorPalette = (GrBorderColorPalette) {
        .grObj = { GR_OBJ_TYPE_BORDER_COLOR_PALETTE, grDevice },
    };

    *pPalette = (GR_BORDER_COLOR_PALETTE)grBorderColorPalette;
    return GR_SUCCESS;
}
