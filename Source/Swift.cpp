#include "Swift.hpp"
#define VOLK_IMPLEMENTATION
#include "Vulkan/VulkanInit.hpp"
#include "Vulkan/VulkanRender.hpp"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

using namespace Swift;

namespace
{
    Context gContext;
    Swapchain gSwapchain;

    std::array<FrameData, 2> gFrameData;
    uint32_t gCurrentFrame = 0;

    Queue gGraphicsQueue;
    Queue gTransferQueue;
    Command gTransferCommand;
    VkFence gTransferFence;
    VkPipelineLayout gPipelineLayout;
    Descriptor gDescriptor;

    std::vector<Shader> gShaders;
    uint32_t gCurrentShader = 0;
    std::vector<Image> gImages;
    std::vector<Image> gTempImages;
    std::vector<Buffer> gBuffers;
    std::vector<VkSampler> gSamplers;
    VkViewport gViewport;
    VkRect2D gScissor;
} // namespace

std::expected<void,
              Error>
Swift::Init(const InitInfo& info)
{
    const auto contextResult = Vulkan::CreateContext(info);
    if (!contextResult)
    {
        return std::unexpected(contextResult.error());
    }
    gContext = contextResult.value();

    const auto queueResult =
        Vulkan::CreateQueue(gContext, vkb::QueueType::graphics);
    if (!queueResult)
    {
        return std::unexpected(queueResult.error());
    }
    gGraphicsQueue = queueResult.value();

    const auto transferQueueResult =
        Vulkan::CreateQueue(gContext, vkb::QueueType::transfer);
    if (!transferQueueResult)
    {
        return std::unexpected(queueResult.error());
    }
    gTransferQueue = transferQueueResult.value();

    const auto transferFenceResult = Vulkan::CreateFence(gContext.Device);
    if (!transferFenceResult)
    {
        return std::unexpected(transferFenceResult.error());
    }
    gTransferFence = transferFenceResult.value();

    const auto transferCommandResult =
        Vulkan::CreateCommand(gContext.Device, gTransferQueue.QueueIndex);
    if (!transferCommandResult)
    {
        return std::unexpected(transferCommandResult.error());
    }
    gTransferCommand = transferCommandResult.value();

    const auto swapchainResult =
        Vulkan::CreateSwapchain(gContext, gGraphicsQueue, info.Extent);
    if (!swapchainResult)
    {
        return std::unexpected(swapchainResult.error());
    }
    gSwapchain = swapchainResult.value();

    for (auto& frameData : gFrameData)
    {
        const auto frameDataResult = Vulkan::CreateFrameData(gContext.Device);
        if (!frameDataResult)
        {
            return std::unexpected(frameDataResult.error());
        }
        frameData = frameDataResult.value();
    }

    const auto descriptorResult = Vulkan::CreateDescriptor(gContext.Device);
    if (!descriptorResult)
    {
        return std::unexpected(descriptorResult.error());
    }
    gDescriptor = descriptorResult.value();

    const auto pipelineLayoutResult =
        Vulkan::CreatePipelineLayout(gContext, gDescriptor.Layout);
    if (!pipelineLayoutResult)
    {
        return std::unexpected(pipelineLayoutResult.error());
    }
    gPipelineLayout = pipelineLayoutResult.value();

    constexpr SamplerCreateInfo samplerCreateInfo{};
    const auto samplerResult =
        Vulkan::CreateSampler(gContext, samplerCreateInfo);
    if (!samplerResult)
    {
        return std::unexpected(samplerResult.error());
    }
    gSamplers.emplace_back(samplerResult.value());

    return {};
}

