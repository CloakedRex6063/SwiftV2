#pragma once

inline vkb::Result<vkb::Instance> CreateInstance(const InitInfo& info)
{
    auto instanceBuilder = vkb::InstanceBuilder()
                               .require_api_version(1, 3, 0)
                               .set_app_name(info.AppName.c_str())
                               .set_engine_name(info.EngineName.c_str());

    if (info.EnableValidationLayer)
    {
        instanceBuilder.enable_validation_layers();
    }
    if (info.EnableDebugMessenger)
    {
        instanceBuilder.use_default_debug_messenger()
            .add_debug_messenger_severity(
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT);
    }
    if (info.EnableMonitorLayer)
    {
        instanceBuilder.enable_layer("VK_LAYER_LUNARG_monitor");
    }

    const auto instanceResult = instanceBuilder.build();
    return instanceResult;
}

inline vkb::Result<vkb::PhysicalDevice>
CreateSelector(vkb::Instance instance,
               VkSurfaceKHR surface,
               DeviceType preferredDeviceType)
{
    VkPhysicalDeviceFeatures deviceFeatures = {
        .multiDrawIndirect = true,
        .fillModeNonSolid = true,
        .depthBounds = true,
        .wideLines = true,
        .samplerAnisotropy = true,
        .textureCompressionBC = true,
    };

    VkPhysicalDeviceVulkan11Features deviceFeatures11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .multiview = true,
        .shaderDrawParameters = true,
    };

    VkPhysicalDeviceVulkan12Features deviceFeatures12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .drawIndirectCount = true,
        .descriptorIndexing = true,
        .shaderInputAttachmentArrayDynamicIndexing = true,
        .shaderUniformBufferArrayNonUniformIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .shaderStorageBufferArrayNonUniformIndexing = true,
        .shaderStorageImageArrayNonUniformIndexing = true,
        .shaderInputAttachmentArrayNonUniformIndexing = true,
        .descriptorBindingUniformBufferUpdateAfterBind = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageImageUpdateAfterBind = true,
        .descriptorBindingStorageBufferUpdateAfterBind = true,
        .descriptorBindingUpdateUnusedWhilePending = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .timelineSemaphore = true,
        .bufferDeviceAddress = true,
    };

    VkPhysicalDeviceVulkan13Features deviceFeatures13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
        .maintenance4 = true,
    };

    const auto gpuSelector = vkb::PhysicalDeviceSelector(instance, surface)
                                 .set_required_features_13(deviceFeatures13)
                                 .set_required_features_12(deviceFeatures12)
                                 .set_required_features_11(deviceFeatures11)
                                 .set_required_features(deviceFeatures);

    for (auto& selector : gpuSelector.select_devices().value())
    {
        switch (preferredDeviceType)
        {
        case DeviceType::eIntegrated:
            if (selector.properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                return selector;
            }
            break;
        case DeviceType::eDiscrete:
            if (selector.properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                return selector;
            }
            break;
        }
    }
    return gpuSelector.select();
};

inline std::expected<Context,
                     Error>
CreateContext(const InitInfo& info)
{
    Context context{};
    const auto instanceResult = CreateInstance(info);
    if (!instanceResult.has_value())
    {
        std::cerr << instanceResult.error().message() << std::endl;
        return std::unexpected(Error::eInstanceInitFailed);
    }
    context.Instance = instanceResult.value();

    VkSurfaceKHR surface = nullptr;
#ifdef SWIFT_GLFW
    const auto result =
        glfwCreateWindowSurface(context.Instance,
                                std::get<GLFWwindow*>(info.Window),
                                nullptr,
                                &surface);
    if (result != VK_SUCCESS)
    {
        return std::unexpected(Error::eSurfaceInitFailed);
    }
#endif
    context.Surface = surface;
    const auto gpuSelector = Vulkan::CreateSelector(context.Instance,
                                                    surface,
                                                    info.PreferredDeviceType);
    if (!gpuSelector.has_value())
    {
        return std::unexpected(Error::eNoDeviceFound);
    }
    const vkb::PhysicalDevice& gpu = gpuSelector.value();
    context.GPU = gpu;

    const auto deviceResult = vkb::DeviceBuilder(gpuSelector.value()).build();
    if (!deviceResult.has_value())
    {
        std::cout << deviceResult.error().message() << std::endl;
        return std::unexpected(Error::eDeviceInitFailed);
    }
    context.Device = deviceResult.value();

    volkInitialize();
    volkLoadInstance(context.Instance);
    volkLoadDevice(context.Device);

    const VmaVulkanFunctions vulkanFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };
    const VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = context.GPU,
        .device = context.Device,
        .pVulkanFunctions = &vulkanFunctions,
        .instance = context.Instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    vmaCreateAllocator(&allocatorInfo, &context.Allocator);

    return context;
}

