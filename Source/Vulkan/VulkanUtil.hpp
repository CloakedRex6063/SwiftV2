#pragma once
#include "span"
#include "VulkanConstants.hpp"
#include "array"
#include "volk.h"
#include "expected"
#include "iostream"

namespace Swift::Vulkan
{
    template<typename T>
    std::expected<T, Error> CheckResult(VkResult result, T expected, Error error);

    VkImageSubresourceRange GetImageSubresourceRange(VkImageAspectFlags aspectFlags,
                                                     uint32_t baseMip = 0,
                                                     uint32_t levelCount = 1,
                                                     uint32_t baseArrayLayer = 0,
                                                     uint32_t layerCount = 1);

    VkImageSubresourceLayers GetImageSubresourceLayers(VkImageAspectFlags aspectFlags,
                                                       uint32_t mipLevel = 0,
                                                       uint32_t baseArrayLayer = 0,
                                                       uint32_t layerCount = 1);

    void TransitionImage(const Command &command,
                         Image &image,
                         VkImageLayout newLayout,
                         VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    void BlitImage(const Command &command,
                   const Image &srcImage,
                   const Image &dstImage,
                   Int2 srcExtents,
                   Int2 dstExtents);

    void UpdateDescriptorSampler(VkDevice device, VkDescriptorSet descriptor,
                                 VkSampler sampler, VkImageView imageView,
                                 uint32_t arrayElement);

    void UpdateDescriptorImage(VkDevice device, VkDescriptorSet descriptor,
                               VkSampler sampler, VkImageView imageView, uint32_t arrayElement);

    void UpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, const void *data,
                      uint64_t offset, uint64_t size);

    void CopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer,
                    std::span<BufferCopy> copyRegions);

    void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage,
                           VkImageLayout dstLayout, std::span<VkBufferImageCopy2> copyRegions);

#include "VulkanUtil.inl"
} // namespace Swift::Vulkan
