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
    std::array<FrameData, 3> gFrameData;
    Queue gGraphicsQueue;
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

    uint32_t gCurrentFrame = 0;
} // namespace

std::expected<void,
    Error>
Swift::Init(const InitInfo &info)
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

    const auto swapchainResult =
            Vulkan::CreateSwapchain(gContext, gGraphicsQueue, info.Extent);
    if (!swapchainResult)
    {
        return std::unexpected(swapchainResult.error());
    }
    gSwapchain = swapchainResult.value();

    for (auto &frameData: gFrameData)
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

    const SamplerCreateInfo samplerCreateInfo{};
    const auto samplerResult = Vulkan::CreateSampler(gContext, samplerCreateInfo);
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

    for (const auto &sampler: gSamplers)
    {
        vkDestroySampler(gContext.Device, sampler, nullptr);
    }

    for (const auto &image: gImages)
    {
        vmaDestroyImage(gContext.Allocator, image.BaseImage, image.Allocation);
        vkDestroyImageView(gContext.Device, image.ImageView, nullptr);
    }

    for (const auto &tempImage: gTempImages)
    {
        vmaDestroyImage(gContext.Allocator, tempImage.BaseImage, tempImage.Allocation);
    }

    for (const auto &buffer: gBuffers)
    {
        vmaDestroyBuffer(gContext.Allocator, buffer.BaseBuffer, buffer.Allocation);
    }

    for (const auto &shader: gShaders)
    {
        vkDestroyPipeline(gContext.Device, shader.Pipeline, nullptr);
    }
    vkDestroyPipelineLayout(gContext.Device, gPipelineLayout, nullptr);

    for (const auto &[Command, ImageAvailable, RenderFinished, Fence]: gFrameData)
    {
        vkDestroyCommandPool(gContext.Device, Command.Pool, nullptr);
        vkDestroySemaphore(gContext.Device, ImageAvailable, nullptr);
        vkDestroySemaphore(gContext.Device, RenderFinished, nullptr);
        vkDestroyFence(gContext.Device, Fence, nullptr);
    }

    vkDestroyDescriptorPool(gContext.Device, gDescriptor.Pool, nullptr);
    vkDestroyDescriptorSetLayout(gContext.Device, gDescriptor.Layout, nullptr);

    for (const auto &image: gSwapchain.Images)
    {
        vkDestroyImageView(gContext.Device, image.ImageView, nullptr);
    }
    vkDestroySwapchainKHR(gContext.Device, gSwapchain.SwapChain, nullptr);
    vmaDestroyAllocator(gContext.Allocator);
    vkb::destroy_device(gContext.Device);
    vkDestroySurfaceKHR(gContext.Instance, gContext.Surface, nullptr);
    vkb::destroy_instance(gContext.Instance);
}

std::expected<void,
    Error>
Swift::BeginFrame(const DynamicInfo &info)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto result = Vulkan::WaitFence(gContext.Device, currentFrameData.Fence);
    if (!result)
    {
        return std::unexpected(result.error());
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
        return std::unexpected(acquireResult.error());
    }
    gSwapchain.CurrentImageIndex = acquireResult.value();

    Vulkan::BeginCommandBuffer(currentFrameData.Command);
    return {};
}

std::expected<void,
    Error>
Swift::EndFrame(const DynamicInfo &info)
{
    const auto &[Command, ImageAvailable, RenderFinished, Fence] = gFrameData.at(gCurrentFrame);
    auto &image = Vulkan::GetSwapchainImage(gSwapchain);
    Vulkan::TransitionImage(Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_IMAGE_ASPECT_COLOR_BIT);

    Vulkan::EndCommandBuffer(Command);

    const SubmitInfo submitInfo{
        .WaitSemaphore = ImageAvailable,
        .WaitPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .SignalSemaphore = RenderFinished,
        .SignalPipelineStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .Fence = Fence,
    };
    Vulkan::SubmitQueue(gGraphicsQueue, Command, submitInfo);

    const auto result =
            Vulkan::Present(gSwapchain, gGraphicsQueue, RenderFinished);

    if (!result)
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

void Swift::BeginRendering(const std::vector<ImageHandle> &colorAttachments,
                           const ImageHandle &depthAttachment,
                           const Int2 &dimensions)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &shader = gShaders.at(gCurrentShader);

    for (const auto &[index, colorAttachment]:
         std::views::enumerate(shader.ColorAttachments))
    {
        auto &realImage = gImages.at(colorAttachments.at(index));
        colorAttachment.imageView = realImage.ImageView;
        Vulkan::TransitionImage(currentFrameData.Command,
                                realImage,
                                realImage.CurrentLayout,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);
    }
    if (depthAttachment != InvalidHandle)
    {
        auto &depthImage = gImages.at(depthAttachment);
        shader.DepthAttachment.imageView = depthImage.ImageView;
        Vulkan::TransitionImage(currentFrameData.Command,
                                depthImage,
                                depthImage.CurrentLayout,
                                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    Vulkan::BeginRendering(currentFrameData.Command,
                           shader.ColorAttachments,
                           shader.DepthAttachment,
                           dimensions);
}

void Swift::EndRendering()
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    Vulkan::EndRendering(currentFrameData.Command);
}

void Swift::BindShader(const ShaderHandle &shaderHandle)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    const auto &shader = gShaders.at(shaderHandle);
    vkCmdBindPipeline(currentFrameData.Command.Buffer,
                      shader.BindPoint,
                      shader.Pipeline);
}