inline std::expected<Queue,
                     Error>
CreateQueue(const Context& context,
            const vkb::QueueType queueType)
{
    Queue queue{};
    const auto queueResult = context.Device.get_queue(queueType);
    if (!queueResult)
    {
        std::cerr << queueResult.error().message() << std::endl;
        return std::unexpected(Error::eFailedToGetQueue);
    }
    queue.BaseQueue = queueResult.value();

    const auto queueIndexResult = context.Device.get_queue_index(queueType);
    if (!queueIndexResult)
    {
        std::cerr << queueIndexResult.error().message() << std::endl;
        return std::unexpected(Error::eFailedToGetQueue);
    }
    queue.QueueIndex = queueIndexResult.value();
    return queue;
}

inline std::expected<std::vector<Image>,
                     Error>
CreateSwapchainImages(const Context& context,
                      const VkSwapchainKHR swapchain,
                      const uint32_t imageCount)
{
    std::vector<Image> images(imageCount);
    std::vector<VkImage> baseImages(imageCount);
    uint32_t getCount = imageCount;
    vkGetSwapchainImagesKHR(context.Device,
                            swapchain,
                            &getCount,
                            baseImages.data());

    for (const auto& [index, image] : std::views::enumerate(images))
    {
        image.BaseImage = baseImages[index];
        VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image.BaseImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .subresourceRange =
                GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        };
        const auto result = vkCreateImageView(context.Device,
                                              &imageViewCreateInfo,
                                              nullptr,
                                              &image.ImageView);
        if (result != VK_SUCCESS)
        {
            return std::unexpected(Error::eSwapchainCreateFailed);
        }
    }

    return images;
}

inline std::expected<Swapchain,
                     Error>
CreateSwapchain(const Context& context,
                const Queue& queue,
                const Int2& dimensions)
{
    constexpr VkSurfaceFormatKHR surfaceFormat{
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    auto result =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.GPU,
                                                  context.Surface,
                                                  &surfaceCapabilities);
    if (result != VK_SUCCESS)
    {
        return std::unexpected(Error::eSwapchainCreateFailed);
    }
    constexpr auto preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.GPU,
                                              context.Surface,
                                              &count,
                                              nullptr);

    std::vector<VkPresentModeKHR> presentModes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.GPU,
                                              context.Surface,
                                              &count,
                                              presentModes.data());

    VkPresentModeKHR currentPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    const auto it =
        std::ranges::find_if(presentModes,
                             [&](VkPresentModeKHR presentMode)
                             { return presentMode == currentPresentMode; });

    VkPresentModeKHR presentMode = preferredPresentMode;
    if (it == presentModes.end())
    {
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    constexpr uint32_t imageCount = 2;
    const VkSwapchainCreateInfoKHR swapchainInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context.Surface,
        .minImageCount = imageCount,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = VkExtent2D(dimensions.x, dimensions.y),
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue.QueueIndex,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = true,
    };
    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(context.Device,
                                  &swapchainInfo,
                                  nullptr,
                                  &swapchain);
    if (result != VK_SUCCESS)
    {
        return std::unexpected(Error::eSwapchainCreateFailed);
    }

    const auto imageResult =
        CreateSwapchainImages(context, swapchain, imageCount);

    if (!imageResult)
    {
        return std::unexpected(Error::eSwapchainCreateFailed);
    }

    Swift::ImageCreateInfo depthImageInfo = {
        .Format = VK_FORMAT_D32_SFLOAT,
        .Extent = Swift::Int2(dimensions.x, dimensions.y),
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .Samples = VK_SAMPLE_COUNT_1_BIT,
        .MipLevels = 1,
        .ArrayLayers = 1,
    };
    const auto depthImageResult = CreateImage(context, depthImageInfo);
    if (!depthImageResult)
    {
        return std::unexpected(Error::eSwapchainCreateFailed);
    }

    Swapchain newSwapchain{
        .SwapChain = swapchain,
        .Dimensions = dimensions,
        .Images = imageResult.value(),
        .DepthImage = depthImageResult.value(),
        .CurrentImageIndex = 0,
    };

    return newSwapchain;
}

inline std::expected<void,
                     Error>
RecreateSwapchain(const Context& context,
                  const Queue& queue,
                  Swapchain& swapchain,
                  const Int2 dimensions)
{
    vkDeviceWaitIdle(context.Device);
    if (dimensions != swapchain.Dimensions)
    {
        for (const auto& image : swapchain.Images)
        {
            vkDestroyImageView(context.Device, image.ImageView, nullptr);
        }

        Vulkan::DestroyImage(context, swapchain.DepthImage);

        vkDestroySwapchainKHR(context.Device, swapchain.SwapChain, nullptr);

        const auto swapchainResult =
            CreateSwapchain(context, queue, dimensions);

        if (!swapchainResult)
        {
            return std::unexpected(swapchainResult.error());
        }
        swapchain = swapchainResult.value();
    }
    return {};
};

inline std::expected<VkSemaphore,
                     Error>
CreateSemaphore(const VkDevice device)
{
    constexpr VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore semaphore;
    const auto result =
        vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore);
    return CheckResult(result, semaphore, Error::eSemaphoreCreateFailed);
}

inline std::expected<VkFence,
                     Error>
CreateFence(const VkDevice device,
            const bool signaled)
{
    const VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u};
    VkFence fence;
    const auto result =
        vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
    return CheckResult(result, fence, Error::eFenceCreateFailed);
}

inline std::expected<VkCommandPool,
                     Error>
CreateCommandPool(const VkDevice device,
                  const uint32_t queueFamilyIndex)
{
    const VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VkCommandPool commandPool;
    const auto result = vkCreateCommandPool(device,
                                            &commandPoolCreateInfo,
                                            nullptr,
                                            &commandPool);
    return CheckResult(result, commandPool, Error::eCommandPoolCreateFailed);
}

inline std::expected<VkCommandBuffer,
                     Error>
CreateCommandBuffer(const VkDevice device,
                    const VkCommandPool commandPool)
{
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer;
    const auto result = vkAllocateCommandBuffers(device,
                                                 &commandBufferAllocateInfo,
                                                 &commandBuffer);
    return CheckResult(result,
                       commandBuffer,
                       Error::eCommandBufferCreateFailed);
}

inline std::expected<Command,
                     Error>
CreateCommand(const VkDevice device,
              const uint32_t queueFamilyIndex)
{
    const auto poolResult = CreateCommandPool(device, queueFamilyIndex);
    if (!poolResult.has_value())
    {
        return std::unexpected(Error::eCommandPoolCreateFailed);
    }
    const auto bufferResult = CreateCommandBuffer(device, poolResult.value());
    if (!bufferResult.has_value())
    {
        return std::unexpected(Error::eCommandBufferCreateFailed);
    }
    return Command{poolResult.value(), bufferResult.value()};
}

inline std::expected<FrameData,
                     Error>
CreateFrameData(const VkDevice device)
{
    FrameData frameData{};

    auto semaphoreResult = CreateSemaphore(device);
    if (!semaphoreResult)
    {
        return std::unexpected(Error::eSemaphoreCreateFailed);
    }
    frameData.ImageAvailable = semaphoreResult.value();

    semaphoreResult = CreateSemaphore(device);
    if (!semaphoreResult)
    {
        return std::unexpected(Error::eSemaphoreCreateFailed);
    }
    frameData.RenderFinished = semaphoreResult.value();

    const auto fenceResult = CreateFence(device, true);
    if (!fenceResult)
    {
        return std::unexpected(Error::eFenceCreateFailed);
    }
    frameData.Fence = fenceResult.value();

    const auto commandResult = CreateCommand(device, 0);
    if (!commandResult)
    {
        return std::unexpected(commandResult.error());
    }
    frameData.Command = commandResult.value();

    return frameData;
}

inline std::expected<VkDescriptorSetLayout,
                     Error>
