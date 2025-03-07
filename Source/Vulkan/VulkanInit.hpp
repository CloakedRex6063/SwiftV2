#pragma once
#include "SwiftInternal.hpp"
#define VK_NO_PROTOTYPES
#include "VkBootstrap.h"
#include "VulkanUtil.hpp"
#include "ranges"

#ifndef SWIFT_GLFW
#define SWIFT_GLFW
#endif

#ifdef SWIFT_GLFW
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#endif

namespace Swift::Vulkan
{
    vkb::Result<vkb::PhysicalDevice> CreateSelector(vkb::Instance instance,
                                                    VkSurfaceKHR surface,
                                                    vkb::PreferredDeviceType preferredDeviceType);

    vkb::Result<vkb::Instance> CreateInstance(const InitInfo &info);

    std::expected<Context, Error> CreateContext(const InitInfo &info);

    std::expected<Queue, Error> CreateQueue(const Context &context, vkb::QueueType queueType);

    std::expected<std::vector<Image>, Error> CreateSwapchainImages(const Context &context,
                                                                   VkSwapchainKHR swapchain,
                                                                   uint32_t imageCount);

    std::expected<Swapchain, Error> CreateSwapchain(const Context &context,
                                                    const Queue &queue,
                                                    const Int2 &dimensions);

    std::expected<void, Error> RecreateSwapchain(const Context &context,
                                                 const Queue &queue,
                                                 Swapchain &swapchain,
                                                 Int2 dimensions);

    std::expected<VkSemaphore, Error> CreateSemaphore(VkDevice device);

    std::expected<VkFence, Error> CreateFence(VkDevice device, bool signaled = false);

    std::expected<VkCommandPool, Error> CreateCommandPool(VkDevice device, uint32_t queueFamilyIndex);

    std::expected<VkCommandBuffer, Error> CreateCommandBuffer(VkDevice device, VkCommandPool commandPool);

    std::expected<Command, Error> CreateCommand(VkDevice device);

    std::expected<FrameData, Error> CreateFrameData(VkDevice device);

    std::expected<VkDescriptorSetLayout, Error> CreateDescriptorSetLayout(VkDevice device);

    std::expected<VkDescriptorPool, Error> CreateDescriptorPool(VkDevice device);

    std::expected<VkDescriptorSet, Error> CreateDescriptorSet(VkDevice device, VkDescriptorSetLayout setLayout,
                                                              VkDescriptorPool descriptorPool);

    std::expected<Descriptor, Error> CreateDescriptor(VkDevice device);

    std::expected<VkPipelineLayout, Error> CreatePipelineLayout(const Context &context,
                                                                VkDescriptorSetLayout descriptorSetLayout);

    std::expected<VkPipeline, Error> CreateGraphicsPipeline(VkDevice device,
                                                            VkPipelineLayout pipelineLayout,
                                                            const std::vector<VkPipelineShaderStageCreateInfo> &
                                                            shaderStages,
                                                            const GraphicsShaderCreateInfo &createInfo);

    std::expected<VkPipeline, Error> CreateComputePipeline(VkDevice device,
                                                           VkPipelineLayout pipelineLayout,
                                                           const VkPipelineShaderStageCreateInfo &shaderStage);

    std::expected<VkShaderModule, Error> CreateShaderModule(VkDevice device, const std::vector<char> &code);

    std::expected<ShaderInfo, Error> CreateShader(VkDevice device,
                                                  const std::vector<char> &code,
                                                  ShaderStage shaderStage);

    std::expected<VkSampler, Error> CreateSampler(const Context &context, const SamplerCreateInfo &createInfo);

    std::expected<std::tuple<VkImage, VmaAllocation>, Error> CreateBaseImage(const Swift::Context &context,
                                                                             const ImageCreateInfo &createInfo);

    std::expected<VkImageView, Error> CreateImageView(VkDevice device,
                                                      VkImage image,
                                                      const ImageCreateInfo &createInfo);

    std::expected<Swift::Image, Error> CreateImage(const Swift::Context &context,
                                                   const ImageCreateInfo &createInfo);

    void DestroyImage(const Swift::Context &context, const Image &image);

    std::expected<Swift::Buffer, Error> CreateBuffer(const Swift::Context &context, const BufferCreateInfo &createInfo);

    std::expected<void *, Error> MapBuffer(const Swift::Context &context, const Swift::Buffer &buffer);

#include "VulkanInit.inl"
} // namespace Swift::Vulkan
