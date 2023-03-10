/*
* Copyright 2016 Sascha Willems - www.saschawillems.de
* SPDX-License-Identifier: MIT
*
* Assorted commonly used Vulkan helper functions
*/

#pragma once

#include "vulkan/vulkan.h"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__ANDROID__)
#include "vulkanandroid.h"
#include <android/asset_manager.h>
#endif

#include <SDL2/SDL_messagebox.h>

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#if 0
// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)                                                                              \
{                                                                                                       \
    VkResult res = (f);                                                                                 \
    if (res != VK_SUCCESS)                                                                              \
    {                                                                                                   \
        std::cout << "Fatal : VkResult is \"" << vkTools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
        assert(res == VK_SUCCESS);                                                                      \
    }                                                                                                   \
}
#endif
#if defined(DEBUG)
#include <sstream>
extern const char* appName();
#define VK_CHECK_RESULT(f)                                                    \
{                                                                             \
    VkResult res = (f);                                                       \
    if (res != VK_SUCCESS)                                                    \
    {                                                                         \
        std::stringstream msg;                                                \
        msg << "Fatal error. VkResult is \""                                  \
            << vkTools::errorString(res) << "\" in " << __FILE__              \
            << " at line " << __LINE__ << std::endl;                          \
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,                        \
                                 appName(),                                      \
                                 msg.str().c_str(),                           \
                                 NULL);                                       \
        assert(res == VK_SUCCESS);                                            \
    }                                                                         \
}
#else
#define VK_CHECK_RESULT(f) (void)f
#endif


namespace vkTools
{
    // Check if extension is globally available
    VkBool32 checkGlobalExtensionPresent(const char* extensionName);
    // Check if extension is present on the given device
    VkBool32 checkDeviceExtensionPresent(VkPhysicalDevice physicalDevice, const char* extensionName);
    // Return string representation of a vulkan error string
    std::string errorString(VkResult errorCode);

    // Selected a suitable supported depth format starting with 32 bit down to 16 bit
    // Returns false if none of the depth formats in the list is supported by the device
    VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);

    // Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    void setImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageAspectFlags aspectMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange);
    // Uses a fixed sub resource layout with first mip level and layer
    void setImageLayout(
        VkCommandBuffer cmdbuffer, 
        VkImage image, 
        VkImageAspectFlags aspectMask, 
        VkImageLayout oldImageLayout, 
        VkImageLayout newImageLayout);

    // Display error message and exit on fatal error
    void exitFatal(std::string message, std::string caption);
    // Load a text file (e.g. GLGL shader) into a std::string
    std::string readTextFile(const char *fileName);
    // Load a binary file into a buffer (e.g. SPIR-V)
    char *readBinaryFile(const char *filename, size_t *psize);

    // Load a SPIR-V shader
#if defined(__ANDROID__)
    VkShaderModule loadShader(AAssetManager* assetManager, const char *fileName, VkDevice device, VkShaderStageFlagBits stage);
#else
    VkShaderModule loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);