CreateDescriptorSetLayout(const VkDevice device)
{
    std::array bindings{
        VkDescriptorSetLayoutBinding{
            .binding = Constants::SamplerBinding,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = Constants::MaxSamplerDescriptors,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = Constants::UniformBinding,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = Constants::MaxUniformDescriptors,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = Constants::StorageBinding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = Constants::MaxStorageDescriptors,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = Constants::ImageBinding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = Constants::MaxImageDescriptors,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
    };

    constexpr VkDescriptorBindingFlags flags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    constexpr std::array bindingFlags{
        flags,
        flags,
        flags,
        flags,
    };
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindCreateInfo{
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data(),
    };

    const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindCreateInfo,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayout descriptorSetLayout;
    const auto result = vkCreateDescriptorSetLayout(device,
                                                    &setLayoutCreateInfo,
                                                    nullptr,
                                                    &descriptorSetLayout);
    return CheckResult(result,
                       descriptorSetLayout,
                       Error::eDescriptorCreateFailed);
}

inline std::expected<VkDescriptorPool,
                     Error>
CreateDescriptorPool(const VkDevice device)
{
    constexpr std::array poolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = Constants::MaxSamplerDescriptors,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = Constants::MaxUniformDescriptors,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = Constants::MaxStorageDescriptors,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = Constants::MaxImageDescriptors,
        },
    };
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    VkDescriptorPool descriptorPool;
    const auto result = vkCreateDescriptorPool(device,
                                               &descriptorPoolCreateInfo,
                                               nullptr,
                                               &descriptorPool);
    return CheckResult(result, descriptorPool, Error::eDescriptorCreateFailed);
};

inline std::expected<VkDescriptorSet,
                     Error>
CreateDescriptorSet(const VkDevice device,
                    const VkDescriptorSetLayout setLayout,
                    const VkDescriptorPool descriptorPool)
{
    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &setLayout,
    };
    VkDescriptorSet descriptorSet;
    const auto result = vkAllocateDescriptorSets(device,
                                                 &descriptorSetAllocateInfo,
                                                 &descriptorSet);
    return CheckResult(result, descriptorSet, Error::eDescriptorCreateFailed);
}

inline std::expected<Descriptor,
                     Error>
CreateDescriptor(const VkDevice device)
{
    Descriptor descriptor{};
    const auto setLayoutResult = CreateDescriptorSetLayout(device);
    if (!setLayoutResult.has_value())
    {
        return std::unexpected(setLayoutResult.error());
    }
    descriptor.Layout = setLayoutResult.value();

    const auto poolResult = CreateDescriptorPool(device);
    if (!poolResult.has_value())
    {
        return std::unexpected(Error::eDescriptorCreateFailed);
    }
    descriptor.Pool = poolResult.value();

    const auto descriptorSetResult =
        CreateDescriptorSet(device, descriptor.Layout, descriptor.Pool);
    if (!descriptorSetResult)
    {
        return std::unexpected(descriptorSetResult.error());
    }
    descriptor.Set = descriptorSetResult.value();

    return descriptor;
}

inline std::expected<VkPipelineLayout,
                     Error>
CreatePipelineLayout(const Context& context,
                     VkDescriptorSetLayout descriptorSetLayout)
{
    constexpr VkPushConstantRange pushConstants{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128,
    };
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants,
    };
    VkPipelineLayout pipelineLayout;
    const auto result = vkCreatePipelineLayout(context.Device,
                                               &pipelineLayoutCreateInfo,
                                               nullptr,
                                               &pipelineLayout);
    return CheckResult(result,
                       pipelineLayout,
                       Error::ePipelineLayoutCreateFailed);
}

inline std::expected<VkPipeline,
                     Error>
