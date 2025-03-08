#pragma once

namespace Swift::Vulkan
{
    template <typename T>
    std::expected<T,
                  Error>
    CheckResult(const VkResult result,
                T expected,
                Error error)
    {
        if (result != VK_SUCCESS)
        {
            return std::unexpected(error);
        }
        return expected;
    }

    inline VkImageSubresourceRange
    GetImageSubresourceRange(const VkImageAspectFlags aspectFlags,
                             const uint32_t baseMip,
                             const uint32_t levelCount,
                             const uint32_t baseArrayLayer,
                             const uint32_t layerCount)
    {
        const VkImageSubresourceRange range{
            .aspectMask = aspectFlags,
            .baseMipLevel = baseMip,
            .levelCount = levelCount,
            .baseArrayLayer = baseArrayLayer,
            .layerCount = layerCount,
        };
        return range;
    }

    inline VkImageSubresourceLayers
    GetImageSubresourceLayers(const VkImageAspectFlags aspectFlags,
                              const uint32_t mipLevel,
                              const uint32_t baseArrayLayer,
                              const uint32_t layerCount)
    {
        const VkImageSubresourceLayers range{
            .aspectMask = aspectFlags,
            .mipLevel = mipLevel,
            .baseArrayLayer = baseArrayLayer,
            .layerCount = layerCount,
        };
        return range;
    }

    inline void TransitionImage(const Command& command,
                                Image& image,
                                const VkImageLayout newLayout,
                                const VkImageAspectFlags aspectMask)
    {
        VkImageMemoryBarrier2 imageBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask =
            VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = image.CurrentLayout;
        imageBarrier.newLayout = newLayout;

        imageBarrier.subresourceRange =
            GetImageSubresourceRange(aspectMask,
                                     0,
                                     image.MipLevels,
                                     0,
                                     image.ArrayLayers);
        imageBarrier.image = image.BaseImage;

        const VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier,
        };
        vkCmdPipelineBarrier2(command.Buffer, &dependencyInfo);
        image.CurrentLayout = imageBarrier.newLayout;
    }

    inline void BlitImage(const Command& command,
                          const Image& srcImage,
                          const Image& dstImage,
                          const Int2 srcExtents,
                          const Int2 dstExtents)
    {
        const std::array srcOffsets{
            VkOffset3D{},
            VkOffset3D{srcExtents.x, srcExtents.y, 1},
        };
        const std::array dstOffsets{
            VkOffset3D{},
            VkOffset3D{dstExtents.x, dstExtents.y, 1},
        };

        VkImageBlit2 blit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource =
                Vulkan::GetImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT),
            .dstSubresource =
                Vulkan::GetImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT),
        };
        blit.srcOffsets[0] = srcOffsets[0];
        blit.srcOffsets[1] = srcOffsets[1];
        blit.dstOffsets[0] = dstOffsets[0];
        blit.dstOffsets[1] = dstOffsets[1];

        const VkBlitImageInfo2 blitImageInfo{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = srcImage.BaseImage,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = dstImage.BaseImage,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR};
        vkCmdBlitImage2(command.Buffer, &blitImageInfo);
    }

    inline void UpdateDescriptorSampler(const VkDevice device,
                                        const VkDescriptorSet descriptor,
                                        const VkSampler sampler,
                                        const VkImageView imageView,
                                        const uint32_t arrayElement)
    {
        VkDescriptorImageInfo imageInfo{
            .sampler = sampler,
            .imageView = imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor,
            .dstBinding = Vulkan::Constants::SamplerBinding,
            .dstArrayElement = arrayElement,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    inline void UpdateDescriptorImage(const VkDevice device,
                                      const VkDescriptorSet descriptor,
                                      const VkSampler sampler,
                                      const VkImageView imageView,
                                      const uint32_t arrayElement)
    {
        VkDescriptorImageInfo imageInfo{.sampler = sampler,
                                        .imageView = imageView,
                                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
        const VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor,
            .dstBinding = Vulkan::Constants::ImageBinding,
            .dstArrayElement = arrayElement,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    inline void UpdateBuffer(const VkCommandBuffer commandBuffer,
                             const VkBuffer buffer,
                             const void* data,
                             const uint64_t offset,
                             const uint64_t size)
    {
        vkCmdUpdateBuffer(commandBuffer, buffer, offset, size, data);
    }

    inline void CopyBuffer(const VkCommandBuffer commandBuffer,
                           const VkBuffer srcBuffer,
                           const VkBuffer dstBuffer,
                           const std::span<const BufferCopy> copyRegions)
    {
        std::vector<VkBufferCopy2> vkCopyRegions;
        vkCopyRegions.reserve(copyRegions.size());

        for (const auto& copyRegion : copyRegions)
        {
            vkCopyRegions.emplace_back(VkBufferCopy2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .srcOffset = copyRegion.SrcOffset,
                .dstOffset = copyRegion.DstOffset,
                .size = copyRegion.Size,
            });
        }

        VkCopyBufferInfo2 copyBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer = srcBuffer,
            .dstBuffer = dstBuffer,
            .regionCount = static_cast<uint32_t>(copyRegions.size()),
            .pRegions = vkCopyRegions.data()};
        vkCmdCopyBuffer2(commandBuffer, &copyBufferInfo);
    }

    inline void
    CopyBufferToImage(const VkCommandBuffer commandBuffer,
                      const VkBuffer srcBuffer,
                      const VkImage dstImage,
                      const VkImageLayout dstLayout,
                      const std::span<VkBufferImageCopy2> copyRegions)
    {
        const VkCopyBufferToImageInfo2 copyImageInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .srcBuffer = srcBuffer,
            .dstImage = dstImage,
            .dstImageLayout = dstLayout,
            .regionCount = static_cast<uint32_t>(copyRegions.size()),
            .pRegions = copyRegions.data(),
        };
        vkCmdCopyBufferToImage2(commandBuffer, &copyImageInfo);
    }
} // namespace Swift::Vulkan