void Swift::Shutdown()
{
    vkDeviceWaitIdle(gContext.Device);

    for (const auto& sampler : gSamplers)
    {
        vkDestroySampler(gContext.Device, sampler, nullptr);
    }

    for (const auto& image : gImages)
    {
        if (!image.Allocation) continue;
        vmaDestroyImage(gContext.Allocator, image.BaseImage, image.Allocation);
        vkDestroyImageView(gContext.Device, image.ImageView, nullptr);
    }

    for (auto& tempImage : gTempImages)
    {
        Vulkan::DestroyImage(gContext, tempImage);
    }

    for (auto& buffer : gBuffers)
    {
        if (!buffer.Allocation) continue;
        Vulkan::DestroyBuffer(gContext.Allocator, buffer);
    }

    for (const auto& shader : gShaders)
    {
        vkDestroyPipeline(gContext.Device, shader.Pipeline, nullptr);
    }
    vkDestroyPipelineLayout(gContext.Device, gPipelineLayout, nullptr);

    for (const auto& [Command, ImageAvailable, RenderFinished, Fence] :
         gFrameData)
    {
        vkDestroyCommandPool(gContext.Device, Command.Pool, nullptr);
        vkDestroySemaphore(gContext.Device, ImageAvailable, nullptr);
        vkDestroySemaphore(gContext.Device, RenderFinished, nullptr);
        vkDestroyFence(gContext.Device, Fence, nullptr);
    }

    vkDestroyFence(gContext.Device, gTransferFence, nullptr);
    vkDestroyCommandPool(gContext.Device, gTransferCommand.Pool, nullptr);
    vkDestroyDescriptorPool(gContext.Device, gDescriptor.Pool, nullptr);
    vkDestroyDescriptorSetLayout(gContext.Device, gDescriptor.Layout, nullptr);

    for (const auto& image : gSwapchain.Images)
    {
        vkDestroyImageView(gContext.Device, image.ImageView, nullptr);
    }
    Vulkan::DestroyImage(gContext, gSwapchain.DepthImage);
    vkDestroySwapchainKHR(gContext.Device, gSwapchain.SwapChain, nullptr);
    vmaDestroyAllocator(gContext.Allocator);
    vkb::destroy_device(gContext.Device);
    vkDestroySurfaceKHR(gContext.Instance, gContext.Surface, nullptr);
    vkb::destroy_instance(gContext.Instance);
}

std::expected<void,
              Error>
Swift::BeginFrame(const DynamicInfo& info)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto result = Vulkan::WaitFence(gContext.Device, currentFrameData.Fence);
    if (!result)
    {
        return std::unexpected(result.error());
    }

    if (info.Extent != gSwapchain.Dimensions)
    {
        const auto swapchainResult = Vulkan::RecreateSwapchain(gContext,
                                                               gGraphicsQueue,
                                                               gSwapchain,
                                                               info.Extent);
        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
    }

    const auto acquireResult =
        Vulkan::AcquireNextImage(gContext,
                                 gSwapchain,
                                 currentFrameData.ImageAvailable);

    result = Vulkan::ResetFence(gContext.Device, currentFrameData.Fence);
    if (!result)
    {
        return std::unexpected(result.error());
    }

    if (!acquireResult)
    {
        const auto swapchainResult = Vulkan::RecreateSwapchain(gContext,
                                                               gGraphicsQueue,
                                                               gSwapchain,
                                                               info.Extent);
        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
    }
    gSwapchain.CurrentImageIndex = acquireResult.value();

    Vulkan::BeginCommandBuffer(currentFrameData.Command);

    return {};
}

std::expected<void,
              Error>
