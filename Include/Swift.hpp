#pragma once
#include "SwiftEnums.hpp"
#include "SwiftStructs.hpp"
#include "expected"
#include "vector"

namespace Swift
{
    std::expected<void,
                  Error>
    Init(const InitInfo& info);
    void Shutdown();

    // Render Operations
    std::expected<void,
                  Error>
    BeginFrame(const DynamicInfo& info);

    std::expected<void,
                  Error>
    EndFrame(const DynamicInfo& info);

    void BeginRendering();

    void BeginRendering(const BeginRenderInfo& renderInfo);

    void EndRendering();

    void BindShader(const ShaderHandle& shaderHandle);
    void BindIndexBuffer(BufferHandle bufferHandle);

    void DispatchCompute(uint32_t groupX,
                         uint32_t groupY,
                         uint32_t groupZ);

    void Draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex = 0,
                     int vertexOffset = 0,
                     uint32_t firstInstance = 0);

    void DrawIndexedIndirect(const BufferHandle& bufferHandle,
                             uint64_t offset,
                             uint32_t drawCount,
                             uint32_t stride);

    void DrawIndexedIndirectCount(const BufferHandle& bufferHandle,
                                  uint64_t offset,
                                  const BufferHandle& countBufferHandle,
                                  uint64_t countOffset,
                                  uint32_t maxDrawCount,
                                  uint32_t stride);

    void ClearSwapchain(Float4 color);

    void ClearImage(ImageHandle imageHandle,
                    Float4 color);

    void PushConstant(const void* data,
                      uint32_t size);

    template <typename T> void PushConstant(T pushConstant)
    {
        PushConstant(&pushConstant, sizeof(pushConstant));
    }

    void SetViewportAndScissor(Int2 extent);

    void SetCullMode(CullMode cullMode);

    void SetDepthTest(bool depthTest);

    void SetDepthWrite(bool depthWrite);

    void SetDepthCompareOp(DepthCompareOp depthCompareOp);

    void SetFrontFace(FrontFace frontFace);

    void SetLineWidth(float lineWidth);

    void SetTopology(Topology topology);

    // Transfer Operations
    void Resolve(ImageHandle srcImageHandle,
                 ImageHandle resolvedImageHandle);
    void BlitImage(ImageHandle srcImageHandle,
                   ImageHandle dstImageHandle,
                   Int2 srcExtent,
                   Int2 dstExtent);

    void BlitToSwapchain(ImageHandle srcImageHandle,
                         Int2 srcExtent);

    // For doing transfer only operations on the transfer queue, this is useful
    // for doing transfer operations in parallel with rendering
    void BeginTransfer();
    void EndTransfer();

    // Shader Operations
    std::expected<ShaderHandle,
                  Error>
    CreateGraphicsShader(const GraphicsShaderCreateInfo& createInfo);
    std::expected<ShaderHandle,
                  Error>
    CreateComputeShader(const ComputeShaderCreateInfo& createInfo);

    // Image Operations
    std::expected<ImageHandle,
                  Error>
    CreateImage(const ImageCreateInfo& createInfo);
    void DestroyImage(ImageHandle handle);

    std::expected<TempImageHandle,
                  Error>
    CreateTempImage(const ImageCreateInfo& createInfo);

    std::expected<SamplerHandle,
                  Error>
    CreateSampler(const SamplerCreateInfo& createInfo);
    VkSampler GetDefaultSampler();

    Int2 GetImageSize(ImageHandle imageHandle);

    std::expected<VkImageView,
                  Error>
    GetImageView(ImageHandle imageHandle);

    std::expected<void,
                  Error>
    UpdateImage(ImageHandle baseImageHandle,
                TempImageHandle tempImageHandle);

    void ClearTempImages();

    // Buffer Operations
    std::expected<BufferHandle,
                  Error>
    CreateBuffer(const BufferCreateInfo& createInfo);
    void DestroyBuffer(BufferHandle bufferHandle);

    std::expected<void*,
                  Error>
    MapBuffer(BufferHandle bufferHandle);
    void UnmapBuffer(BufferHandle bufferHandle);
    uint64_t GetBufferAddress(BufferHandle bufferHandle);

    void CopyBuffer(BufferHandle srcHandle,
                    BufferHandle dstHandle,
                    const std::vector<BufferCopy>& copyRegions);

    void UpdateBuffer(BufferHandle bufferHandle,
                      const void* data,
                      uint64_t offset,
                      uint64_t size);

    // Misc
    void WaitIdle();

    void TransitionImage(ImageHandle imageHandle,
                         VkImageLayout newLayout);
    void CopyBufferToImage(BufferHandle srcBuffer,
                           ImageHandle dstImageHandle,
                           const std::vector<BufferImageCopy>& copyRegions);

    Context GetContext();
    Queue GetGraphicsQueue();
    Queue GetTransferQueue();
    Command GetGraphicsCommand();
    Command GetTransferCommand();
} // namespace Swift
