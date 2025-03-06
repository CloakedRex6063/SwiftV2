#pragma once

namespace Swift
{
    enum class Error
    {
        eSuccess,
        eInstanceInitFailed,
        eSurfaceInitFailed,
        eNoDeviceFound,
        eDeviceInitFailed,
        eSwapchainCreateFailed,
        eSemaphoreCreateFailed,
        eFenceCreateFailed,
        eCommandPoolCreateFailed,
        eCommandBufferCreateFailed,
        eDescriptorCreateFailed,
        eAcquireFailed,
        ePresentFailed,
        eFailedToWaitFence,
        eFailedToGetQueue,
        ePipelineLayoutCreateFailed,
        ePipelineCreateFailed,
        eShaderCreateFailed,
        eImageCreateFailed,
        eSamplerCreateFailed,
        eBufferCreateFailed,
        eImageNotFound,
        eBufferNotFound,
        eBufferMapFailed,
        eCopyFailed
    };

    enum class DeviceType
    {
        eIntegrated,
        eDiscrete
    };

    enum class ShaderStage
    {
        eVertex,
        eFragment,
        eCompute
    };

    enum class BufferUsage
    {
        eUniform,
        eStorage,
        eIndex,
        eIndirect,
        eReadback
    };

    enum class CullMode
    {
        eNone,
        eFront,
        eBack,
        eFrontAndBack
    };

    enum class DepthCompareOp
    {
        eNever,
        eLess,
        eEqual,
        eLessOrEqual,
        eGreater,
        eNotEqual,
        eGreaterOrEqual,
        eAlways
    };

    enum class PolygonMode
    {
        eFill,
        eLine,
        ePoint,
    };

    enum class Topology
    {
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
        eTriangleFan
    };

    enum class FrontFace
    {
        eCounterClockWise,
        eClockWise,
    };
} // namespace Swift