void Swift::Draw(const uint32_t vertexCount,
                 const uint32_t instanceCount,
                 const uint32_t firstVertex,
                 const uint32_t firstInstance)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
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
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdDrawIndexed(currentFrameData.Command.Buffer,
                     indexCount,
                     instanceCount,
                     firstIndex,
                     vertexOffset,
                     firstInstance);
}

void Swift::DrawIndexedIndirect(const BufferHandle &bufferHandle, const uint64_t offset, const uint32_t drawCount,
                                const uint32_t stride)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    const auto &buffer = gBuffers.at(bufferHandle);
    vkCmdDrawIndexedIndirect(currentFrameData.Command.Buffer,
                             buffer.BaseBuffer,
                             offset,
                             drawCount,
                             stride);
}

void Swift::DrawIndexedIndirectCount(const BufferHandle &bufferHandle, const uint64_t offset,
                                     const BufferHandle &countBufferHandle,
                                     const uint64_t countOffset, const uint32_t maxDrawCount, const uint32_t stride)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    const auto &buffer = gBuffers.at(bufferHandle);
    const auto &countBuffer = gBuffers.at(countBufferHandle);
    vkCmdDrawIndexedIndirectCount(currentFrameData.Command.Buffer,
                                  buffer.BaseBuffer,
                                  offset,
                                  countBuffer.BaseBuffer, countOffset, maxDrawCount, stride);
}

void Swift::ClearSwapchain(const Float4 color)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &image = Vulkan::GetSwapchainImage(gSwapchain);
    Vulkan::TransitionImage(currentFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    Vulkan::ClearImage(currentFrameData.Command, image, color);
}

void Swift::ClearImage(const ImageHandle imageHandle,
                       const Float4 color)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &image = gImages.at(imageHandle);
    Vulkan::TransitionImage(currentFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    Vulkan::ClearImage(currentFrameData.Command, image, color);
}

void Swift::SetViewportAndScissor(const Int2 extent)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
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
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetCullMode(currentFrameData.Command.Buffer,
                     static_cast<VkCullModeFlags>(cullMode));
}

void Swift::SetDepthTest(const bool depthTest)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthTestEnable(currentFrameData.Command.Buffer, depthTest);
}

void Swift::SetDepthWrite(const bool depthWrite)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthWriteEnable(currentFrameData.Command.Buffer, depthWrite);
}

void Swift::SetDepthCompareOp(DepthCompareOp depthCompareOp)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetDepthCompareOp(currentFrameData.Command.Buffer,
                           static_cast<VkCompareOp>(depthCompareOp));
}

void Swift::SetFrontFace(FrontFace frontFace)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetFrontFace(currentFrameData.Command.Buffer,
                      static_cast<VkFrontFace>(frontFace));
}

void Swift::SetLineWidth(const float lineWidth)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetLineWidth(currentFrameData.Command.Buffer, lineWidth);
}

void Swift::SetTopology(Topology topology)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    vkCmdSetPrimitiveTopology(currentFrameData.Command.Buffer,
                              static_cast<VkPrimitiveTopology>(topology));
}

