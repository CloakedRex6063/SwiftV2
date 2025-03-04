#pragma once
#include "SwiftStructs.hpp"
#include "volk.h"

namespace Swift::Vulkan
{
    inline void
    BeginRendering(const Command& command,
                   const std::span<VkRenderingAttachmentInfo>& colorAttachments,
                   VkRenderingAttachmentInfo& depthAttachment,
                   const Int2& Dimensions)
    {
        const auto extent = VkExtent2D(Dimensions.x, Dimensions.y);
        const VkRect2D renderArea{
            .offset = VkOffset2D(),
            .extent = extent,
        };
        const VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = renderArea,
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount =
                static_cast<uint32_t>(colorAttachments.size()),
            .pColorAttachments = colorAttachments.data(),
            .pDepthAttachment = &depthAttachment,
        };
        vkCmdBeginRendering(command.Buffer, &renderingInfo);
    }
    inline void EndRendering(const Command& command)
    {
        vkCmdEndRendering(command.Buffer);
    }

    inline std::expected<uint32_t,
                         Error>
    AcquireNextImage(const Context& context,
                     const Swapchain& swapchain,
                     const VkSemaphore& semaphore)
    {
        const VkAcquireNextImageInfoKHR acquireInfo{
            .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
            .swapchain = swapchain.SwapChain,
            .timeout = std::numeric_limits<uint64_t>::max(),
            .semaphore = semaphore,
            .deviceMask = 1,
        };
        uint32_t imageIndex = 0;
        const auto result =
            vkAcquireNextImage2KHR(context.Device, &acquireInfo, &imageIndex);
        return CheckResult(result, imageIndex, Error::eAcquireFailed);
    }

    inline std::expected<void,
                         Error>
    Present(const Swapchain& swapchain,
            const Queue queue,
            VkSemaphore semaphore)
    {
        const VkSwapchainKHR defaultSwapchain = swapchain.SwapChain;
        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &semaphore,
            .swapchainCount = 1,
            .pSwapchains = &defaultSwapchain,
            .pImageIndices = &swapchain.CurrentImageIndex,
        };

        if (vkQueuePresentKHR(queue.BaseQueue, &presentInfo) != VK_SUCCESS)
        {
            return std::unexpected(Error::ePresentFailed);
        }
        return {};
    }

    inline std::expected<void,
                         Error>
    WaitFence(const VkDevice device,
              const VkFence fence)
    {
        const auto result =
            vkWaitForFences(device,
                            1,
                            &fence,
                            VK_TRUE,
                            std::numeric_limits<uint64_t>::max());
        if (result != VK_SUCCESS)
        {
            return std::unexpected(Error::eFailedToWaitFence);
        }
        return {};
    }

    inline std::expected<void,
                         Error>
    ResetFence(const VkDevice device,
               const VkFence fence)
    {
        if (vkResetFences(device, 1, &fence) != VK_SUCCESS)
        {
            return std::unexpected(Error::eFailedToWaitFence);
        }
        return {};
    }

    inline void BeginCommandBuffer(const Command& command)
    {
        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(command.Buffer, &beginInfo);
    }

    inline void EndCommandBuffer(const Command& command)
    {
        vkEndCommandBuffer(command.Buffer);
    }

    inline void SubmitQueue(const Queue queue,
                            const Command& command,
                            const SubmitInfo& submitInfo)
    {
        VkCommandBufferSubmitInfo commandSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = command.Buffer,
            .deviceMask = 1,
        };
        VkSemaphoreSubmitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitInfo.WaitSemaphore,
            .stageMask = submitInfo.WaitPipelineStage,
            .deviceIndex = 0,
        };
        VkSemaphoreSubmitInfo signalInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitInfo.SignalSemaphore,
            .stageMask = submitInfo.SignalPipelineStage,
            .deviceIndex = 0,
        };
        const VkSubmitInfo2 queueSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
        };
        vkQueueSubmit2(queue.BaseQueue, 1, &queueSubmitInfo, submitInfo.Fence);
    }

    inline Image& GetSwapchainImage(Swapchain& swapchain)
    {
        return swapchain.Images.at(swapchain.CurrentImageIndex);
    }

    inline void ClearImage(const Command& command,
                           const Image& image,
                           const Float4& color)
    {
        const auto clearColor =
            VkClearColorValue({color.x, color.y, color.z, color.w});
        const auto subresourceRange =
            Vulkan::GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(command.Buffer,
                             image.BaseImage,
                             VK_IMAGE_LAYOUT_GENERAL,
                             &clearColor,
                             1,
                             &subresourceRange);
    }

    inline std::vector<VkRenderingAttachmentInfo>
    CreateRenderAttachments(const std::span<Image>& images,
                            const std::vector<ImageHandle>& imageHandles,
                            const bool& bDepth = false)
    {

        const VkRenderingAttachmentInfo colorInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageLayout = bDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                  : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        std::vector colorAttachments(images.size(), colorInfo);
        for (const auto& [index, imageHandle] :
             std::views::enumerate(imageHandles))
        {
            if (imageHandle != InvalidHandle)
            {
                const auto& image = images[imageHandle];
                colorAttachments[index].imageView = image.ImageView;
            }
        }
        return colorAttachments;
    };
} // namespace Swift::Vulkan
