#include "mantle/mantleExt.h"
#include "logger.h"

// Library Versioning Functions

GR_UINT32 grGetExtensionLibraryVersion()
{
    LOGW("STUB\n");
    return 0;
}

// Border Color Palette Extension Functions

GR_RESULT grCreateBorderColorPalette(
    GR_DEVICE device,
    const GR_BORDER_COLOR_PALETTE_CREATE_INFO* pCreateInfo,
    GR_BORDER_COLOR_PALETTE* pPalette)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grUpdateBorderColorPalette(
    GR_BORDER_COLOR_PALETTE palette,
    GR_UINT firstEntry,
    GR_UINT entryCount,
    const GR_FLOAT* pEntries)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_VOID grCmdBindBorderColorPalette(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_BORDER_COLOR_PALETTE palette)
{
    LOGW("STUB\n");
}

// Advanced Multisampling Extnension Functions

GR_RESULT grCreateAdvancedMsaaState(
    GR_DEVICE device,
    const GR_ADVANCED_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateFmaskImageView(
    GR_DEVICE device,
    const GR_FMASK_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
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
    LOGW("STUB\n");
}

GR_VOID grCmdResetOcclusionPredication(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGW("STUB\n");
}

GR_VOID grCmdSetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY mem,
    GR_GPU_SIZE offset)
{
    LOGW("STUB\n");
}

GR_VOID grCmdResetMemoryPredication(
    GR_CMD_BUFFER cmdBuffer)
{
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
    LOGW("STUB\n");
}

GR_VOID grCmdElse(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGW("STUB\n");
}

GR_VOID grCmdEndIf(
    GR_CMD_BUFFER cmdBuffer)
{
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
    LOGW("STUB\n");
}

GR_VOID grCmdEndWhile(
    GR_CMD_BUFFER cmdBuffer)
{
    LOGW("STUB\n");
}

// Timer Queue Extension Functions

GR_RESULT grQueueDelay(
    GR_QUEUE queue,
    GR_FLOAT delay)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCalibrateGpuTimestamp(
    GR_DEVICE device,
    GR_GPU_TIMESTAMP_CALIBRATION* pCalibrationData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Undocumented Functions

GR_RESULT grAddEmulatedPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grAddPerfExperimentCounter()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grAddPerfExperimentTrace()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grBlankPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdBeginPerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdBindUserData()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdCopyRegisterToMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdDispatchOffset()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdDispatchOffsetIndirect()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdEndPerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdInsertTraceMarker()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdWaitMemoryValue()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCmdWaitRegisterValue()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreatePerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreatePrivateDisplayImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grCreateVirtualDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDestroyVirtualDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grDisablePrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grEnablePrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grEnablePrivateDisplayAudio()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grFinalizePerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetPrivateDisplayScanLine()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetPrivateDisplays()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grGetVirtualDisplayProperties()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grOpenExternalSharedPrivateDisplayImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grPrivateDisplayPresent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueDelayAfterVsync()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueMigrateObjects()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grQueueSetExecutionPriority()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRegisterPowerEvent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRegisterPrivateDisplayEvent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grRemoveEmulatedPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetEventAfterVsync()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerDefaultPerformance()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerProfile()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPowerRegions()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPrivateDisplayPowerMode()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grSetPrivateDisplaySettings()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinAllocMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT grWinOpenExternalSharedQueueSemaphore()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}
