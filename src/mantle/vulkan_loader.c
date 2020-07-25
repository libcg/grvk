#include "vulkan_loader.h"

#define LOAD_VULKAN_FN(obj, instance, name) \
    obj.name = (PFN_##name)vkGetInstanceProcAddr(instance, #name)

PFN_vkVoidFunction vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName);

VULKAN_LIBRARY vkl;
VULKAN_INSTANCE vki;
VkInstance vk;

void vulkanLoaderLibraryInit()
{
    LOAD_VULKAN_FN(vkl, NULL, vkCreateInstance);
    LOAD_VULKAN_FN(vkl, NULL, vkEnumerateInstanceExtensionProperties);
    LOAD_VULKAN_FN(vkl, NULL, vkEnumerateInstanceLayerProperties);
}

void vulkanLoaderInstanceInit(VkInstance instance)
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

    // Device functions
    // TODO use vkGetDeviceProcAddr
    LOAD_VULKAN_FN(vki, instance, vkAllocateCommandBuffers);
    LOAD_VULKAN_FN(vki, instance, vkAllocateDescriptorSets);
    LOAD_VULKAN_FN(vki, instance, vkAllocateMemory);
    LOAD_VULKAN_FN(vki, instance, vkBeginCommandBuffer);
    LOAD_VULKAN_FN(vki, instance, vkBindBufferMemory);
    LOAD_VULKAN_FN(vki, instance, vkBindImageMemory);
    LOAD_VULKAN_FN(vki, instance, vkCmdBeginQuery);
    LOAD_VULKAN_FN(vki, instance, vkCmdBeginRenderPass);
    LOAD_VULKAN_FN(vki, instance, vkCmdBindDescriptorSets);
    LOAD_VULKAN_FN(vki, instance, vkCmdBindIndexBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCmdBindPipeline);
    LOAD_VULKAN_FN(vki, instance, vkCmdBindVertexBuffers);
    LOAD_VULKAN_FN(vki, instance, vkCmdBlitImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdClearAttachments);
    LOAD_VULKAN_FN(vki, instance, vkCmdClearColorImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdClearDepthStencilImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdCopyBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCmdCopyBufferToImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdCopyImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdCopyImageToBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCmdCopyQueryPoolResults);
    LOAD_VULKAN_FN(vki, instance, vkCmdDispatch);
    LOAD_VULKAN_FN(vki, instance, vkCmdDispatchIndirect);
    LOAD_VULKAN_FN(vki, instance, vkCmdDraw);
    LOAD_VULKAN_FN(vki, instance, vkCmdDrawIndexed);
    LOAD_VULKAN_FN(vki, instance, vkCmdDrawIndexedIndirect);
    LOAD_VULKAN_FN(vki, instance, vkCmdDrawIndirect);
    LOAD_VULKAN_FN(vki, instance, vkCmdEndQuery);
    LOAD_VULKAN_FN(vki, instance, vkCmdEndRenderPass);
    LOAD_VULKAN_FN(vki, instance, vkCmdExecuteCommands);
    LOAD_VULKAN_FN(vki, instance, vkCmdFillBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCmdNextSubpass);
    LOAD_VULKAN_FN(vki, instance, vkCmdPipelineBarrier);
    LOAD_VULKAN_FN(vki, instance, vkCmdPushConstants);
    LOAD_VULKAN_FN(vki, instance, vkCmdResetEvent);
    LOAD_VULKAN_FN(vki, instance, vkCmdResetQueryPool);
    LOAD_VULKAN_FN(vki, instance, vkCmdResolveImage);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetBlendConstants);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthBias);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthBounds);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetEvent);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetLineWidth);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetScissor);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetStencilCompareMask);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetStencilReference);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetStencilWriteMask);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetViewport);
    LOAD_VULKAN_FN(vki, instance, vkCmdUpdateBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCmdWaitEvents);
    LOAD_VULKAN_FN(vki, instance, vkCmdWriteTimestamp);
    LOAD_VULKAN_FN(vki, instance, vkCreateBuffer);
    LOAD_VULKAN_FN(vki, instance, vkCreateBufferView);
    LOAD_VULKAN_FN(vki, instance, vkCreateCommandPool);
    LOAD_VULKAN_FN(vki, instance, vkCreateComputePipelines);
    LOAD_VULKAN_FN(vki, instance, vkCreateDescriptorPool);
    LOAD_VULKAN_FN(vki, instance, vkCreateDescriptorSetLayout);
    LOAD_VULKAN_FN(vki, instance, vkCreateDescriptorUpdateTemplate);
    LOAD_VULKAN_FN(vki, instance, vkCreateEvent);
    LOAD_VULKAN_FN(vki, instance, vkCreateFence);
    LOAD_VULKAN_FN(vki, instance, vkCreateFramebuffer);
    LOAD_VULKAN_FN(vki, instance, vkCreateGraphicsPipelines);
    LOAD_VULKAN_FN(vki, instance, vkCreateImage);
    LOAD_VULKAN_FN(vki, instance, vkCreateImageView);
    LOAD_VULKAN_FN(vki, instance, vkCreatePipelineCache);
    LOAD_VULKAN_FN(vki, instance, vkCreatePipelineLayout);
    LOAD_VULKAN_FN(vki, instance, vkCreateQueryPool);
    LOAD_VULKAN_FN(vki, instance, vkCreateRenderPass);
    LOAD_VULKAN_FN(vki, instance, vkCreateSampler);
    LOAD_VULKAN_FN(vki, instance, vkCreateSemaphore);
    LOAD_VULKAN_FN(vki, instance, vkCreateShaderModule);
    LOAD_VULKAN_FN(vki, instance, vkDestroyBuffer);
    LOAD_VULKAN_FN(vki, instance, vkDestroyBufferView);
    LOAD_VULKAN_FN(vki, instance, vkDestroyCommandPool);
    LOAD_VULKAN_FN(vki, instance, vkDestroyDescriptorPool);
    LOAD_VULKAN_FN(vki, instance, vkDestroyDescriptorSetLayout);
    LOAD_VULKAN_FN(vki, instance, vkDestroyDescriptorUpdateTemplate);
    LOAD_VULKAN_FN(vki, instance, vkDestroyDevice);
    LOAD_VULKAN_FN(vki, instance, vkDestroyEvent);
    LOAD_VULKAN_FN(vki, instance, vkDestroyFence);
    LOAD_VULKAN_FN(vki, instance, vkDestroyFramebuffer);
    LOAD_VULKAN_FN(vki, instance, vkDestroyImage);
    LOAD_VULKAN_FN(vki, instance, vkDestroyImageView);
    LOAD_VULKAN_FN(vki, instance, vkDestroyPipeline);
    LOAD_VULKAN_FN(vki, instance, vkDestroyPipelineCache);
    LOAD_VULKAN_FN(vki, instance, vkDestroyPipelineLayout);
    LOAD_VULKAN_FN(vki, instance, vkDestroyQueryPool);
    LOAD_VULKAN_FN(vki, instance, vkDestroyRenderPass);
    LOAD_VULKAN_FN(vki, instance, vkDestroySampler);
    LOAD_VULKAN_FN(vki, instance, vkDestroySemaphore);
    LOAD_VULKAN_FN(vki, instance, vkDestroyShaderModule);
    LOAD_VULKAN_FN(vki, instance, vkDeviceWaitIdle);
    LOAD_VULKAN_FN(vki, instance, vkEndCommandBuffer);
    LOAD_VULKAN_FN(vki, instance, vkFlushMappedMemoryRanges);
    LOAD_VULKAN_FN(vki, instance, vkFreeCommandBuffers);
    LOAD_VULKAN_FN(vki, instance, vkFreeDescriptorSets);
    LOAD_VULKAN_FN(vki, instance, vkFreeMemory);
    LOAD_VULKAN_FN(vki, instance, vkGetBufferMemoryRequirements);
    LOAD_VULKAN_FN(vki, instance, vkGetBufferMemoryRequirements2);
    LOAD_VULKAN_FN(vki, instance, vkGetDeviceMemoryCommitment);
    LOAD_VULKAN_FN(vki, instance, vkGetDeviceQueue);
    LOAD_VULKAN_FN(vki, instance, vkGetEventStatus);
    LOAD_VULKAN_FN(vki, instance, vkGetFenceStatus);
    LOAD_VULKAN_FN(vki, instance, vkGetImageMemoryRequirements);
    LOAD_VULKAN_FN(vki, instance, vkGetImageMemoryRequirements2);
    LOAD_VULKAN_FN(vki, instance, vkGetImageSparseMemoryRequirements);
    LOAD_VULKAN_FN(vki, instance, vkGetImageSparseMemoryRequirements2);
    LOAD_VULKAN_FN(vki, instance, vkGetImageSubresourceLayout);
    LOAD_VULKAN_FN(vki, instance, vkGetPipelineCacheData);
    LOAD_VULKAN_FN(vki, instance, vkGetQueryPoolResults);
    LOAD_VULKAN_FN(vki, instance, vkGetRenderAreaGranularity);
    LOAD_VULKAN_FN(vki, instance, vkInvalidateMappedMemoryRanges);
    LOAD_VULKAN_FN(vki, instance, vkMapMemory);
    LOAD_VULKAN_FN(vki, instance, vkMergePipelineCaches);
    LOAD_VULKAN_FN(vki, instance, vkQueueBindSparse);
    LOAD_VULKAN_FN(vki, instance, vkQueueSubmit);
    LOAD_VULKAN_FN(vki, instance, vkQueueWaitIdle);
    LOAD_VULKAN_FN(vki, instance, vkResetCommandBuffer);
    LOAD_VULKAN_FN(vki, instance, vkResetCommandPool);
    LOAD_VULKAN_FN(vki, instance, vkResetDescriptorPool);
    LOAD_VULKAN_FN(vki, instance, vkResetEvent);
    LOAD_VULKAN_FN(vki, instance, vkResetFences);
    LOAD_VULKAN_FN(vki, instance, vkSetEvent);
    LOAD_VULKAN_FN(vki, instance, vkUnmapMemory);
    LOAD_VULKAN_FN(vki, instance, vkUpdateDescriptorSetWithTemplate);
    LOAD_VULKAN_FN(vki, instance, vkUpdateDescriptorSets);
    LOAD_VULKAN_FN(vki, instance, vkWaitForFences);