void Swift::Resolve(const ImageHandle srcImageHandle, const ImageHandle resolvedImageHandle)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &srcImage = gImages.at(srcImageHandle);
    auto &resolvedImage = gImages.at(resolvedImageHandle);
    const auto &extent = VkExtent3D(srcImage.Extent.x, srcImage.Extent.y, 1);
    Vulkan::TransitionImage(currentFrameData.Command, srcImage, srcImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Vulkan::TransitionImage(currentFrameData.Command, resolvedImage, resolvedImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::ResolveImage(currentFrameData.Command.Buffer, srcImage.BaseImage, resolvedImage.BaseImage, extent);
}

std::expected<ShaderHandle, Error> Swift::CreateGraphicsShader(const GraphicsShaderCreateInfo &createInfo)
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

    const VkRenderingAttachmentInfo colorInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    const VkRenderingAttachmentInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    const std::vector colorAttachments{createInfo.ColorFormats.size(), colorInfo};

    const uint32_t shaderHandle = gShaders.size();
    gShaders.emplace_back(Shader{
        pipelineResult.value(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        colorAttachments,
        depthInfo,
    });
    return shaderHandle;
}

void Swift::BlitImage(const ImageHandle srcImageHandle, const ImageHandle dstImageHandle, const Int2 srcExtent,
                      const Int2 dstExtent)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &srcImage = gImages.at(srcImageHandle);
    auto &dstImage = gImages.at(dstImageHandle);
    Vulkan::TransitionImage(currentFrameData.Command,
                            srcImage,
                            srcImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Vulkan::TransitionImage(currentFrameData.Command,
                            dstImage,
                            dstImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::BlitImage(currentFrameData.Command,
                      srcImage,
                      dstImage,
                      srcExtent,
                      dstExtent);
}

void Swift::BlitToSwapchain(const ImageHandle srcImageHandle,
                            const Int2 srcExtent)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    auto &srcImage = gImages.at(srcImageHandle);
    auto &dstImage = Vulkan::GetSwapchainImage(gSwapchain);

    Vulkan::TransitionImage(currentFrameData.Command,
                            srcImage,
                            srcImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Vulkan::TransitionImage(currentFrameData.Command,
                            dstImage,
                            dstImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::BlitImage(currentFrameData.Command,
                      srcImage,
                      dstImage,
                      srcExtent,
                      gSwapchain.Dimensions);
}

std::expected<Int2, Error> Swift::GetImageSize(const ImageHandle imageHandle)
{
    if (imageHandle == InvalidHandle || imageHandle >= gImages.size())
    {
        return std::unexpected(Error::eImageNotFound);
    }
    return gImages.at(imageHandle).Extent;
}

std::expected<VkImageView, Error> Swift::GetImageView(const ImageHandle imageHandle)
{
    if (imageHandle == InvalidHandle || imageHandle >= gImages.size())
    {
        return std::unexpected(Error::eImageNotFound);
    }
    return gImages.at(imageHandle).ImageView;
}

std::expected<BufferHandle, Error> Swift::CreateBuffer(const BufferCreateInfo &createInfo)
{
    const auto result = Vulkan::CreateBuffer(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gBuffers.emplace_back(result.value());
    return static_cast<uint32_t>(gBuffers.size() - 1);
}

std::expected<void *, Error> Swift::MapBuffer(const BufferHandle bufferHandle)
{
#ifdef SWIFT_DEBUG
    if (bufferHandle == InvalidHandle || bufferHandle >= gBuffers.size())
    {
        return std::unexpected(Error::eBufferNotFound);
    }
#endif
    const auto &buffer = gBuffers.at(bufferHandle);
    return Vulkan::MapBuffer(gContext, buffer);
}

std::expected<void, Error> Swift::CopyToBuffer(const BufferHandle bufferHandle, const void *data, const uint64_t offset,
                                               const uint64_t size)
{
#ifdef SWIFT_DEBUG
    if (bufferHandle == InvalidHandle || bufferHandle >= gBuffers.size())
    {
        return std::unexpected(Error::eBufferNotFound);
    }
#endif
    const auto &buffer = gBuffers.at(bufferHandle);
    return Vulkan::CopyToBuffer(gContext.Allocator, buffer.Allocation, data, offset, size);
}

void Swift::UpdateBuffer(const BufferHandle bufferHandle, const void *data, const uint64_t offset, const uint64_t size)
{
    const auto &currentFrameData = gFrameData.at(gCurrentFrame);
    const auto &buffer = gBuffers.at(bufferHandle);
    Vulkan::UpdateBuffer(currentFrameData.Command.Buffer, buffer.BaseBuffer, data, offset, size);
}

std::expected<ImageHandle, Error> Swift::CreateImage(const ImageCreateInfo &createInfo)
{
    const auto result = Vulkan::CreateImage(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gImages.emplace_back(result.value());
    const uint32_t arrayElement = gImages.size() - 1;
    Vulkan::UpdateDescriptorSampler(gContext.Device, gDescriptor.Set, gSamplers[0], gImages.back().ImageView,
                                    arrayElement);
    if (createInfo.Usage & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        Vulkan::UpdateDescriptorImage(gContext.Device, gDescriptor.Set, gSamplers[0], gImages.back().ImageView,
                                      arrayElement);
    }
    return arrayElement;
}

std::expected<TempImageHandle, Error> Swift::CreateTempImage(const ImageCreateInfo &createInfo)
{
    const auto result = Vulkan::CreateImage(gContext, createInfo);
    if (!result)
    {
        return std::unexpected(result.error());
    }
    gTempImages.emplace_back(result.value());
    return static_cast<uint32_t>(gTempImages.size() - 1);
}

void Swift::ClearTempImages()
{
    gTempImages.clear();
}

std::expected<void, Error> Swift::UpdateImage(const ImageHandle baseImageHandle, const TempImageHandle tempImageHandle)
{
#ifdef SWIFT_DEBUG
    if (baseImageHandle == Swift::InvalidHandle || baseImageHandle > gImages.size() - 1)
    {
        return std::unexpected(Error::eImageNotFound);
    }
    if (tempImageHandle == Swift::InvalidHandle || tempImageHandle > gTempImages.size() - 1)
    {
        return std::unexpected(Error::eImageNotFound);
    }
#endif
    auto &baseImage = gImages.at(baseImageHandle);
    Vulkan::DestroyImage(gContext, baseImage);
    const auto &tempImage = gTempImages.at(tempImageHandle);
    baseImage = tempImage;
    Vulkan::UpdateDescriptorSampler(
        gContext.Device,
        gDescriptor.Set,
        gSamplers[0],
        baseImage.ImageView,
        baseImageHandle);
    return {};
}

void Swift::WaitIdle()
{
    vkDeviceWaitIdle(gContext.Device);
}
