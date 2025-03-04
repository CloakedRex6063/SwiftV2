#pragma once
#include "SwiftEnums.hpp"
#include "expected"
#include "vector"

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"

#include <SwiftStructs.hpp>

typedef uint32_t VkBool32;
struct VkColorBlendEquationEXT;

namespace Swift
{
    struct Float2;
    struct Int2;
    using ShaderHandle = uint32_t;
    using ImageHandle = uint32_t;
    using BufferHandle = uint32_t;
    struct VulkanRenderAttachment;
    struct Float4;
    struct InitInfo;
    struct DynamicInfo;
    struct ImageCreateInfo;
    std::expected<void,
                  Error>
    Init(const InitInfo& info);

    // Creation
    std::expected<ShaderHandle,
                  Error>
    CreateGraphicsShader(const GraphicsShaderCreateInfo& createInfo);
    std::expected<ImageHandle,
                  Error>
    CreateImage(const ImageCreateInfo& createInfo);

    // Render Operations
    std::expected<void,
                  Error>
    BeginFrame(const DynamicInfo& info);
    std::expected<void,
                  Error>
    EndFrame(const DynamicInfo& info);
  
    void BeginRendering(const std::vector<ImageHandle>& colorAttachments,
                        const ImageHandle& depthAttachment,
                        const Int2& dimensions);
    void EndRendering();

    void BindShader(const ShaderHandle& shaderHandle);
    void Draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);
    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex = 0,
                     int vertexOffset = 0,
                     uint32_t firstInstance = 0);

    void ClearSwapchain(Float4 color);
    void ClearImage(ImageHandle imageHandle,
                    Float4 color);

    void SetViewportAndScissor(Int2 extent);
    void SetCullMode(CullMode cullMode);
    void SetDepthTest(bool depthTest);
    void SetDepthWrite(bool depthWrite);
    void SetDepthCompareOp(DepthCompareOp depthCompareOp);
    void SetFrontFace(FrontFace frontFace);
    void SetLineWidth(float lineWidth);
    void SetTopology(Topology topology);

    // Transfer Operations
    void BlitToSwapchain(ImageHandle srcImageHandle,
                         Int2 srcExtent);
} // namespace Swift