#endif

    // Load a GLSL shader
    // Note : Only for testing purposes, support for directly feeding GLSL shaders into Vulkan
    // may be dropped at some point 
    VkShaderModule loadShaderGLSL(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);

    // Returns a pre-present image memory barrier
    // Transforms the image's layout from color attachment to present khr
    VkImageMemoryBarrier prePresentBarrier(VkImage presentImage);

    // Returns a post-present image memory barrier
    // Transforms the image's layout back from present khr to color attachment
    VkImageMemoryBarrier postPresentBarrier(VkImage presentImage);

    // Contains all vulkan objects
    // required for a uniform data object
    struct UniformData 
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDescriptorBufferInfo descriptor;
        uint32_t allocSize;
        void* mapped = nullptr;
    };

    // Destroy (and free) Vulkan resources used by a uniform data structure
    void destroyUniformData(VkDevice device, vkTools::UniformData *uniformData);

    // Contains often used vulkan object initializers
    // Save lot of VK_STRUCTURE_TYPE assignments
    // Some initializers are parameterized for convenience
    namespace initializers
    {
        VkMemoryAllocateInfo memoryAllocateInfo();

        VkCommandBufferAllocateInfo commandBufferAllocateInfo(
            VkCommandPool commandPool,
            VkCommandBufferLevel level,
            uint32_t bufferCount);

        VkCommandPoolCreateInfo commandPoolCreateInfo();
        VkCommandBufferBeginInfo commandBufferBeginInfo();
        VkCommandBufferInheritanceInfo commandBufferInheritanceInfo();

        VkRenderPassBeginInfo renderPassBeginInfo();
        VkRenderPassCreateInfo renderPassCreateInfo();

        VkImageMemoryBarrier imageMemoryBarrier();
        VkBufferMemoryBarrier bufferMemoryBarrier();
        VkMemoryBarrier memoryBarrier();

        VkImageCreateInfo imageCreateInfo();
        VkSamplerCreateInfo samplerCreateInfo();
        VkImageViewCreateInfo imageViewCreateInfo();

        VkFramebufferCreateInfo framebufferCreateInfo();

        VkSemaphoreCreateInfo semaphoreCreateInfo();
        VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags);
        VkEventCreateInfo eventCreateInfo();

        VkSubmitInfo submitInfo();

        VkViewport viewport(
            float width, 
            float height, 
            float minDepth, 
            float maxDepth);

        VkRect2D rect2D(
            int32_t width,
            int32_t height,
            int32_t offsetX,
            int32_t offsetY);

        VkBufferCreateInfo bufferCreateInfo();

        VkBufferCreateInfo bufferCreateInfo(
            VkBufferUsageFlags usage, 
            VkDeviceSize size);

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo(
            uint32_t poolSizeCount,
            VkDescriptorPoolSize* pPoolSizes,
            uint32_t maxSets);

        VkDescriptorPoolSize descriptorPoolSize(
            VkDescriptorType type,
            uint32_t descriptorCount);

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(
            VkDescriptorType type, 
            VkShaderStageFlags stageFlags, 
            uint32_t binding);

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
            const VkDescriptorSetLayoutBinding* pBindings,
            uint32_t bindingCount);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
            const VkDescriptorSetLayout* pSetLayouts,
            uint32_t setLayoutCount );

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo(
            VkDescriptorPool descriptorPool,
            const VkDescriptorSetLayout* pSetLayouts,
            uint32_t descriptorSetCount);

        VkDescriptorImageInfo descriptorImageInfo(
            VkSampler sampler,
            VkImageView imageView,
            VkImageLayout imageLayout);

        VkWriteDescriptorSet writeDescriptorSet(
            VkDescriptorSet dstSet, 
            VkDescriptorType type, 
            uint32_t binding, 
            VkDescriptorBufferInfo* bufferInfo);

        VkWriteDescriptorSet writeDescriptorSet(
            VkDescriptorSet dstSet, 
            VkDescriptorType type, 
            uint32_t binding, 
            VkDescriptorImageInfo* imageInfo);

        VkVertexInputBindingDescription vertexInputBindingDescription(
            uint32_t binding, 
            uint32_t stride, 
            VkVertexInputRate inputRate);

        VkVertexInputAttributeDescription vertexInputAttributeDescription(
            uint32_t binding,
            uint32_t location,
            VkFormat format,
            uint32_t offset);

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo();

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
            VkPrimitiveTopology topology,
            VkPipelineInputAssemblyStateCreateFlags flags,
            VkBool32 primitiveRestartEnable);

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
            VkPolygonMode polygonMode,
            VkCullModeFlags cullMode,
            VkFrontFace frontFace,
            VkPipelineRasterizationStateCreateFlags flags);

        VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
            VkColorComponentFlags colorWriteMask,
            VkBool32 blendEnable);

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
            uint32_t attachmentCount,
            const VkPipelineColorBlendAttachmentState* pAttachments);

        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
            VkBool32 depthTestEnable,
            VkBool32 depthWriteEnable,
            VkCompareOp depthCompareOp);

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
            uint32_t viewportCount,
            uint32_t scissorCount,
            VkPipelineViewportStateCreateFlags flags);

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
            VkSampleCountFlagBits rasterizationSamples,
            VkPipelineMultisampleStateCreateFlags flags);

        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
            const VkDynamicState *pDynamicStates,
            uint32_t dynamicStateCount,
            VkPipelineDynamicStateCreateFlags flags);

        VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(
            uint32_t patchControlPoints);

        VkGraphicsPipelineCreateInfo pipelineCreateInfo(
            VkPipelineLayout layout,
            VkRenderPass renderPass,
            VkPipelineCreateFlags flags);

        VkComputePipelineCreateInfo computePipelineCreateInfo(
            VkPipelineLayout layout,
            VkPipelineCreateFlags flags);

        VkPushConstantRange pushConstantRange(
            VkShaderStageFlags stageFlags,
            uint32_t size,
            uint32_t offset);
    }

}

// vi: set sw=2 ts=4 expandtab:
