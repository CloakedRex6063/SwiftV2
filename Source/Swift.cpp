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
    FrameData gFrameData;
    Queue gGraphicsQueue;
    VkPipelineLayout gPipelineLayout;
    Descriptor gDescriptor;

    std::vector<Shader> gShaders;
    uint32_t gCurrentShader = 0;
    std::vector<Image> gImages;
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

    const auto swapchainResult =
        Vulkan::CreateSwapchain(gContext, gGraphicsQueue, info.Dimensions);
    if (!swapchainResult)
    {
        return std::unexpected(swapchainResult.error());
    }
    gSwapchain = swapchainResult.value();

    const auto frameData = Vulkan::CreateFrameData(gContext.Device);
    if (!frameData)
    {
        return std::unexpected(frameData.error());
    }
    gFrameData = frameData.value();

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

    return {};
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

    std::vector colorAttachments{createInfo.ColorFormats.size(), colorInfo};

    const uint32_t shaderHandle = gShaders.size();
    gShaders.emplace_back(Shader{
        pipelineResult.value(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        colorAttachments,
        depthInfo,
    });
    return shaderHandle;
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
    return static_cast<uint32_t>(gImages.size() - 1);
}

std::expected<void,
              Error>
Swift::BeginFrame(const DynamicInfo& info)
{
    auto result = Vulkan::WaitFence(gContext.Device, gFrameData.Fence);
    if (!result)
    {
        return std::unexpected(result.error());
    }

    const auto acquireResult =
        Vulkan::AcquireNextImage(gContext,
                                 gSwapchain,
                                 gFrameData.ImageAvailable);

    result = Vulkan::ResetFence(gContext.Device, gFrameData.Fence);
    if (!result)
    {
        return std::unexpected(result.error());
    }

    if (!acquireResult)
    {
        const auto swapchainResult = Vulkan::RecreateSwapchain(gContext,
                                                               gGraphicsQueue,
                                                               gSwapchain,
                                                               info.Dimensions);
        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
        return std::unexpected(acquireResult.error());
    }
    gSwapchain.CurrentImageIndex = acquireResult.value();

    Vulkan::BeginCommandBuffer(gFrameData.Command);
    return {};
}

std::expected<void,
              Error>
Swift::EndFrame(const DynamicInfo& info)
{
    auto& image = Vulkan::GetSwapchainImage(gSwapchain);
    Vulkan::TransitionImage(gFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_IMAGE_ASPECT_COLOR_BIT);

    Vulkan::EndCommandBuffer(gFrameData.Command);

    const SubmitInfo submitInfo{
        .WaitSemaphore = gFrameData.ImageAvailable,
        .WaitPipelineStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .SignalSemaphore = gFrameData.RenderFinished,
        .SignalPipelineStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .Fence = gFrameData.Fence,
    };
    Vulkan::SubmitQueue(gGraphicsQueue, gFrameData.Command, submitInfo);

    const auto result =
        Vulkan::Present(gSwapchain, gGraphicsQueue, gFrameData.RenderFinished);

    if (!result)
    {
        const auto swapchainResult = Vulkan::RecreateSwapchain(gContext,
                                                               gGraphicsQueue,
                                                               gSwapchain,
                                                               info.Dimensions);
        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
    }

    return {};
}

void Swift::BeginRendering(const std::vector<ImageHandle>& colorAttachments,
                           const ImageHandle& depthAttachment,
                           const Int2& dimensions)
{
    auto& image = Vulkan::GetSwapchainImage(gSwapchain);
    Vulkan::TransitionImage(gFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_ASPECT_COLOR_BIT);

    auto& shader = gShaders.at(gCurrentShader);

    for (const auto& [index, colorAttachment] :
         std::views::enumerate(shader.ColorAttachments))
    {
        const auto& realImage = gImages.at(colorAttachments.at(index));
        colorAttachment.imageView = realImage.ImageView;
    }
    if (depthAttachment != InvalidHandle)
    {
        const auto& depthImage = gImages.at(depthAttachment);
        shader.DepthAttachment.imageView = depthImage.ImageView;
    }

    Vulkan::BeginRendering(gFrameData.Command,
                           shader.ColorAttachments,
                           shader.DepthAttachment,
                           dimensions);
}