CreateGraphicsPipeline(
    const VkDevice device,
    const VkPipelineLayout pipelineLayout,
    const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages,
    const GraphicsShaderCreateInfo& createInfo)
{
    VkPipelineRenderingCreateInfo renderCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount =
            static_cast<uint32_t>(createInfo.ColorFormats.size()),
        .pColorAttachmentFormats = createInfo.ColorFormats.data(),
        .depthAttachmentFormat = createInfo.DepthFormat,
    };
    constexpr auto vertexInputCreateInfo = VkPipelineVertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    constexpr auto inputAssemblyCreateInfo =
        VkPipelineInputAssemblyStateCreateInfo{
            .sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

    constexpr auto viewport = VkViewport{
        .width = 1280.f,
        .height = 720.f,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    constexpr auto scissor = VkRect2D{.offset = {0, 0}, .extent = {1280, 720}};
    const auto viewportCreateInfo = VkPipelineViewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    const auto rasterizerCreateInfo =
        VkPipelineRasterizationStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = createInfo.PolygonMode,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.f,
        };
    const auto multisampleCreateInfo = VkPipelineMultisampleStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = createInfo.Samples,
    };
    constexpr auto depthStencilCreateInfo =
        VkPipelineDepthStencilStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
    constexpr auto colorBlendAttachmentState =
        VkPipelineColorBlendAttachmentState{
            .blendEnable = true,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
    std::vector colorBlendAttachments{createInfo.ColorFormats.size(),
                                      colorBlendAttachmentState};
    const auto colorBlendStateCreateInfo = VkPipelineColorBlendStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
        .pAttachments = colorBlendAttachments.data(),
    };

    constexpr std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR,
                                       VK_DYNAMIC_STATE_LINE_WIDTH,
                                       VK_DYNAMIC_STATE_CULL_MODE,
                                       VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                       VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                                       VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                                       VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                                       VK_DYNAMIC_STATE_FRONT_FACE};

    const auto dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkPipeline pipeline;
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderCreateInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputCreateInfo,
        .pInputAssemblyState = &inputAssemblyCreateInfo,
        .pViewportState = &viewportCreateInfo,
        .pRasterizationState = &rasterizerCreateInfo,
        .pMultisampleState = &multisampleCreateInfo,
        .pDepthStencilState = &depthStencilCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = pipelineLayout,
    };
    const auto result = vkCreateGraphicsPipelines(device,
                                                  nullptr,
                                                  1,
                                                  &graphicsPipelineCreateInfo,
                                                  nullptr,
                                                  &pipeline);

    return CheckResult(result, pipeline, Error::ePipelineCreateFailed);
}

inline std::expected<VkPipeline,
                     Error>
CreateComputePipeline(const VkDevice device,
                      const VkPipelineLayout pipelineLayout,
                      const VkPipelineShaderStageCreateInfo& shaderStage)
{
    const VkComputePipelineCreateInfo computePipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStage,
        .layout = pipelineLayout,
    };
    VkPipeline pipeline;
    const auto result = vkCreateComputePipelines(device,
                                                 nullptr,
                                                 1,
                                                 &computePipelineCreateInfo,
                                                 nullptr,
                                                 &pipeline);
    return CheckResult(result, pipeline, Error::ePipelineCreateFailed);
}

inline std::expected<VkShaderModule,
                     Error>
CreateShaderModule(const VkDevice device,
                   const std::vector<char>& code)
{
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };
    VkShaderModule shaderModule;
    const auto result =
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    return CheckResult(result, shaderModule, Error::eShaderCreateFailed);
}

inline std::expected<ShaderInfo,
                     Error>
CreateShader(const VkDevice device,
             const std::vector<char>& code,
             const ShaderStage shaderStage)
{
    VkShaderStageFlagBits stageFlags = {};
    switch (shaderStage)
    {
    case ShaderStage::eVertex:
        stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderStage::eFragment:
        stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderStage::eCompute:
        stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    }
    auto shaderResult = Vulkan::CreateShaderModule(device, code);
    if (!shaderResult)
    {
        return std::unexpected(shaderResult.error());
    }
    const VkPipelineShaderStageCreateInfo shaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stageFlags,
        .module = shaderResult.value(),
        .pName = "main",
    };

    ShaderInfo shader{
        .ShaderModule = shaderResult.value(),
        .ShaderStage = shaderStageInfo,
    };
    return shader;
}

inline std::expected<VkSampler,
                     Error>
CreateSampler(const Context& context,
              const SamplerCreateInfo& createInfo)
{
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(context.GPU, &properties);
        const VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = createInfo.MagFilter,
            .minFilter = createInfo.MinFilter,
            .mipmapMode = createInfo.MipmapMode,
            .addressModeU = createInfo.AddressModeU,
            .addressModeV = createInfo.AddressModeV,
            .addressModeW = createInfo.AddressModeW,
            .anisotropyEnable = true,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .minLod = 0,
            .maxLod = 16,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = false,

        };
        VkSampler sampler;
        const auto result =
            vkCreateSampler(context.Device, &samplerInfo, nullptr, &sampler);
        return CheckResult(result, sampler, Error::eSamplerCreateFailed);
    }
}

inline std::expected<std::tuple<VkImage,
                                VmaAllocation>,
                     Error>
