#ifndef VULKAN_LOADER_H_
#define VULKAN_LOADER_H_

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"

#define VULKAN_FN(name) \
    PFN_##name name

typedef struct _VULKAN_LIBRARY {
    VULKAN_FN(vkCreateInstance);
    VULKAN_FN(vkEnumerateInstanceExtensionProperties);
    VULKAN_FN(vkEnumerateInstanceLayerProperties);
} VULKAN_LIBRARY;

typedef struct _VULKAN_INSTANCE {
    VULKAN_FN(vkCreateDevice);
    VULKAN_FN(vkDestroyInstance);
    VULKAN_FN(vkEnumerateDeviceExtensionProperties);
    VULKAN_FN(vkEnumeratePhysicalDevices);
    VULKAN_FN(vkGetPhysicalDeviceFeatures);
    VULKAN_FN(vkGetPhysicalDeviceFeatures2);
    VULKAN_FN(vkGetPhysicalDeviceFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceFormatProperties2);
    VULKAN_FN(vkGetPhysicalDeviceImageFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceImageFormatProperties2);
    VULKAN_FN(vkGetPhysicalDeviceMemoryProperties);
    VULKAN_FN(vkGetPhysicalDeviceMemoryProperties2);
    VULKAN_FN(vkGetPhysicalDeviceProperties);
    VULKAN_FN(vkGetPhysicalDeviceProperties2);
    VULKAN_FN(vkGetPhysicalDeviceQueueFamilyProperties);
    VULKAN_FN(vkGetPhysicalDeviceQueueFamilyProperties2);
    VULKAN_FN(vkGetPhysicalDeviceSparseImageFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceSparseImageFormatProperties2);

#ifdef VK_KHR_surface
    VULKAN_FN(vkDestroySurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceSupportKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceFormatsKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfacePresentModesKHR);
#endif

#ifdef VK_KHR_swapchain
    VULKAN_FN(vkGetPhysicalDevicePresentRectanglesKHR);
#endif

#ifdef VK_KHR_win32_surface
    VULKAN_FN(vkCreateWin32SurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif
} VULKAN_INSTANCE;

typedef struct _VULKAN_DEVICE {
    VULKAN_FN(vkAllocateCommandBuffers);
    VULKAN_FN(vkAllocateDescriptorSets);
    VULKAN_FN(vkAllocateMemory);
    VULKAN_FN(vkBeginCommandBuffer);
    VULKAN_FN(vkBindBufferMemory);
    VULKAN_FN(vkBindImageMemory);
    VULKAN_FN(vkCmdBeginQuery);
    VULKAN_FN(vkCmdBeginRenderPass);
    VULKAN_FN(vkCmdBindDescriptorSets);
    VULKAN_FN(vkCmdBindIndexBuffer);
    VULKAN_FN(vkCmdBindPipeline);
    VULKAN_FN(vkCmdBindVertexBuffers);
    VULKAN_FN(vkCmdBlitImage);
    VULKAN_FN(vkCmdClearAttachments);
    VULKAN_FN(vkCmdClearColorImage);
    VULKAN_FN(vkCmdClearDepthStencilImage);
    VULKAN_FN(vkCmdCopyBuffer);
    VULKAN_FN(vkCmdCopyBufferToImage);
    VULKAN_FN(vkCmdCopyImage);
    VULKAN_FN(vkCmdCopyImageToBuffer);
    VULKAN_FN(vkCmdCopyQueryPoolResults);
    VULKAN_FN(vkCmdDispatch);
    VULKAN_FN(vkCmdDispatchIndirect);
    VULKAN_FN(vkCmdDraw);
    VULKAN_FN(vkCmdDrawIndexed);
    VULKAN_FN(vkCmdDrawIndexedIndirect);
    VULKAN_FN(vkCmdDrawIndirect);
    VULKAN_FN(vkCmdEndQuery);
    VULKAN_FN(vkCmdEndRenderPass);
    VULKAN_FN(vkCmdExecuteCommands);
    VULKAN_FN(vkCmdFillBuffer);
    VULKAN_FN(vkCmdNextSubpass);
    VULKAN_FN(vkCmdPipelineBarrier);
    VULKAN_FN(vkCmdPushConstants);
    VULKAN_FN(vkCmdResetEvent);
    VULKAN_FN(vkCmdResetQueryPool);
    VULKAN_FN(vkCmdResolveImage);
    VULKAN_FN(vkCmdSetBlendConstants);
    VULKAN_FN(vkCmdSetDepthBias);
    VULKAN_FN(vkCmdSetDepthBounds);
    VULKAN_FN(vkCmdSetEvent);
    VULKAN_FN(vkCmdSetLineWidth);
    VULKAN_FN(vkCmdSetScissor);
    VULKAN_FN(vkCmdSetStencilCompareMask);
    VULKAN_FN(vkCmdSetStencilReference);
    VULKAN_FN(vkCmdSetStencilWriteMask);
    VULKAN_FN(vkCmdSetViewport);
    VULKAN_FN(vkCmdUpdateBuffer);
    VULKAN_FN(vkCmdWaitEvents);
    VULKAN_FN(vkCmdWriteTimestamp);
    VULKAN_FN(vkCreateBuffer);
    VULKAN_FN(vkCreateBufferView);
    VULKAN_FN(vkCreateCommandPool);
    VULKAN_FN(vkCreateComputePipelines);
    VULKAN_FN(vkCreateDescriptorPool);
    VULKAN_FN(vkCreateDescriptorSetLayout);
    VULKAN_FN(vkCreateDescriptorUpdateTemplate);
    VULKAN_FN(vkCreateEvent);
    VULKAN_FN(vkCreateFence);
    VULKAN_FN(vkCreateFramebuffer);
    VULKAN_FN(vkCreateGraphicsPipelines);
    VULKAN_FN(vkCreateImage);
    VULKAN_FN(vkCreateImageView);
    VULKAN_FN(vkCreatePipelineCache);
    VULKAN_FN(vkCreatePipelineLayout);
    VULKAN_FN(vkCreateQueryPool);
    VULKAN_FN(vkCreateRenderPass);
    VULKAN_FN(vkCreateSampler);
    VULKAN_FN(vkCreateSemaphore);
    VULKAN_FN(vkCreateShaderModule);
    VULKAN_FN(vkDestroyBuffer);
    VULKAN_FN(vkDestroyBufferView);
    VULKAN_FN(vkDestroyCommandPool);
    VULKAN_FN(vkDestroyDescriptorPool);
    VULKAN_FN(vkDestroyDescriptorSetLayout);
    VULKAN_FN(vkDestroyDescriptorUpdateTemplate);
    VULKAN_FN(vkDestroyDevice);
    VULKAN_FN(vkDestroyEvent);
    VULKAN_FN(vkDestroyFence);
    VULKAN_FN(vkDestroyFramebuffer);
    VULKAN_FN(vkDestroyImage);
    VULKAN_FN(vkDestroyImageView);
    VULKAN_FN(vkDestroyPipeline);
    VULKAN_FN(vkDestroyPipelineCache);
    VULKAN_FN(vkDestroyPipelineLayout);
    VULKAN_FN(vkDestroyQueryPool);
    VULKAN_FN(vkDestroyRenderPass);
    VULKAN_FN(vkDestroySampler);
    VULKAN_FN(vkDestroySemaphore);
    VULKAN_FN(vkDestroyShaderModule);
    VULKAN_FN(vkDeviceWaitIdle);
    VULKAN_FN(vkEndCommandBuffer);
    VULKAN_FN(vkFlushMappedMemoryRanges);
    VULKAN_FN(vkFreeCommandBuffers);
    VULKAN_FN(vkFreeDescriptorSets);
    VULKAN_FN(vkFreeMemory);
    VULKAN_FN(vkGetBufferMemoryRequirements);
    VULKAN_FN(vkGetBufferMemoryRequirements2);
    VULKAN_FN(vkGetBufferDeviceAddress);
    VULKAN_FN(vkGetDeviceMemoryCommitment);
    VULKAN_FN(vkGetDeviceQueue);
    VULKAN_FN(vkGetEventStatus);
    VULKAN_FN(vkGetFenceStatus);
    VULKAN_FN(vkGetImageMemoryRequirements);
    VULKAN_FN(vkGetImageMemoryRequirements2);
    VULKAN_FN(vkGetImageSparseMemoryRequirements);
    VULKAN_FN(vkGetImageSparseMemoryRequirements2);
    VULKAN_FN(vkGetImageSubresourceLayout);
    VULKAN_FN(vkGetPipelineCacheData);
    VULKAN_FN(vkGetQueryPoolResults);
    VULKAN_FN(vkGetRenderAreaGranularity);
    VULKAN_FN(vkInvalidateMappedMemoryRanges);
    VULKAN_FN(vkMapMemory);
    VULKAN_FN(vkMergePipelineCaches);
    VULKAN_FN(vkQueueBindSparse);
    VULKAN_FN(vkQueueSubmit);
    VULKAN_FN(vkQueueWaitIdle);
    VULKAN_FN(vkResetCommandBuffer);
    VULKAN_FN(vkResetCommandPool);
    VULKAN_FN(vkResetDescriptorPool);
    VULKAN_FN(vkResetEvent);
    VULKAN_FN(vkResetFences);
    VULKAN_FN(vkSetEvent);
    VULKAN_FN(vkUnmapMemory);
    VULKAN_FN(vkUpdateDescriptorSets);
    VULKAN_FN(vkUpdateDescriptorSetWithTemplate);
    VULKAN_FN(vkWaitForFences);

#ifdef VK_KHR_dynamic_rendering
    VULKAN_FN(vkCmdBeginRenderingKHR);
    VULKAN_FN(vkCmdEndRenderingKHR);
#endif

#ifdef VK_KHR_swapchain
    VULKAN_FN(vkCreateSwapchainKHR);
    VULKAN_FN(vkDestroySwapchainKHR);
    VULKAN_FN(vkGetSwapchainImagesKHR);
    VULKAN_FN(vkAcquireNextImageKHR);
    VULKAN_FN(vkQueuePresentKHR);
    VULKAN_FN(vkGetDeviceGroupPresentCapabilitiesKHR);
    VULKAN_FN(vkGetDeviceGroupSurfacePresentModesKHR);
    VULKAN_FN(vkAcquireNextImage2KHR);
#endif

#ifdef VK_EXT_extended_dynamic_state
    VULKAN_FN(vkCmdBindVertexBuffers2EXT);
    VULKAN_FN(vkCmdSetCullModeEXT);
    VULKAN_FN(vkCmdSetDepthBoundsTestEnableEXT);
    VULKAN_FN(vkCmdSetDepthCompareOpEXT);
    VULKAN_FN(vkCmdSetDepthTestEnableEXT);
    VULKAN_FN(vkCmdSetDepthWriteEnableEXT);
    VULKAN_FN(vkCmdSetFrontFaceEXT);
    VULKAN_FN(vkCmdSetPrimitiveTopologyEXT);
    VULKAN_FN(vkCmdSetScissorWithCountEXT);
    VULKAN_FN(vkCmdSetStencilOpEXT);
    VULKAN_FN(vkCmdSetStencilTestEnableEXT);
    VULKAN_FN(vkCmdSetViewportWithCountEXT);
#endif

#ifdef VK_KHR_push_descriptor
    VULKAN_FN(vkCmdPushDescriptorSetKHR);
#endif

#ifdef VK_EXT_descriptor_buffer
    VULKAN_FN(vkCmdBindDescriptorBuffersEXT);
    VULKAN_FN(vkCmdSetDescriptorBufferOffsetsEXT);
    VULKAN_FN(vkGetDescriptorEXT);
    VULKAN_FN(vkGetDescriptorSetLayoutBindingOffsetEXT);
    VULKAN_FN(vkGetDescriptorSetLayoutSizeEXT);
#endif

#ifdef VK_EXT_extended_dynamic_state3
    VULKAN_FN(vkCmdSetPolygonModeEXT);
    VULKAN_FN(vkCmdSetColorBlendEnableEXT);
    VULKAN_FN(vkCmdSetColorBlendEquationEXT);
    VULKAN_FN(vkCmdSetSampleMaskEXT);
    VULKAN_FN(vkCmdSetRasterizationSamplesEXT);
#endif
} VULKAN_DEVICE;

extern VULKAN_LIBRARY vkl;
extern VULKAN_INSTANCE vki;
extern VkInstance vk;

void vulkanLoaderLibraryInit(
    VULKAN_LIBRARY* vkl);

void vulkanLoaderInstanceInit(
    VULKAN_INSTANCE* vki,
    VkInstance instance);

void vulkanLoaderDeviceInit(
    VULKAN_DEVICE* vkd,
    VkDevice device);

#endif // VULKAN_EXT_H_
