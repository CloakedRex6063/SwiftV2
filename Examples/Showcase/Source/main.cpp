#define SWIFT_GLFW
#include "GLFW/glfw3.h"
#include "Swift.hpp"
#include "SwiftStructs.hpp"
#include "Utility.hpp"

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const auto window = glfwCreateWindow(1280, 720, "Swift", nullptr, nullptr);
    auto initInfo = Swift::InitInfo()
                        .SetAppName("Showcase")
                        .SetEngineName("Swift")
                        .SetPreferredDeviceType(Swift::DeviceType::eDiscrete)
                        .SetDimensions({1280, 720})
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
        .DepthFormat = {VK_FORMAT_UNDEFINED},
    };

    const auto shaderResult = Swift::CreateGraphicsShader(shaderCreateInfo);
    if (!shaderResult) return -1;
    const auto shader = shaderResult.value();

    const Swift::ImageCreateInfo imageInfo = {
        .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .Extent = VkExtent2D{1280, 720},
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .MipLevels = 1,
        .IsCubemap = false,
    };

    const auto imageResult = Swift::CreateImage(imageInfo);
    const auto renderImage = imageResult.value();

    const Swift::ImageCreateInfo depthImageInfo = {
        .Format = VK_FORMAT_D32_SFLOAT,
        .Extent = VkExtent2D{1280, 720},
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .MipLevels = 1,
        .IsCubemap = false,
    };
    const auto depthImageResult = Swift::CreateImage(depthImageInfo);
    const auto depthImage = depthImageResult.value();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        auto info = Swift::DynamicInfo().SetDimensions({width, height});
        auto result = Swift::BeginFrame(info);
        if (!result) return -1;

        Swift::ClearImage(renderImage, Swift::Float4{0.2, 0.3, 0.4, 0.4});

        Swift::BindShader(shader);

        Swift::SetViewportAndScissor(Swift::Int2(1280, 720));
        Swift::SetCullMode(Swift::CullMode::eFront);
        Swift::SetDepthCompareOp(Swift::DepthCompareOp::eLessOrEqual);
        Swift::SetDepthTest(true);
        Swift::SetDepthWrite(true);
        Swift::SetFrontFace(Swift::FrontFace::eCounterClockWise);
        Swift::SetTopology(Swift::Topology::eTriangleList);

        Swift::BeginRendering({renderImage}, Swift::InvalidHandle, {1280, 720});

        Swift::Draw(3, 1, 0, 0);

        Swift::EndRendering();

        Swift::BlitToSwapchain(renderImage, {1280, 720});

        result = Swift::EndFrame(info);
        if (!result) return -1;
    }
    return 0;
}
