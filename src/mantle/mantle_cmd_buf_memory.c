#include "mantle_internal.h"

static VkImageSubresourceLayers getVkImageSubresourceLayers(
    const GR_IMAGE_SUBRESOURCE* subresource)
{
    return (VkImageSubresourceLayers){
        .aspectMask = getVkImageAspectFlags(subresource->aspect),
        .mipLevel = subresource->mipLevel,
        .baseArrayLayer = subresource->arraySlice,
        .layerCount = 1
    };
}

GR_VOID grCmdCopyMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_COPY* pRegions)
{
    LOGT("%p %p %p 0x%u %p\n", cmdBuffer, srcMem, destMem, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrGpuMemory* grSrcMemory = (GrGpuMemory*)srcMem;
    GrGpuMemory* grDestMemory = (GrGpuMemory*)srcMem;
    //TODO: avoid unnecessary allocation as VkBufferCopy is completely identical to GR_MEMORY_COPY
    VkBufferCopy *pVkRegions = (VkBufferCopy*)malloc(sizeof(VkBufferCopy) * regionCount);
    for (GR_UINT i = 0; i < regionCount; ++i) {
        pVkRegions[i] = (VkBufferCopy){
            .srcOffset = pRegions[i].srcOffset,
            .dstOffset = pRegions[i].destOffset,
            .size = pRegions[i].copySize,
        };
    }
    vki.vkCmdCopyBuffer(grCmdBuffer->commandBuffer, grSrcMemory->buffer, grDestMemory->buffer, regionCount, pVkRegions);
    free(pVkRegions);
}

GR_VOID grCmdCopyImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p 0x%u %p\n", cmdBuffer, srcImage, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrImage* grSrcImage  = (GrImage*)srcImage;
    GrImage* grDestImage = (GrImage*)destImage;
    VkImageCopy *pVkRegions = (VkImageCopy*)malloc(sizeof(VkImageCopy) * regionCount);
    for (GR_UINT i = 0; i < regionCount; ++i) {
        pVkRegions[i] = (VkImageCopy){
            .srcSubresource = getVkImageSubresourceLayers(&pRegions[i].srcSubresource),
            .srcOffset = {
                .x = pRegions[i].srcOffset.x,
                .y = pRegions[i].srcOffset.y,
                .z = pRegions[i].srcOffset.z,
            },
            .dstSubresource = getVkImageSubresourceLayers(&pRegions[i].destSubresource),
            .dstOffset = {
                .x = pRegions[i].destOffset.x,
                .y = pRegions[i].destOffset.y,
                .z = pRegions[i].destOffset.z,
            },
            .extent = {
                .width  = pRegions[i].extent.width,
                .height = pRegions[i].extent.height,
                .depth  = pRegions[i].extent.depth,
            }
        };
    }
    vki.vkCmdCopyImage(grCmdBuffer->commandBuffer, grSrcImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, grDestImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, pVkRegions);
    free(pVkRegions);
}

static void mapBufferCopyRanges(GR_UINT regionCount, const GR_MEMORY_IMAGE_COPY* pRegions, VkBufferImageCopy * pVkRegions)
{
    for (GR_UINT i = 0; i < regionCount;++i) {
        pVkRegions[i] = (VkBufferImageCopy){
            .bufferOffset = pRegions[i].memOffset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageOffset = {
                .x = pRegions[i].imageOffset.x,
                .y = pRegions[i].imageOffset.y,
                .z = pRegions[i].imageOffset.z,
            },
            .imageExtent = {
                .width  = pRegions[i].imageExtent.width,
                .height = pRegions[i].imageExtent.height,
                .depth  = pRegions[i].imageExtent.depth,
            },
        };
    }
}

GR_VOID grCmdCopyMemoryToImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY srcMem,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p 0x%u %p\n", cmdBuffer, srcMem, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrGpuMemory* grMemory = (GrGpuMemory*)srcMem;
    GrImage* grImage = (GrImage*)destImage;
    VkBufferImageCopy* vkRegions = (VkBufferImageCopy*)malloc(sizeof(VkBufferImageCopy) * regionCount);
    mapBufferCopyRanges(regionCount, pRegions, vkRegions);
    vki.vkCmdCopyBufferToImage(grCmdBuffer->commandBuffer, grMemory->buffer, grImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, vkRegions);
    free(vkRegions);
}

GR_VOID grCmdCopyImageToMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_GPU_MEMORY destMem,
    GR_UINT regionCount,
    const GR_MEMORY_IMAGE_COPY* pRegions)
{
    LOGT("%p %p %p 0x%u %p\n", cmdBuffer, srcImage, destMem, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrGpuMemory* grMemory = (GrGpuMemory*)destMem;
    GrImage* grImage = (GrImage*)srcImage;
    VkBufferImageCopy* vkRegions = (VkBufferImageCopy*)malloc(sizeof(VkBufferImageCopy) * regionCount);
    mapBufferCopyRanges(regionCount, pRegions, vkRegions);
    vki.vkCmdCopyImageToBuffer(grCmdBuffer->commandBuffer, grImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, grMemory->buffer, regionCount, vkRegions);
    free(vkRegions);
}

GR_VOID grCmdUpdateMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE dataSize,
    const GR_UINT32* pData)
{
    LOGT("%p %p 0x%lX 0x%lX %p\n", cmdBuffer, destMem, destOffset, dataSize, pData);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrGpuMemory* grMemory = (GrGpuMemory*)destMem;
    vki.vkCmdUpdateBuffer(grCmdBuffer->commandBuffer, grMemory->buffer, destOffset, dataSize, pData);
}

GR_VOID grCmdResolveImage(
    GR_CMD_BUFFER cmdBuffer,
    GR_IMAGE srcImage,
    GR_IMAGE destImage,
    GR_UINT regionCount,
    const GR_IMAGE_RESOLVE* pRegions)
{
    LOGT("%p %p %p %u %p\n", cmdBuffer, srcImage, destImage, regionCount, pRegions);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrImage* grSrcImage = (GrImage*)srcImage;
    GrImage* grDstImage = (GrImage*)destImage;
    VkImageResolve* pVkRegions = (VkImageResolve*)malloc(sizeof(VkImageResolve) * regionCount);
    for (GR_UINT i = 0; i < regionCount; ++i) {
        pVkRegions[i] = (VkImageResolve) {
            .srcSubresource = getVkImageSubresourceLayers(&pRegions[i].srcSubresource),
            .srcOffset = {
                .x = pRegions[i].srcOffset.x,
                .y = pRegions[i].srcOffset.y,
            },
            .dstSubresource = getVkImageSubresourceLayers(&pRegions[i].destSubresource),
            .srcOffset = {
                .x = pRegions[i].destOffset.x,
                .y = pRegions[i].destOffset.y,
            },
            .extent = {
                .width  = pRegions[i].extent.width,
                .height = pRegions[i].extent.height,
            }
        };
    }
    vki.vkCmdResolveImage(grCmdBuffer->commandBuffer,
                          grSrcImage->image,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          grDstImage->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          regionCount, pVkRegions);
    free(pVkRegions);
}

GR_VOID grCmdFillMemory(
    GR_CMD_BUFFER cmdBuffer,
    GR_GPU_MEMORY destMem,
    GR_GPU_SIZE destOffset,
    GR_GPU_SIZE fillSize,
    GR_UINT32 data)
{
    LOGT("%p %p 0x%lX 0x%lX %u\n", cmdBuffer, destMem, destOffset, fillSize, data);
    GrCmdBuffer* grCmdBuffer = (GrCmdBuffer*)cmdBuffer;
    GrGpuMemory* grMemory = (GrGpuMemory*)destMem;
    vki.vkCmdFillBuffer(grCmdBuffer->commandBuffer, grMemory->buffer, destOffset, fillSize, data);
}