Swift::EndFrame(const DynamicInfo& info)
{
    const auto& [Command, ImageAvailable, RenderFinished, Fence] =
        gFrameData.at(gCurrentFrame);

    auto& image = Vulkan::GetSwapchainImage(gSwapchain);

    const auto presentTransition =
        Vulkan::TransitionImage(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    Vulkan::PipelineBarrier(Command.Buffer, {presentTransition});
    Vulkan::EndCommandBuffer(Command);

    const SubmitInfo submitInfo{
        .WaitSemaphore = ImageAvailable,
        .WaitPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .SignalSemaphore = RenderFinished,
        .SignalPipelineStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .Fence = Fence,
    };
    Vulkan::SubmitQueue(gGraphicsQueue, Command, submitInfo);

    if (!Vulkan::Present(gSwapchain, gGraphicsQueue, RenderFinished))
    {
        const auto swapchainResult = Vulkan::RecreateSwapchain(gContext,
                                                               gGraphicsQueue,
                                                               gSwapchain,
                                                               info.Extent);
        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
    }

    gCurrentFrame = (gCurrentFrame + 1) % gFrameData.size();
    return {};
}

void Swift::BeginRendering()
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& swapchainImage = Vulkan::GetSwapchainImage(gSwapchain);
    const auto renderTransition =
        Vulkan::TransitionImage(swapchainImage,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto depthTransition =
        Vulkan::TransitionImage(swapchainImage,
                                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer,
                            {renderTransition, depthTransition});

    const VkRenderingAttachmentInfo colorInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImage.ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    const VkRenderingAttachmentInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = gSwapchain.DepthImage.ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    Vulkan::BeginRendering(currentFrameData.Command,
                           {colorInfo},
                           depthInfo,
                           gSwapchain.Dimensions);
}

void Swift::BeginRendering(const std::vector<ImageHandle>& colorAttachments,
                           const ImageHandle& depthAttachment,
                           const Int2& dimensions)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& shader = gShaders.at(gCurrentShader);

    std::vector<VkImageMemoryBarrier2> imageBarriers;
    for (const auto& [index, colorAttachment] :
         std::views::enumerate(shader.ColorAttachments))
    {
        auto& realImage = gImages.at(colorAttachments.at(index));
        colorAttachment.imageView = realImage.ImageView;
        imageBarriers.emplace_back(
            Vulkan::TransitionImage(realImage,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }
    if (depthAttachment != InvalidHandle)
    {
        auto& depthImage = gImages.at(depthAttachment);
        shader.DepthAttachment.imageView = depthImage.ImageView;
        imageBarriers.emplace_back(
            Vulkan::TransitionImage(depthImage,
                                    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_ASPECT_DEPTH_BIT));
    }
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer, {imageBarriers});

    Vulkan::BeginRendering(currentFrameData.Command,
                           shader.ColorAttachments,
                           shader.DepthAttachment,
                           dimensions);
}

void Swift::EndRendering()
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    Vulkan::EndRendering(currentFrameData.Command);
}

void Swift::BindShader(const ShaderHandle& shaderHandle)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& shader = gShaders.at(shaderHandle);
    gCurrentShader = shaderHandle;
    vkCmdBindPipeline(currentFrameData.Command.Buffer,
                      shader.BindPoint,
                      shader.Pipeline);
    vkCmdBindDescriptorSets(currentFrameData.Command.Buffer,
                        shader.BindPoint,
                        gPipelineLayout,
                        0,
                        1,
                        &gDescriptor.Set,
                        0,
                        nullptr);
}

void Swift::BindIndexBuffer(const BufferHandle bufferHandle)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& buffer = gBuffers.at(bufferHandle);
    vkCmdBindIndexBuffer(currentFrameData.Command.Buffer,
                         buffer.BaseBuffer,
                         0,
                         VK_INDEX_TYPE_UINT32);
}

void Swift::DispatchCompute(const uint32_t groupX,
                            const uint32_t groupY,
                            const uint32_t groupZ)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdDispatch(currentFrameData.Command.Buffer, groupX, groupY, groupZ);
}

void Swift::Draw(const uint32_t vertexCount,
                 const uint32_t instanceCount,
                 const uint32_t firstVertex,
                 const uint32_t firstInstance)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdDraw(currentFrameData.Command.Buffer,
              vertexCount,
              instanceCount,
              firstVertex,
              firstInstance);
}

void Swift::DrawIndexed(const uint32_t indexCount,
                        const uint32_t instanceCount,
                        const uint32_t firstIndex,
                        const int vertexOffset,
                        const uint32_t firstInstance)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdDrawIndexed(currentFrameData.Command.Buffer,
                     indexCount,
                     instanceCount,
                     firstIndex,
                     vertexOffset,
                     firstInstance);
}

void Swift::DrawIndexedIndirect(const BufferHandle& bufferHandle,
                                const uint64_t offset,
                                const uint32_t drawCount,
                                const uint32_t stride)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& buffer = gBuffers.at(bufferHandle);
    vkCmdDrawIndexedIndirect(currentFrameData.Command.Buffer,
                             buffer.BaseBuffer,
                             offset,
                             drawCount,
                             stride);
}

