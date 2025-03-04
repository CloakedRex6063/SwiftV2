#pragma once
#include "array"
#include "volk.h"
#include "expected"

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
                             const uint32_t baseMip = 0,
                             const uint32_t levelCount = 1,
                             const uint32_t baseArrayLayer = 0,
                             const uint32_t layerCount = 1)
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
                              const uint32_t mipLevel = 0,
                              const uint32_t baseArrayLayer = 0,
                              const uint32_t layerCount = 1)
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
                                const VkImageLayout& oldLayout,
                                const VkImageLayout& newLayout,
                                const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        VkImageMemoryBarrier2 imageBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
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
            .filter = VK_FILTER_LINEAR};
        vkCmdBlitImage2(command.Buffer, &blitImageInfo);
    }
} // namespace Swift::Vulkan
