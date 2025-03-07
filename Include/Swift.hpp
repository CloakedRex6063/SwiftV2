#pragma once
#include "SwiftStructs.hpp"
#include "SwiftEnums.hpp"
#include "expected"
#include "vector"

namespace Swift
{
    std::expected<void,
        Error>
    Init(const InitInfo &info);

    void Shutdown();

    // Render Operations
    std::expected<void, Error> BeginFrame(const DynamicInfo &info);

    std::expected<void, Error> EndFrame(const DynamicInfo &info);

    void BeginRendering(const std::vector<ImageHandle> &colorAttachments,
                        const ImageHandle &depthAttachment,
                        const Int2 &dimensions);

    void EndRendering();

    void BindShader(const ShaderHandle &shaderHandle);

    void DispatchCompute(uint32_t groupX, uint32_t groupY, uint32_t groupZ);

    void Draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex = 0,
                     int vertexOffset = 0,
                     uint32_t firstInstance = 0);

    void DrawIndexedIndirect(
        const BufferHandle &bufferHandle,
        uint64_t offset,
        uint32_t drawCount,
        uint32_t stride);

    void DrawIndexedIndirectCount(
        const BufferHandle &bufferHandle,
        uint64_t offset,
        const BufferHandle &countBufferHandle,
        uint64_t countOffset,
        uint32_t maxDrawCount,
        uint32_t stride);

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
    void Resolve(ImageHandle srcImageHandle, ImageHandle resolvedImageHandle);
    void BlitImage(ImageHandle srcImageHandle,
                   ImageHandle dstImageHandle,
                   Int2 srcExtent,
                   Int2 dstExtent);

    void BlitToSwapchain(ImageHandle srcImageHandle,
                         Int2 srcExtent);

    // Shader Operations
    std::expected<ShaderHandle, Error> CreateGraphicsShader(const GraphicsShaderCreateInfo &createInfo);
    std::expected<ShaderHandle, Error> CreateComputeShader(const ComputeShaderCreateInfo &createInfo);

    // Image Operations
    std::expected<ImageHandle, Error> CreateImage(const ImageCreateInfo &createInfo);

    std::expected<TempImageHandle, Error> CreateTempImage(const ImageCreateInfo &createInfo);

    std::expected<SamplerHandle, Error> CreateSampler(const SamplerCreateInfo &createInfo);

    std::expected<Int2, Error> GetImageSize(ImageHandle imageHandle);

    std::expected<VkImageView, Error> GetImageView(ImageHandle imageHandle);

    std::expected<void, Error> UpdateImage(ImageHandle baseImageHandle, TempImageHandle tempImageHandle);

    void ClearTempImages();

    // Buffer Operations
    std::expected<BufferHandle, Error> CreateBuffer(const BufferCreateInfo &createInfo);

    std::expected<void *, Error> MapBuffer(BufferHandle bufferHandle);

    std::expected<void, Error> CopyToBuffer(BufferHandle bufferHandle, const void *data, uint64_t offset,
                                            uint64_t size);

    void UpdateBuffer(BufferHandle bufferHandle, const void *data, uint64_t offset, uint64_t size);

    // Misc
    void WaitIdle();
} // namespace Swift