void Swift::DrawIndexedIndirectCount(const BufferHandle& bufferHandle,
                                     const uint64_t offset,
                                     const BufferHandle& countBufferHandle,
                                     const uint64_t countOffset,
                                     const uint32_t maxDrawCount,
                                     const uint32_t stride)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& buffer = gBuffers.at(bufferHandle);
    const auto& countBuffer = gBuffers.at(countBufferHandle);
    vkCmdDrawIndexedIndirectCount(currentFrameData.Command.Buffer,
                                  buffer.BaseBuffer,
                                  offset,
                                  countBuffer.BaseBuffer,
                                  countOffset,
                                  maxDrawCount,
                                  stride);
}

void Swift::ClearSwapchain(const Float4 color)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& image = Vulkan::GetSwapchainImage(gSwapchain);
    const auto clearTransition =
        Vulkan::TransitionImage(image, VK_IMAGE_LAYOUT_GENERAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer, {clearTransition});
    Vulkan::ClearImage(currentFrameData.Command, image, color);
}

void Swift::ClearImage(const ImageHandle imageHandle,
                       const Float4 color)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& image = gImages.at(imageHandle);
    const auto clearTransition =
        Vulkan::TransitionImage(image, VK_IMAGE_LAYOUT_GENERAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer, {clearTransition});
    Vulkan::ClearImage(currentFrameData.Command, image, color);
}

void Swift::PushConstant(const void* data,
                         const uint32_t size)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdPushConstants(currentFrameData.Command.Buffer,
                       gPipelineLayout,
                       VK_SHADER_STAGE_ALL,
                       0,
                       size,
                       data);
}

void Swift::SetViewportAndScissor(const Int2 extent)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    gViewport.width = static_cast<float>(extent.x);
    gViewport.height = static_cast<float>(extent.y);
    gViewport.minDepth = 0.0f;
    gViewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentFrameData.Command.Buffer, 0, 1, &gViewport);

    gScissor.offset = {0, 0};
    gScissor.extent.width = static_cast<uint32_t>(extent.x);
    gScissor.extent.height = static_cast<uint32_t>(extent.y);
    vkCmdSetScissor(currentFrameData.Command.Buffer, 0, 1, &gScissor);
}

void Swift::SetCullMode(CullMode cullMode)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetCullMode(currentFrameData.Command.Buffer,
                     static_cast<VkCullModeFlags>(cullMode));
}

void Swift::SetDepthTest(const bool depthTest)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthTestEnable(currentFrameData.Command.Buffer, depthTest);
}

void Swift::SetDepthWrite(const bool depthWrite)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthWriteEnable(currentFrameData.Command.Buffer, depthWrite);
}

void Swift::SetDepthCompareOp(DepthCompareOp depthCompareOp)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthCompareOp(currentFrameData.Command.Buffer,
                           static_cast<VkCompareOp>(depthCompareOp));
}

void Swift::SetFrontFace(FrontFace frontFace)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetFrontFace(currentFrameData.Command.Buffer,
                      static_cast<VkFrontFace>(frontFace));
}

void Swift::SetLineWidth(const float lineWidth)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetLineWidth(currentFrameData.Command.Buffer, lineWidth);
}

void Swift::SetTopology(Topology topology)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetPrimitiveTopology(currentFrameData.Command.Buffer,
                              static_cast<VkPrimitiveTopology>(topology));
}

void Swift::Resolve(const ImageHandle srcImageHandle,
                    const ImageHandle resolvedImageHandle)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& srcImage = gImages.at(srcImageHandle);
    auto& resolvedImage = gImages.at(resolvedImageHandle);
    const auto& extent = VkExtent3D(srcImage.Extent.x, srcImage.Extent.y, 1);
    const auto transferSrcTransition =
        Vulkan::TransitionImage(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const auto transferDstTransition =
        Vulkan::TransitionImage(resolvedImage,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer,
                            {transferSrcTransition, transferDstTransition});
    Vulkan::ResolveImage(currentFrameData.Command.Buffer,
                         srcImage.BaseImage,
                         resolvedImage.BaseImage,
                         extent);
}

std::expected<ShaderHandle,
              Error>
