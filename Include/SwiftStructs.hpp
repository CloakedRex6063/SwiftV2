#pragma once
#include "SwiftEnums.hpp"
#include "variant"
#define VK_NO_PROTOTYPES
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

struct GLFWwindow;
namespace Swift
{
    struct Int2
    {
        int x;
        int y;
    };

    struct Float2
    {
        float x;
        float y;
    };

    struct Float4
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct InitInfo
    {
        std::string AppName;
        std::string EngineName;
        DeviceType PreferredDeviceType;
        Int2 Extent;
        std::variant<GLFWwindow*> Window;
        bool EnableDebugMessenger = false;
        bool EnableValidationLayer = false;
        bool EnableMonitorLayer = false;
        int AdditionalGraphicsQueueCount = 0;
        int AdditionalComputeQueueCount = 0;
        int AdditionalOpticalFlowQueueCount = 0;
        VkPhysicalDeviceVulkan12Features AdditionalFeatures12{};
        VkPhysicalDeviceVulkan13Features AdditionalFeatures13{};

        auto& SetAppName(const std::string& name)
        {
            AppName = name;
            return *this;
        }
        auto& SetEngineName(const std::string& name)
        {
            EngineName = name;
            return *this;
        }
        auto& SetPreferredDeviceType(const DeviceType preferredDeviceType)
        {
            PreferredDeviceType = preferredDeviceType;
            return *this;
        };
        auto& SetExtent(const Int2& extent)
        {
            Extent = extent;
            return *this;
        }
        auto& SetWindow(GLFWwindow* window)
        {
            Window = window;
            return *this;
        }
        auto& SetEnableDebugMessenger(const bool enableDebugMessenger)
        {
            EnableDebugMessenger = enableDebugMessenger;
            return *this;
        }
        auto& SetEnableValidationLayer(const bool enableValidationLayer)
        {
            EnableValidationLayer = enableValidationLayer;
            return *this;
        }
        auto& SetEnableMonitorLayer(const bool enableMonitorLayer)
        {
            EnableMonitorLayer = enableMonitorLayer;
            return *this;
        }
        auto& SetAdditionalGraphicsQueueCount(const int additionalGraphicsQueueCount)
        {
            AdditionalGraphicsQueueCount = additionalGraphicsQueueCount;
            return *this;
        }
        auto& SetAdditionalComputeQueueCount(const int additionalComputeQueueCount)
        {
            AdditionalComputeQueueCount = additionalComputeQueueCount;
            return *this;
        }
        auto& SetAdditionalOpticalFlowQueueCount(const int additionalOpticalFlowQueueCount)
        {
            AdditionalOpticalFlowQueueCount = additionalOpticalFlowQueueCount;
            return *this;
        }
        auto& SetAdditionalFeatures12(const VkPhysicalDeviceVulkan12Features& additionalFeatures12)
        {
            AdditionalFeatures12 = additionalFeatures12;
            return *this;
        }
        auto& SetAdditionalFeatures13(const VkPhysicalDeviceVulkan13Features& additionalFeatures13)
        {
            AdditionalFeatures13 = additionalFeatures13;
            return *this;
        }
    };

    struct DynamicInfo
    {
        Int2 Extent;

        auto& SetExtent(const Int2& extent)
        {
            Extent = extent;
            return *this;
        }
    };

    using ShaderHandle = uint32_t;
    using ImageHandle = uint32_t;
    using TempImageHandle = uint32_t;
    using SamplerHandle = uint32_t;
    using BufferHandle = uint32_t;
    inline uint32_t InvalidHandle = std::numeric_limits<uint32_t>::max();

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

    struct Context
    {
        vkb::Instance Instance;
        vkb::Device Device;
        vkb::PhysicalDevice GPU;
        VmaAllocator Allocator{};
        VkSurfaceKHR Surface{};
    };

    struct GraphicsShaderCreateInfo
    {
        std::vector<char> VertexCode;
        std::vector<char> FragmentCode;
        std::vector<VkFormat> ColorFormats;
        VkFormat DepthFormat;
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    };

    struct ComputeShaderCreateInfo
    {
        std::vector<char> ComputeCode;
    };

    struct ImageCreateInfo
    {
        VkFormat Format{};
        Int2 Extent{};
        VkImageUsageFlags Usage{};
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
        SamplerHandle Sampler = InvalidHandle;
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
    };

    struct SamplerCreateInfo
    {
        VkFilter MinFilter = VK_FILTER_LINEAR;
        VkFilter MagFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode MipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode AddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode AddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode AddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkBorderColor BorderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    };

    struct BufferCreateInfo
    {
        BufferUsage Usage;
        uint64_t Size;
    };

    struct BufferCopy
    {
        uint64_t SrcOffset;
        uint64_t DstOffset;
        uint64_t Size;
    };

    struct BufferImageCopy
    {
        uint64_t BufferOffset;
        uint32_t MipLevel;
        uint32_t ArrayLayer;
        Int2 Extent;
    };


    struct BeginRenderInfo
    {
        std::vector<ImageHandle> ColorAttachments;
        ImageHandle DepthAttachment;
        Int2 Dimensions;
        LoadOp ColorLoadOp = LoadOp::eClear;
        StoreOp ColorStoreOp = StoreOp::eStore;
        LoadOp DepthLoadOp = LoadOp::eClear;
        StoreOp DepthStoreOp = StoreOp::eStore;
    };
} // namespace Swift