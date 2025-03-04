#pragma once
#include "SwiftEnums.hpp"
#include "VkBootstrap.h"
#include "string"
#include "variant"
#include "vector"
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
        Int2 Dimensions;
        std::variant<GLFWwindow*> Window;
        bool EnableDebugMessenger = false;
        bool EnableValidationLayer = false;
        bool EnableMonitorLayer = false;

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
        auto& SetDimensions(const Int2& dimensions)
        {
            Dimensions = dimensions;
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
    };

    struct DynamicInfo
    {
        Int2 Dimensions;

        auto& SetDimensions(const Int2& dimensions)
        {
            Dimensions = dimensions;
            return *this;
        }
    };

    using ShaderHandle = uint32_t;
    using ImageHandle = uint32_t;
    using BufferHandle = uint32_t;
    inline uint32_t InvalidHandle = std::numeric_limits<uint32_t>::max();

    struct GraphicsShaderCreateInfo
    {
        std::vector<char> VertexCode;
        std::vector<char> FragmentCode;
        std::vector<VkFormat> ColorFormats;
        VkFormat DepthFormat;
    };
    
    struct ImageCreateInfo
    {
        VkFormat Format{};
        VkExtent2D Extent{};
        VkImageUsageFlags Usage{};
        uint32_t MipLevels = 1;
        bool IsCubemap = false;
    };
} // namespace Swift