void Swift::EndRendering() { Vulkan::EndRendering(gFrameData.Command); }

void Swift::BindShader(const ShaderHandle& shaderHandle)
{
    const auto& shader = gShaders.at(shaderHandle);
    vkCmdBindPipeline(gFrameData.Command.Buffer,
                      shader.BindPoint,
                      shader.Pipeline);
}

void Swift::Draw(const uint32_t vertexCount,
                 const uint32_t instanceCount,
                 const uint32_t firstVertex,
                 const uint32_t firstInstance)
{
    vkCmdDraw(gFrameData.Command.Buffer,
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
    vkCmdDrawIndexed(gFrameData.Command.Buffer,
                     indexCount,
                     instanceCount,
                     firstIndex,
                     vertexOffset,
                     firstInstance);
}

void Swift::ClearSwapchain(const Float4 color)
{
    auto& image = Vulkan::GetSwapchainImage(gSwapchain);
    Vulkan::TransitionImage(gFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_ASPECT_COLOR_BIT);

    Vulkan::ClearImage(gFrameData.Command, image, color);
}

void Swift::ClearImage(const ImageHandle imageHandle,
                       const Float4 color)
{
    auto& image = gImages.at(imageHandle);
    Vulkan::TransitionImage(gFrameData.Command,
                            image,
                            image.CurrentLayout,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    Vulkan::ClearImage(gFrameData.Command, image, color);
}

void Swift::SetViewportAndScissor(const Int2 extent)
{
    gViewport.width = static_cast<float>(extent.x);
    gViewport.height = static_cast<float>(extent.y);
    gViewport.minDepth = 0.0f;
    gViewport.maxDepth = 1.0f;
    vkCmdSetViewport(gFrameData.Command.Buffer, 0, 1, &gViewport);

    gScissor.offset = {0, 0};
    gScissor.extent.width = static_cast<uint32_t>(extent.x);
    gScissor.extent.height = static_cast<uint32_t>(extent.y);
    vkCmdSetScissor(gFrameData.Command.Buffer, 0, 1, &gScissor);
}

void Swift::SetCullMode(CullMode cullMode)
{
    vkCmdSetCullMode(gFrameData.Command.Buffer,
                     static_cast<VkCullModeFlags>(cullMode));
}

void Swift::SetDepthTest(const bool depthTest)
{
    vkCmdSetDepthTestEnable(gFrameData.Command.Buffer, depthTest);
}

void Swift::SetDepthWrite(const bool depthWrite)
{
    vkCmdSetDepthWriteEnable(gFrameData.Command.Buffer, depthWrite);
}

void Swift::SetDepthCompareOp(DepthCompareOp depthCompareOp)
{
    vkCmdSetDepthCompareOp(gFrameData.Command.Buffer,
                           static_cast<VkCompareOp>(depthCompareOp));
}

void Swift::SetFrontFace(FrontFace frontFace)
{
    vkCmdSetFrontFace(gFrameData.Command.Buffer,
                      static_cast<VkFrontFace>(frontFace));
}

void Swift::SetLineWidth(const float lineWidth)
{
    vkCmdSetLineWidth(gFrameData.Command.Buffer, lineWidth);
}

void Swift::SetTopology(Topology topology)
{
    vkCmdSetPrimitiveTopology(gFrameData.Command.Buffer,
                              static_cast<VkPrimitiveTopology>(topology));
}

void Swift::BlitToSwapchain(const ImageHandle srcImageHandle,
                            const Int2 srcExtent)
{
    auto& srcImage = gImages.at(srcImageHandle);
    auto& dstImage = Vulkan::GetSwapchainImage(gSwapchain);

    Vulkan::TransitionImage(gFrameData.Command,
                            srcImage,
                            srcImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Vulkan::TransitionImage(gFrameData.Command,
                            dstImage,
                            dstImage.CurrentLayout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Vulkan::BlitImage(gFrameData.Command,
                      srcImage,
                      dstImage,
                      srcExtent,
                      gSwapchain.Dimensions);
}
