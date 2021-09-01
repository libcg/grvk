#include "mantle/mantleExt.h"
#include "logger.h"

// Border Color Palette Extension Functions

GR_VOID grCmdBindBorderColorPalette(
    GR_CMD_BUFFER cmdBuffer,
    GR_ENUM pipelineBindPoint,
    GR_BORDER_COLOR_PALETTE palette)
{
    LOGW("STUB\n");
}

// Advanced Multisampling Extnension Functions

GR_RESULT GR_STDCALL grCreateAdvancedMsaaState(
    GR_DEVICE device,
    const GR_ADVANCED_MSAA_STATE_CREATE_INFO* pCreateInfo,
    GR_MSAA_STATE_OBJECT* pState)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCreateFmaskImageView(
    GR_DEVICE device,
    const GR_FMASK_IMAGE_VIEW_CREATE_INFO* pCreateInfo,
    GR_IMAGE_VIEW* pView)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Copy Occlusion Query Data Extension Functions

GR_RESULT GR_STDCALL grCmdCopyOcclusionData(
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

GR_RESULT GR_STDCALL grQueueDelay(
    GR_QUEUE queue,
    GR_FLOAT delay)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCalibrateGpuTimestamp(
    GR_DEVICE device,
    GR_GPU_TIMESTAMP_CALIBRATION* pCalibrationData)
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

// Undocumented Functions

GR_RESULT GR_STDCALL grAddEmulatedPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grAddPerfExperimentCounter()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grAddPerfExperimentTrace()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grBlankPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdBeginPerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdBindUserData()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdCopyRegisterToMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdDispatchOffset()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdDispatchOffsetIndirect()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdEndPerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdInsertTraceMarker()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdWaitMemoryValue()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCmdWaitRegisterValue()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCreatePerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCreatePrivateDisplayImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grCreateVirtualDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grDestroyVirtualDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grDisablePrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grEnablePrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grEnablePrivateDisplayAudio()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grFinalizePerfExperiment()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grGetPrivateDisplayScanLine()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grGetPrivateDisplays()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grGetVirtualDisplayProperties()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grOpenExternalSharedPrivateDisplayImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grPrivateDisplayPresent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grQueueDelayAfterVsync()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grQueueMigrateObjects()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grQueueSetExecutionPriority()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grRegisterPowerEvent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grRegisterPrivateDisplayEvent()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grRemoveEmulatedPrivateDisplay()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetEventAfterVsync()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetPowerDefaultPerformance()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetPowerProfile()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetPowerRegions()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetPrivateDisplayPowerMode()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grSetPrivateDisplaySettings()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grWinAllocMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grWinOpenExternalSharedImage()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grWinOpenExternalSharedMemory()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}

GR_RESULT GR_STDCALL grWinOpenExternalSharedQueueSemaphore()
{
    LOGW("STUB\n");
    return GR_UNSUPPORTED;
}
