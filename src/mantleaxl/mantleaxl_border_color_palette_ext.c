#include "mantle/mantleExt.h"
#include "mantle_object.h"
#include "logger.h"

// Border Color Palette Extension Functions

GR_RESULT GR_STDCALL grCreateBorderColorPalette(
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

    GrBorderColorPalette* grBorderColorPalette = malloc(sizeof(GrBorderColorPalette));
    *grBorderColorPalette = (GrBorderColorPalette) {
        .grObj = { GR_OBJ_TYPE_BORDER_COLOR_PALETTE, grDevice },
        .size = pCreateInfo->paletteSize,
        .data = malloc(4 * pCreateInfo->paletteSize * sizeof(float)),
    };

    // HACK: pass the palette through GrDevice so we don't have to defer sampler creation
    if (grDevice->grBorderColorPalette != NULL) {
        LOGW("multiple border color palettes are not supported\n");
    } else {
        grDevice->grBorderColorPalette = grBorderColorPalette;
    }

    *pPalette = (GR_BORDER_COLOR_PALETTE)grBorderColorPalette;
    return GR_SUCCESS;
}

GR_RESULT GR_STDCALL grUpdateBorderColorPalette(
    GR_BORDER_COLOR_PALETTE palette,
    GR_UINT firstEntry,
    GR_UINT entryCount,
    const GR_FLOAT* pEntries)
{
    LOGT("%p %u %u %p\n", palette, firstEntry, entryCount, pEntries);
    GrBorderColorPalette* grBorderColorPalette = (GrBorderColorPalette*)palette;

    if (grBorderColorPalette == NULL) {
        return GR_ERROR_INVALID_HANDLE;
    } else if (GET_OBJ_TYPE(grBorderColorPalette) != GR_OBJ_TYPE_BORDER_COLOR_PALETTE) {
        return GR_ERROR_INVALID_OBJECT_TYPE;
    } else if (pEntries == NULL) {
        return GR_ERROR_INVALID_POINTER;
    } else if (firstEntry + entryCount > grBorderColorPalette->size) {
        return GR_ERROR_INVALID_VALUE;
    }

    memcpy(&grBorderColorPalette->data[4 * firstEntry], pEntries, 4 * entryCount * sizeof(float));

    return GR_SUCCESS;
}

GR_VOID grCmdBindBorderColorPalette(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_BORDER_COLOR_PALETTE palette)
{
    LOGT("%p 0x%X %p\n", cmdBuffer, pipelineBindPoint, palette);

    // No-op
}
