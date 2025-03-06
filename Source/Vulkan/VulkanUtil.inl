#pragma once

template<typename T>
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

    inline void TransitionImage(const Command &command,
                                Image &image,
                                const VkImageLayout &oldLayout,
                                const VkImageLayout &newLayout,
                                const VkImageAspectFlags aspectMask)
    {
        VkImageMemoryBarrier2 imageBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2
        };
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask =
                VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = oldLayout;
        imageBarrier.newLayout = newLayout;

        imageBarrier.subresourceRange = GetImageSubresourceRange(aspectMask);
        imageBarrier.image = image.BaseImage;

        const VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier,
        };
        vkCmdPipelineBarrier2(command.Buffer, &dependencyInfo);
        image.CurrentLayout = imageBarrier.newLayout;
    }

    inline void BlitImage(const Command &command,
                          const Image &srcImage,
                          const Image &dstImage,
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
            .srcOffsets[0] = srcOffsets[0],
            .srcOffsets[1] = srcOffsets[1],
            .dstSubresource =
            Vulkan::GetImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT),
            .dstOffsets[0] = dstOffsets[0],
            .dstOffsets[1] = dstOffsets[1],
        };
        const VkBlitImageInfo2 blitImageInfo{
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = srcImage.BaseImage,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = dstImage.BaseImage,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR
        };
        vkCmdBlitImage2(command.Buffer, &blitImageInfo);
    }

    inline void UpdateDescriptorSampler(const VkDevice device, const VkDescriptorSet descriptor,
                                        const VkSampler sampler, const VkImageView imageView,
                                        const uint32_t arrayElement)
    {
        VkDescriptorImageInfo imageInfo{
            .imageView = imageView,
            .sampler = sampler,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor,
            .dstBinding = Constants::SamplerBinding,
            .dstArrayElement = arrayElement,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    inline void UpdateDescriptorImage(const VkDevice device, const VkDescriptorSet descriptor,
                                      const VkSampler sampler, const VkImageView imageView, const uint32_t arrayElement)
    {
        VkDescriptorImageInfo imageInfo{
            .imageView = imageView,
            .sampler = sampler,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
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

    inline std::expected<void, Error> CopyToBuffer(const VmaAllocator allocator, const VmaAllocation bufferAlloc,
                                                   const void *data,
                                                   const uint64_t offset, const uint64_t size)
    {
        if (vmaCopyMemoryToAllocation(allocator, data, bufferAlloc, offset, size) != VK_SUCCESS)
        {
            return std::unexpected(Error::eCopyFailed);
        }
        return {};
    }

    inline void UpdateBuffer(const VkCommandBuffer commandBuffer, const VkBuffer buffer, const void *data,
                             const uint64_t offset, const uint64_t size)
    {
        vkCmdUpdateBuffer(commandBuffer, buffer, offset, size, data);
    }