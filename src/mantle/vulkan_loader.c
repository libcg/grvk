#include "vulkan_loader.h"

#define LOAD_VULKAN_FN(obj, instance, name) \
    (obj)->name = (PFN_##name)vkGetInstanceProcAddr((instance), #name)

#define LOAD_VULKAN_DEV_FN(obj, device, name) \
    (obj)->name = (PFN_##name)vkGetDeviceProcAddr((device), #name)

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device,
    const char* pName);

VULKAN_LIBRARY vkl;
VULKAN_INSTANCE vki;
VkInstance vk;

void vulkanLoaderLibraryInit(
    VULKAN_LIBRARY* vkl)
{
    LOAD_VULKAN_FN(vkl, NULL, vkCreateInstance);
    LOAD_VULKAN_FN(vkl, NULL, vkEnumerateInstanceExtensionProperties);
    LOAD_VULKAN_FN(vkl, NULL, vkEnumerateInstanceLayerProperties);
}

void vulkanLoaderInstanceInit(
    VULKAN_INSTANCE* vki,
    VkInstance instance)
{
    vk = instance;

    LOAD_VULKAN_FN(vki, instance, vkCreateDevice);
    LOAD_VULKAN_FN(vki, instance, vkDestroyInstance);
    LOAD_VULKAN_FN(vki, instance, vkEnumerateDeviceExtensionProperties);
    LOAD_VULKAN_FN(vki, instance, vkEnumeratePhysicalDevices);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceFeatures);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceFeatures2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceFormatProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceFormatProperties2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceImageFormatProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceImageFormatProperties2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceMemoryProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceMemoryProperties2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceProperties2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceQueueFamilyProperties2);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSparseImageFormatProperties);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSparseImageFormatProperties2);

#ifdef VK_KHR_surface
    LOAD_VULKAN_FN(vki, instance, vkDestroySurfaceKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
#endif

#ifdef VK_KHR_swapchain
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDevicePresentRectanglesKHR);
#endif

#ifdef VK_KHR_win32_surface
    LOAD_VULKAN_FN(vki, instance, vkCreateWin32SurfaceKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif

}

