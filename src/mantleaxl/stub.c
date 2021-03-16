#include <stdbool.h>
#include "mantle/mantleExt.h"
#include "logger.h"

static void initLogger()
{
    static bool init = false;

    if (init) {
        return;
    }

    logInit("GRVK_AXL_LOG_PATH", "grvk_axl.log");
    init = true;
}

// Library Versioning Functions

GR_UINT32 grGetExtensionLibraryVersion()
{
    initLogger();
    LOGW("STUB\n");
    return 0;
}

// Border Color Palette Extension Functions

GR_RESULT grCreateBorderColorPalette(
    GR_DEVICE device,
    const GR_BORDER_COLOR_PALETTE_CREATE_INFO* pCreateInfo,
    GR_BORDER_COLOR_PALETTE* pPalette)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grUpdateBorderColorPalette(
    GR_BORDER_COLOR_PALETTE palette,
    GR_UINT firstEntry,
    GR_UINT entryCount,
    const GR_FLOAT* pEntries)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_VOID grCmdBindBorderColorPalette(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_BORDER_COLOR_PALETTE palette)
{
    initLogger();
    LOGW("STUB\n");
}

// Advanced Multisampling Extnension Functions

GR_RESULT grCreateAdvancedMsaaState(
    GR_DEVICE device,
    const GR_ADVANCED_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateFmaskImageView(
    GR_DEVICE device,
    const GR_FMASK_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Copy Occlusion Query Data Extension Functions

GR_RESULT grCmdCopyOcclusionData(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT startQuery,
    GR_UINT queryCount,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_BOOL accumulateData)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Command Buffer Control Flow Extension Functions

GR_VOID grCmdSetOcclusionPredication(
    GR_CMD_BUFFER cmdBuffer,
    GR_QUERY_POOL queryPool,
    GR_UINT slot,
    GR_ENUM condition,
    GR_BOOL waitResults,
    GR_BOOL accumulateData)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdResetOcclusionPredication(
    GR_CMD_BUFFER cmdBuffer)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdSetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdResetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdIf(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset,
    GR_UINT64 data,
    GR_UINT64 mask,
    GR_ENUM func)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdElse(
    GR_CMD_BUFFER cmdBuffer)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdEndIf(
    GR_CMD_BUFFER cmdBuffer)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdWhile(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_SIZE srcOffset,
    GR_UINT64 data,
    GR_UINT64 mask,
    GR_ENUM func)
{
    initLogger();
    LOGW("STUB\n");
}

GR_VOID grCmdEndWhile(
    GR_CMD_BUFFER cmdBuffer)
{
    initLogger();
    LOGW("STUB\n");
}

// Timer Queue Extension Functions

GR_RESULT grQueueDelay(
    GR_QUEUE queue,
    GR_FLOAT delay)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCalibrateGpuTimestamp(
    GR_DEVICE device,
    GR_GPU_TIMESTAMP_CALIBRATION* pCalibrationData)
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Undocumented Functions

GR_RESULT grAddEmulatedPrivateDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grAddPerfExperimentCounter()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grAddPerfExperimentTrace()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grBlankPrivateDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdBeginPerfExperiment()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdBindUserData()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdCopyRegisterToMemory()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdDispatchOffset()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdDispatchOffsetIndirect()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdEndPerfExperiment()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdInsertTraceMarker()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdWaitMemoryValue()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdWaitRegisterValue()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreatePerfExperiment()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreatePrivateDisplayImage()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateVirtualDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDestroyVirtualDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDisablePrivateDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grEnablePrivateDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grEnablePrivateDisplayAudio()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grFinalizePerfExperiment()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetPrivateDisplayScanLine()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetPrivateDisplays()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetVirtualDisplayProperties()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenExternalSharedPrivateDisplayImage()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grPrivateDisplayPresent()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueDelayAfterVsync()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueMigrateObjects()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueSetExecutionPriority()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRegisterPowerEvent()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRegisterPrivateDisplayEvent()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRemoveEmulatedPrivateDisplay()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetEventAfterVsync()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerDefaultPerformance()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerProfile()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerRegions()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPrivateDisplayPowerMode()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPrivateDisplaySettings()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinAllocMemory()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedImage()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedMemory()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedQueueSemaphore()
{
    initLogger();
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}
