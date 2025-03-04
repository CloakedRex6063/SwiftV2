#pragma once
#include "SwiftStructs.hpp"

namespace Swift
{
    inline bool operator==(const Int2& lhs,
                    const Int2& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }

    inline bool operator!=(const Int2& lhs,
                    const Int2& rhs)
    {
        return !(lhs == rhs);
    }

    inline Int2 operator+(const Int2& lhs,
                          const Int2& rhs)
    {
        return {lhs.x + rhs.x, lhs.y + rhs.y};
    }

    inline Int2& operator+=(Int2& lhs,
                            const Int2& rhs)
    {
        lhs.x += rhs.x;
        lhs.y += rhs.y;
        return lhs;
    }

    inline Int2 operator-(const Int2& lhs,
                          const Int2& rhs)
    {
        return {lhs.x - rhs.x, lhs.y - rhs.y};
    }

    inline Int2& operator-=(Int2& lhs,
                            const Int2& rhs)
    {
        lhs.x -= rhs.x;
        lhs.y -= rhs.y;
        return lhs;
    }

    inline Int2 operator*(const Int2& lhs,
                          const Int2& rhs)
    {
        return {lhs.x * rhs.x, lhs.y * rhs.y};
    }

    inline Int2& operator*=(Int2& lhs,
                            const Int2& rhs)
    {
        lhs.x *= rhs.x;
        lhs.y *= rhs.y;
        return lhs;
    }

    inline Int2 operator/(const Int2& lhs,
                          const Int2& rhs)
    {
        return {lhs.x / rhs.x, lhs.y / rhs.y};
    }

    inline Int2& operator/=(Int2& lhs, const Int2& rhs)
    {
        lhs.x /= rhs.x;
        lhs.y /= rhs.y;
        return lhs; 
    }

    struct SubmitInfo
    {
        VkSemaphore WaitSemaphore;
        VkPipelineStageFlagBits2 WaitPipelineStage;
        VkSemaphore SignalSemaphore;
        VkPipelineStageFlagBits2 SignalPipelineStage;
        VkFence Fence;
    };

    struct ShaderInfo
    {
        VkShaderModule ShaderModule;
        VkPipelineShaderStageCreateInfo ShaderStage;
    };

    struct Descriptor
    {
        VkDescriptorSetLayout Layout;
        VkDescriptorSet Set;
        VkDescriptorPool Pool;
    };

    struct Shader
    {
        VkPipeline Pipeline;
        VkPipelineBindPoint BindPoint;
        std::vector<VkRenderingAttachmentInfo> ColorAttachments;
        VkRenderingAttachmentInfo DepthAttachment;
    };

    struct Image
    {
        VkImage BaseImage{};
        VkImageView ImageView{};
        VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocation Allocation{};
    };
    
    struct Swapchain
    {
        VkSwapchainKHR SwapChain;
        Int2 Dimensions;
        std::vector<Image> Images;
        uint32_t CurrentImageIndex;
    };

    struct Command
    {
        VkCommandPool Pool;
        VkCommandBuffer Buffer;
    };

    struct Queue
    {
        VkQueue BaseQueue;
        uint32_t QueueIndex;
    };

    struct FrameData
    {
        Command Command;
        VkSemaphore ImageAvailable;
        VkSemaphore RenderFinished;
        VkFence Fence;
    };

    struct Context
    {
        vkb::Instance Instance;
        vkb::Device Device;
        vkb::PhysicalDevice GPU;
        VmaAllocator Allocator{};
        VkSurfaceKHR Surface{};
    };
}