void vulkanLoaderDeviceInit(
    VULKAN_DEVICE* vkd,
    VkDevice device)
{
    LOAD_VULKAN_DEV_FN(vkd, device, vkAllocateCommandBuffers);
    LOAD_VULKAN_DEV_FN(vkd, device, vkAllocateDescriptorSets);
    LOAD_VULKAN_DEV_FN(vkd, device, vkAllocateMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkBeginCommandBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkBindBufferMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkBindImageMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBeginQuery);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBeginRendering);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBeginRenderPass);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindDescriptorSets);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindIndexBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindPipeline);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindVertexBuffers);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBlitImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdClearAttachments);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdClearColorImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdClearDepthStencilImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdCopyBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdCopyBufferToImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdCopyImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdCopyImageToBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdCopyQueryPoolResults);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDispatch);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDispatchIndirect);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDraw);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDrawIndexed);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDrawIndexedIndirect);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdDrawIndirect);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdEndQuery);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdEndRendering);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdEndRenderPass);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdExecuteCommands);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdFillBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdNextSubpass);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdPipelineBarrier);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdPushConstants);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdResetEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdResetQueryPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdResolveImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetBlendConstants);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthBias);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthBounds);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetLineWidth);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetScissor);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetStencilCompareMask);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetStencilReference);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetStencilWriteMask);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetViewport);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdUpdateBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdWaitEvents);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdWriteTimestamp);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateBufferView);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateCommandPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateComputePipelines);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateDescriptorPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateDescriptorSetLayout);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateDescriptorUpdateTemplate);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateFence);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateFramebuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateGraphicsPipelines);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateImageView);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreatePipelineCache);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreatePipelineLayout);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateQueryPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateRenderPass);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateSampler);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateSemaphore);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateShaderModule);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyBufferView);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyCommandPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyDescriptorPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyDescriptorSetLayout);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyDescriptorUpdateTemplate);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyDevice);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyFence);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyFramebuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyImage);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyImageView);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyPipeline);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyPipelineCache);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyPipelineLayout);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyQueryPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyRenderPass);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroySampler);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroySemaphore);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroyShaderModule);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDeviceWaitIdle);
    LOAD_VULKAN_DEV_FN(vkd, device, vkEndCommandBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkFlushMappedMemoryRanges);
    LOAD_VULKAN_DEV_FN(vkd, device, vkFreeCommandBuffers);
    LOAD_VULKAN_DEV_FN(vkd, device, vkFreeDescriptorSets);
    LOAD_VULKAN_DEV_FN(vkd, device, vkFreeMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetBufferMemoryRequirements);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetBufferMemoryRequirements2);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetBufferDeviceAddress);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDeviceMemoryCommitment);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDeviceQueue);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetEventStatus);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetFenceStatus);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetImageMemoryRequirements);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetImageMemoryRequirements2);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetImageSparseMemoryRequirements);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetImageSparseMemoryRequirements2);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetImageSubresourceLayout);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetPipelineCacheData);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetQueryPoolResults);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetRenderAreaGranularity);
    LOAD_VULKAN_DEV_FN(vkd, device, vkInvalidateMappedMemoryRanges);
    LOAD_VULKAN_DEV_FN(vkd, device, vkMapMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkMergePipelineCaches);
    LOAD_VULKAN_DEV_FN(vkd, device, vkQueueBindSparse);
    LOAD_VULKAN_DEV_FN(vkd, device, vkQueueSubmit);
    LOAD_VULKAN_DEV_FN(vkd, device, vkQueueWaitIdle);
    LOAD_VULKAN_DEV_FN(vkd, device, vkResetCommandBuffer);
    LOAD_VULKAN_DEV_FN(vkd, device, vkResetCommandPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkResetDescriptorPool);
    LOAD_VULKAN_DEV_FN(vkd, device, vkResetEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkResetFences);
    LOAD_VULKAN_DEV_FN(vkd, device, vkSetEvent);
    LOAD_VULKAN_DEV_FN(vkd, device, vkUnmapMemory);
    LOAD_VULKAN_DEV_FN(vkd, device, vkUpdateDescriptorSetWithTemplate);
    LOAD_VULKAN_DEV_FN(vkd, device, vkUpdateDescriptorSets);
    LOAD_VULKAN_DEV_FN(vkd, device, vkWaitForFences);

#ifdef VK_KHR_swapchain
    LOAD_VULKAN_DEV_FN(vkd, device, vkCreateSwapchainKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkDestroySwapchainKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetSwapchainImagesKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkAcquireNextImageKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkQueuePresentKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDeviceGroupPresentCapabilitiesKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDeviceGroupSurfacePresentModesKHR);
    LOAD_VULKAN_DEV_FN(vkd, device, vkAcquireNextImage2KHR);
#endif

#ifdef VK_EXT_extended_dynamic_state
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindVertexBuffers2EXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetCullModeEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthBoundsTestEnableEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthCompareOpEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthTestEnableEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDepthWriteEnableEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetFrontFaceEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetPrimitiveTopologyEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetScissorWithCountEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetStencilOpEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetStencilTestEnableEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetViewportWithCountEXT);
#endif

#ifdef VK_EXT_extended_dynamic_state3
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetColorBlendEnableEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetColorBlendEquationEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetPolygonModeEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetRasterizationSamplesEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetSampleMaskEXT);
#endif

#ifdef VK_KHR_push_descriptor
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdPushDescriptorSetKHR);
#endif

#ifdef VK_EXT_descriptor_buffer
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdBindDescriptorBuffersEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkCmdSetDescriptorBufferOffsetsEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDescriptorEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDescriptorSetLayoutBindingOffsetEXT);
    LOAD_VULKAN_DEV_FN(vkd, device, vkGetDescriptorSetLayoutSizeEXT);
#endif
}