CreateBaseImage(const Context& context,
                const ImageCreateInfo& createInfo)
{
    const VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = createInfo.ArrayLayers == 6
                     ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                     : VkImageCreateFlags(),
        .imageType = VK_IMAGE_TYPE_2D,
        .format = createInfo.Format,
        .extent = VkExtent3D(createInfo.Extent.x, createInfo.Extent.y, 1),
        .mipLevels = createInfo.MipLevels,
        .arrayLayers = createInfo.ArrayLayers,
        .samples = createInfo.Samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = createInfo.Usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    constexpr VmaAllocationCreateInfo allocCreateInfo{
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VkImage image;
    VmaAllocation allocation;
    const auto result = vmaCreateImage(context.Allocator,
                                       &imageCreateInfo,
                                       &allocCreateInfo,
                                       &image,
                                       &allocation,
                                       nullptr);
    return CheckResult(result,
                       std::tuple{image, allocation},
                       Error::eImageCreateFailed);
}

inline std::expected<VkImageView,
                     Error>
CreateImageView(const VkDevice device,
                const VkImage image,
                const ImageCreateInfo& createInfo)
{
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    const auto layers = createInfo.ArrayLayers;

    if (createInfo.Format == VK_FORMAT_D16_UNORM ||
        createInfo.Format == VK_FORMAT_D32_SFLOAT)
    {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    const VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = layers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
        .format = createInfo.Format,
        .subresourceRange = GetImageSubresourceRange(aspectMask,
                                                     0,
                                                     createInfo.MipLevels,
                                                     0,
                                                     layers),
    };
    VkImageView imageView;
    const auto result =
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
    return CheckResult(result, imageView, Error::eImageCreateFailed);
}

inline std::expected<Image,
                     Error>
CreateImage(const Context& context,
            const ImageCreateInfo& createInfo)
{
    Image image;
    const auto imageResult = CreateBaseImage(context, createInfo);
    if (!imageResult)
    {
        return std::unexpected(imageResult.error());
    }
    std::tie(image.BaseImage, image.Allocation) = imageResult.value();

    const auto imageViewResult =
        CreateImageView(context.Device, image.BaseImage, createInfo);
    if (!imageViewResult)
    {
        return std::unexpected(imageViewResult.error());
    }
    image.ImageView = imageViewResult.value();
    image.Extent = createInfo.Extent;
    image.Sampler = createInfo.Sampler;
    image.MipLevels = createInfo.MipLevels;
    image.ArrayLayers = createInfo.ArrayLayers;
    return image;
}

inline void DestroyImage(const Swift::Context& context, Image& image)
{
    vmaDestroyImage(context.Allocator, image.BaseImage, image.Allocation);
    vkDestroyImageView(context.Device, image.ImageView, nullptr);
    image.BaseImage = nullptr;
    image.ImageView = nullptr;
    image.Allocation = nullptr;
}

inline std::expected<Swift::Buffer,
                     Error>
CreateBuffer(const Swift::Context& context,
             const BufferCreateInfo& createInfo)
{
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    switch (createInfo.Usage)
    {
    case BufferUsage::eUniform:
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case BufferUsage::eStorage:
        usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case BufferUsage::eIndex:
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        break;
    case BufferUsage::eIndirect:
        usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        break;
    default:
        break;
    }
    const VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = createInfo.Size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    constexpr VmaAllocationCreateInfo allocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    Buffer buffer;
    const auto result = vmaCreateBuffer(context.Allocator,
                                        &bufferCreateInfo,
                                        &allocCreateInfo,
                                        &buffer.BaseBuffer,
                                        &buffer.Allocation,
                                        &buffer.AllocationInfo);
    return CheckResult(result, buffer, Error::eBufferCreateFailed);
}

inline std::expected<void*,
                     Error>
MapBuffer(const Swift::Context& context,
          const Swift::Buffer& buffer)
{
    void* data;
    const auto result =
        vmaMapMemory(context.Allocator, buffer.Allocation, &data);
    return CheckResult(result, data, Error::eBufferMapFailed);
}

inline void UnmapBuffer(const Swift::Context& context,
                        const Swift::Buffer& buffer)
{
    vmaUnmapMemory(context.Allocator, buffer.Allocation);
}

inline void DestroyBuffer(const VmaAllocator& allocator,
                          Buffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.BaseBuffer, buffer.Allocation);
    buffer.Allocation = nullptr;
    buffer.BaseBuffer = nullptr;
}