Swift::CreateGraphicsShader(const GraphicsShaderCreateInfo& createInfo)
{
    const auto vertexShaderResult = Vulkan::CreateShader(gContext.Device,
                                                         createInfo.VertexCode,
                                                         ShaderStage::eVertex);
    if (!vertexShaderResult)
    {
        return std::unexpected(vertexShaderResult.error());
    }
    const auto [vertShaderModule, vertShaderStage] = vertexShaderResult.value();

    const auto fragmentShaderResult =
        Vulkan::CreateShader(gContext.Device,
                             createInfo.FragmentCode,
                             ShaderStage::eFragment);
    if (!fragmentShaderResult)
    {
        return std::unexpected(fragmentShaderResult.error());
    }
    const auto [fragShaderModule, fragShaderStage] =
        fragmentShaderResult.value();

    const auto pipelineResult =
        Vulkan::CreateGraphicsPipeline(gContext.Device,
                                       gPipelineLayout,
                                       {vertShaderStage, fragShaderStage},
                                       createInfo);
    if (!pipelineResult)
    {
        return std::unexpected(pipelineResult.error());
    }

    vkDestroyShaderModule(gContext.Device, vertShaderModule, nullptr);
    vkDestroyShaderModule(gContext.Device, fragShaderModule, nullptr);

    constexpr VkRenderingAttachmentInfo colorInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    constexpr VkRenderingAttachmentInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    const std::vector colorAttachments{createInfo.ColorFormats.size(),
                                       colorInfo};

    const uint32_t shaderHandle = gShaders.size();
    gShaders.emplace_back(Shader{
        pipelineResult.value(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        colorAttachments,
        depthInfo,
    });
    return shaderHandle;
}

std::expected<ShaderHandle,
              Error>
Swift::CreateComputeShader(const ComputeShaderCreateInfo& createInfo)
{
    const auto computeShaderResult =
        Vulkan::CreateShader(gContext.Device,
                             createInfo.ComputeCode,
                             ShaderStage::eCompute);
    if (!computeShaderResult)
    {
        return std::unexpected(computeShaderResult.error());
    }
    const auto [computeShaderModule, computeShaderStage] =
        computeShaderResult.value();
    const auto computePipelineResult =
        Vulkan::CreateComputePipeline(gContext.Device,
                                      gPipelineLayout,
                                      computeShaderStage);
    if (!computePipelineResult)
    {
        return std::unexpected(computePipelineResult.error());
    }

    vkDestroyShaderModule(gContext.Device, computeShaderModule, nullptr);
    const uint32_t shaderHandle = gShaders.size();
    gShaders.emplace_back(Shader{
        computePipelineResult.value(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
    });
    return shaderHandle;
}

void Swift::BlitImage(const ImageHandle srcImageHandle,
                      const ImageHandle dstImageHandle,
                      const Int2 srcExtent,
                      const Int2 dstExtent)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& srcImage = gImages.at(srcImageHandle);
    auto& dstImage = gImages.at(dstImageHandle);
    const auto& blitSrcTransition =
        Vulkan::TransitionImage(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const auto& blitDstTransition =
        Vulkan::TransitionImage(dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer,
                            {blitSrcTransition, blitDstTransition});
    Vulkan::BlitImage(currentFrameData.Command,
                      srcImage,
                      dstImage,
                      srcExtent,
                      dstExtent);
}

void Swift::BlitToSwapchain(const ImageHandle srcImageHandle,
                            const Int2 srcExtent)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& srcImage = gImages.at(srcImageHandle);
    auto& dstImage = Vulkan::GetSwapchainImage(gSwapchain);

    const auto blitSrcTransition =
        Vulkan::TransitionImage(srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    const auto blitDstTransition =
        Vulkan::TransitionImage(dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer,
                            {blitSrcTransition, blitDstTransition});
    Vulkan::BlitImage(currentFrameData.Command,
                      srcImage,
                      dstImage,
                      srcExtent,
                      gSwapchain.Dimensions);
}

void Swift::BeginTransfer() { Vulkan::BeginCommandBuffer(gTransferCommand); }

void Swift::EndTransfer()
{
    Vulkan::EndCommandBuffer(gTransferCommand);
    const SubmitInfo submitInfo{.WaitSemaphore = nullptr,
                                .WaitPipelineStage = VK_PIPELINE_STAGE_2_NONE,
                                .SignalSemaphore = nullptr,
                                .SignalPipelineStage = VK_PIPELINE_STAGE_2_NONE,
                                .Fence = gTransferFence};
    Vulkan::SubmitQueue(gTransferQueue, gTransferCommand, submitInfo);
    const auto result = Vulkan::WaitFence(gContext.Device, gTransferFence);
    Vulkan::ResetFence(gContext.Device, gTransferFence);
}

Int2 Swift::GetImageSize(const ImageHandle imageHandle)
{
    return gImages.at(imageHandle).Extent;
}

std::expected<VkImageView,
              Error>
Swift::GetImageView(const ImageHandle imageHandle)
{
    if (imageHandle == InvalidHandle || imageHandle >= gImages.size())
    {
        return std::unexpected(Error::eImageNotFound);
    }
    return gImages.at(imageHandle).ImageView;
}

std::expected<BufferHandle,
              Error>
Swift::CreateBuffer(const BufferCreateInfo& createInfo)
{
    const auto result = Vulkan::CreateBuffer(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gBuffers.emplace_back(result.value());
    return static_cast<uint32_t>(gBuffers.size() - 1);
}

void Swift::DestroyBuffer(const BufferHandle bufferHandle)
{
    auto& buffer = gBuffers.at(bufferHandle);
    Vulkan::DestroyBuffer(gContext.Allocator, buffer);
}

std::expected<void*,
              Error>
Swift::MapBuffer(const BufferHandle bufferHandle)
{
#ifdef SWIFT_DEBUG
    if (bufferHandle == InvalidHandle || bufferHandle >= gBuffers.size())
    {
        return std::unexpected(Error::eBufferNotFound);
    }
#endif
    const auto& buffer = gBuffers.at(bufferHandle);
    return Vulkan::MapBuffer(gContext, buffer);
}

void Swift::UnmapBuffer(const BufferHandle bufferHandle)
{
    const auto& buffer = gBuffers.at(bufferHandle);
    Vulkan::UnmapBuffer(gContext, buffer);
}

uint64_t Swift::GetBufferAddress(BufferHandle bufferHandle)
{
    auto& buffer = gBuffers.at(bufferHandle);
    return Vulkan::GetBufferAddress(gContext.Device, buffer.BaseBuffer);
}

void Swift::CopyBuffer(const BufferHandle srcHandle,
                       const BufferHandle dstHandle,
                       const std::vector<BufferCopy>& copyRegions)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& srcBuffer = gBuffers.at(srcHandle);
    const auto& dstBuffer = gBuffers.at(dstHandle);

    Vulkan::CopyBuffer(currentFrameData.Command.Buffer,
                       srcBuffer.BaseBuffer,
                       dstBuffer.BaseBuffer,
                       copyRegions);
}

void Swift::UpdateBuffer(const BufferHandle bufferHandle,
                         const void* data,
                         const uint64_t offset,
                         const uint64_t size)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    const auto& buffer = gBuffers.at(bufferHandle);
    Vulkan::UpdateBuffer(currentFrameData.Command.Buffer,
                         buffer.BaseBuffer,
                         data,
                         offset,
                         size);
}

std::expected<ImageHandle,
              Error>
Swift::CreateImage(const ImageCreateInfo& createInfo)
{
    const auto result = Vulkan::CreateImage(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gImages.emplace_back(result.value());
    const uint32_t arrayElement = gImages.size() - 1;

    VkSampler sampler;
    if (createInfo.Sampler == InvalidHandle)
    {
        sampler = gSamplers[0];
    }
    else
    {
        sampler = gSamplers.at(createInfo.Sampler);
    }

    Vulkan::UpdateDescriptorSampler(gContext.Device,
                                    gDescriptor.Set,
                                    sampler,
                                    gImages.back().ImageView,
                                    arrayElement);
    if (createInfo.Usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        Vulkan::UpdateDescriptorImage(gContext.Device,
                                      gDescriptor.Set,
                                      sampler,
                                      gImages.back().ImageView,
                                      arrayElement);
    }
    return arrayElement;
}

void Swift::DestroyImage(const ImageHandle handle)
{
    Vulkan::DestroyImage(gContext, gImages.at(handle));
}

std::expected<TempImageHandle,
              Error>
Swift::CreateTempImage(const ImageCreateInfo& createInfo)
{
    const auto result = Vulkan::CreateImage(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gTempImages.emplace_back(result.value());
    return static_cast<uint32_t>(gTempImages.size() - 1);
}

std::expected<SamplerHandle,
              Error>
Swift::CreateSampler(const SamplerCreateInfo& createInfo)
{
    const auto samplerResult = Vulkan::CreateSampler(gContext, createInfo);
    if (!samplerResult)
    {
        return std::unexpected(samplerResult.error());
    }
    gSamplers.emplace_back(samplerResult.value());
    return static_cast<uint32_t>(gSamplers.size() - 1);
}

VkSampler Swift::GetDefaultSampler() { return gSamplers[0]; }

void Swift::ClearTempImages() { gTempImages.clear(); }

std::expected<void,
              Error>
Swift::UpdateImage(const ImageHandle baseImageHandle,
                   const TempImageHandle tempImageHandle)
{
#ifdef SWIFT_DEBUG
    if (baseImageHandle == Swift::InvalidHandle ||
        baseImageHandle > gImages.size() - 1)
    {
        return std::unexpected(Error::eImageNotFound);
    }
    if (tempImageHandle == Swift::InvalidHandle ||
        tempImageHandle > gTempImages.size() - 1)
    {
        return std::unexpected(Error::eImageNotFound);
    }
#endif
    auto& baseImage = gImages.at(baseImageHandle);
    Vulkan::DestroyImage(gContext, baseImage);
    const auto& tempImage = gTempImages.at(tempImageHandle);
    baseImage = tempImage;

    VkSampler sampler;
    if (baseImage.Sampler == InvalidHandle)
    {
        sampler = gSamplers[0];
    }
    else
    {
        sampler = gSamplers.at(baseImage.Sampler);
    }

    Vulkan::UpdateDescriptorSampler(gContext.Device,
                                    gDescriptor.Set,
                                    sampler,
                                    baseImage.ImageView,
                                    baseImageHandle);
    return {};
}

void Swift::WaitIdle() { vkDeviceWaitIdle(gContext.Device); }

void Swift::TransitionImage(const ImageHandle imageHandle,
                            const VkImageLayout newLayout)
{
    const auto& currentFrameData = gFrameData.at(gCurrentFrame);
    auto& image = gImages.at(imageHandle);
    const auto transition = Vulkan::TransitionImage(image, newLayout);
    Vulkan::PipelineBarrier(currentFrameData.Command.Buffer, {transition});
}

void Swift::CopyBufferToImage(const BufferHandle srcBuffer,
                              const ImageHandle dstImageHandle,
                              const std::vector<BufferImageCopy>& copyRegions)
{
    const auto& buffer = gBuffers.at(srcBuffer);
    auto& image = gImages.at(dstImageHandle);
    std::vector<VkBufferImageCopy2> vkCopyRegions;
    for (const auto& [BufferOffset, MipLevel, ArrayLayer, Extent] : copyRegions)
    {
        VkBufferImageCopy2 copy{
            .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .bufferOffset = BufferOffset,
            .imageSubresource =
                Vulkan::GetImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT,
                                                  MipLevel,
                                                  ArrayLayer),
            .imageExtent = VkExtent3D(Extent.x, Extent.y, 1),
        };
        vkCopyRegions.emplace_back(copy);
    }
    const auto dstTransition =
        Vulkan::TransitionImage(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::PipelineBarrier(gTransferCommand.Buffer, {dstTransition});
    Vulkan::CopyBufferToImage(gTransferCommand.Buffer,
                              buffer.BaseBuffer,
                              image.BaseImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              vkCopyRegions);
    const auto shaderReadTransition =
        Vulkan::TransitionImage(image,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Vulkan::PipelineBarrier(gTransferCommand.Buffer, {shaderReadTransition});
}

Context Swift::GetContext() { return gContext; }

Queue Swift::GetGraphicsQueue() { return gGraphicsQueue; }

Queue Swift::GetTransferQueue() { return gTransferQueue; }

Command Swift::GetGraphicsCommand()
{
    return gFrameData.at(gCurrentFrame).Command;
}

Command Swift::GetTransferCommand() { return gTransferCommand; }
