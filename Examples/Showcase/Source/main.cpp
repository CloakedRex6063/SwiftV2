#define SWIFT_GLFW
#include "GLFW/glfw3.h"
#include "Swift.hpp"
#include "SwiftStructs.hpp"
#include "Utility.hpp"
#include "imgui.h"
#include "Image.hpp"

VkSampleCountFlagBits gSamples = VK_SAMPLE_COUNT_1_BIT;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const auto window = glfwCreateWindow(1280, 720, "Swift", nullptr, nullptr);
    auto initInfo = Swift::InitInfo()
            .SetAppName("Showcase")
            .SetEngineName("Swift")
            .SetPreferredDeviceType(Swift::DeviceType::eDiscrete)
            .SetExtent({1280, 720})
            .SetWindow(window)
            .SetEnableMonitorLayer(true);
#ifndef NDEBUG
    initInfo.SetEnableDebugMessenger(true).SetEnableValidationLayer(true);
#endif

    if (!Swift::Init(initInfo)) return -1;

    auto vertexCode = Utility::ReadBinaryFile("Shaders/triangle.vert.spv");
    auto fragmentCode = Utility::ReadBinaryFile("Shaders/triangle.frag.spv");

    Swift::GraphicsShaderCreateInfo shaderCreateInfo{
        .VertexCode = std::move(vertexCode),
        .FragmentCode = std::move(fragmentCode),
        .ColorFormats = {VK_FORMAT_R16G16B16A16_SFLOAT},
        .DepthFormat = VK_FORMAT_D32_SFLOAT,
        .Samples = gSamples,
    };

    const auto shaderResult = Swift::CreateGraphicsShader(shaderCreateInfo);
    if (!shaderResult) return -1;
    const auto shader = shaderResult.value();

    Swift::ImageCreateInfo imageInfo = {
        .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .Extent = Swift::Int2{1280, 720},
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .Samples = gSamples,
        .MipLevels = 1,
        .ArrayLayers = 1,
    };

    const auto imageResult = Swift::CreateImage(imageInfo);
    const auto renderImage = imageResult.value();

    auto resolvedInfo = imageInfo;
    resolvedInfo.Samples = VK_SAMPLE_COUNT_1_BIT;
    const auto resolvedImageResult = Swift::CreateImage(resolvedInfo);
    const auto resolvedImage = resolvedImageResult.value();

    Swift::ImageCreateInfo depthImageInfo = {
        .Format = VK_FORMAT_D32_SFLOAT,
        .Extent = Swift::Int2{1280, 720},
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .Samples = gSamples,
        .MipLevels = 1,
        .ArrayLayers = 1,
    };
    const auto depthImageResult = Swift::CreateImage(depthImageInfo);
    const auto depthImage = depthImageResult.value();

    const auto sampledImageResult = Swift::Image::LoadImage("Resources/Default_albedo.dds");
    const auto sampledImage = sampledImageResult.value();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        auto info = Swift::DynamicInfo().SetExtent({width, height});

        const auto imageSize = Swift::GetImageSize(renderImage);
        if (imageSize.x != width || imageSize.y != height)
        {
            Swift::WaitIdle();
            imageInfo.Extent = Swift::Int2{width, height};
            const auto tempImage = Swift::CreateTempImage(imageInfo);
            assert(tempImage);
            const auto colorResult = Swift::UpdateImage(renderImage, tempImage.value());
            assert(colorResult);

            resolvedInfo.Extent = Swift::Int2{width, height};
            const auto tempResolveImage = Swift::CreateTempImage(resolvedInfo);
            assert(tempResolveImage);
            const auto resolveResult = Swift::UpdateImage(resolvedImage, tempResolveImage.value());
            assert(resolveResult);

            depthImageInfo.Extent = Swift::Int2{width, height};
            const auto tempDepthImage = Swift::CreateTempImage(depthImageInfo);
            assert(tempDepthImage);
            const auto depthResult = Swift::UpdateImage(depthImage, tempDepthImage.value());
            assert(depthResult);
            Swift::ClearTempImages();
        }

        auto result = Swift::BeginFrame(info);
        if (!result) return -1;

        Swift::ClearImage(renderImage, Swift::Float4{0.2, 0.3, 0.4, 0.4});

        Swift::BlitImage(sampledImage, renderImage, {2048, 2048}, {width, height});

        Swift::BindShader(shader);

        Swift::SetViewportAndScissor(Swift::Int2(width, height));
        Swift::SetCullMode(Swift::CullMode::eFront);
        Swift::SetDepthCompareOp(Swift::DepthCompareOp::eLessOrEqual);
        Swift::SetDepthTest(true);
        Swift::SetDepthWrite(true);
        Swift::SetFrontFace(Swift::FrontFace::eCounterClockWise);
        Swift::SetTopology(Swift::Topology::eTriangleList);

        Swift::BeginRendering({renderImage}, depthImage, {width, height});

        Swift::Draw(3, 1, 0, 0);

        Swift::EndRendering();

        ImGui::ShowDemoWindow();

        if (gSamples != VK_SAMPLE_COUNT_1_BIT)
        {
            Swift::Resolve(renderImage, resolvedImage);
            Swift::BlitToSwapchain(resolvedImage, {width, height});
        } else
        {
            Swift::BlitToSwapchain(renderImage, {width, height});
        }

        result = Swift::EndFrame(info);
        if (!result) return -1;
    }
    Swift::Shutdown();
    return 0;
}