#ifdef VK_EXT_extended_dynamic_state
    LOAD_VULKAN_FN(vki, instance, vkCmdBindVertexBuffers2EXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetCullModeEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthBoundsTestEnableEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthCompareOpEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthTestEnableEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetDepthWriteEnableEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetFrontFaceEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetPrimitiveTopologyEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetScissorWithCountEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetStencilOpEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetStencilTestEnableEXT);
    LOAD_VULKAN_FN(vki, instance, vkCmdSetViewportWithCountEXT);
#endif

#ifdef VK_KHR_surface
    LOAD_VULKAN_FN(vki, instance, vkDestroySurfaceKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
#endif

#ifdef VK_KHR_swapchain
    LOAD_VULKAN_FN(vki, instance, vkCreateSwapchainKHR);
    LOAD_VULKAN_FN(vki, instance, vkDestroySwapchainKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetSwapchainImagesKHR);
    LOAD_VULKAN_FN(vki, instance, vkAcquireNextImageKHR);
    LOAD_VULKAN_FN(vki, instance, vkQueuePresentKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetDeviceGroupPresentCapabilitiesKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetDeviceGroupSurfacePresentModesKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDevicePresentRectanglesKHR);
    LOAD_VULKAN_FN(vki, instance, vkAcquireNextImage2KHR);
#endif

#ifdef VK_KHR_win32_surface
    LOAD_VULKAN_FN(vki, instance, vkCreateWin32SurfaceKHR);
    LOAD_VULKAN_FN(vki, instance, vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif
}
