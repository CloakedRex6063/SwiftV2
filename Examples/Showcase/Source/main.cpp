#define SWIFT_GLFW
#include "GLFW/glfw3.h"
#include "Swift.hpp"
#include "SwiftStructs.hpp"
#include "Utility.hpp"
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
            .SetWindow(window);
#ifndef NDEBUG
    initInfo.SetEnableDebugMessenger(true).SetEnableValidationLayer(true).SetEnableMonitorLayer(true);
#endif

    if (!Swift::Init(initInfo)) return -1;

    auto vertexCode = Utility::ReadBinaryFile("Shaders/triangle.vert.spv");
    auto fragmentCode = Utility::ReadBinaryFile("Shaders/triangle.frag.spv");

    Swift::GraphicsShaderCreateInfo shaderCreateInfo{
        .VertexCode = std::move(vertexCode),
        .FragmentCode = std::move(fragmentCode),
        .ColorFormats = {VK_FORMAT_B8G8R8A8_UNORM},
        .DepthFormat = VK_FORMAT_D32_SFLOAT,
        .Samples = gSamples,
    };

    const auto shaderResult = Swift::CreateGraphicsShader(shaderCreateInfo);
    if (!shaderResult) return -1;
    const auto shader = shaderResult.value();

    const auto sampledImageResult = Swift::Image::LoadImage("Resources/Default_albedo.dds");
    const auto sampledImage = sampledImageResult.value();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        auto info = Swift::DynamicInfo().SetExtent({width, height});

        auto result = Swift::BeginFrame(info);
        if (!result) return -1;

        Swift::BlitToSwapchain(sampledImage, {2048, 2048});

        Swift::BindShader(shader);

        Swift::SetViewport({Swift::Int2(width, height)});
        Swift::SetScissor({Swift::Int2(width, height)});
        Swift::SetCullMode(Swift::CullMode::eFront);
        Swift::SetDepthCompareOp(Swift::DepthCompareOp::eLessOrEqual);
        Swift::SetDepthTest(true);
        Swift::SetDepthWrite(true);
        Swift::SetFrontFace(Swift::FrontFace::eCounterClockWise);
        Swift::SetTopology(Swift::Topology::eTriangleList);

        Swift::BeginRendering();

        Swift::Draw(3, 1, 0, 0);

        Swift::EndRendering();

        result = Swift::EndFrame(info);
        if (!result) return -1;
    }
    Swift::Shutdown();
    return 0;
}
