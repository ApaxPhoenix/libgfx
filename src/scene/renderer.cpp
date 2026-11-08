#include "scene/renderer.hpp"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>

#include "shaders/vertex.h"
#include "shaders/fragment.h"

namespace gfx::scene {
    struct Renderer::State {
        struct Buffer {
            vk::UniqueDeviceMemory memory;
            vk::UniqueBuffer buffer;
        };

        struct Attachment {
            vk::UniqueDeviceMemory memory;
            vk::UniqueImage image;
            vk::UniqueImageView view;
        };

        static constexpr auto color = vk::Format::eR8G8B8A8Unorm;
        static constexpr auto depth = vk::Format::eD16Unorm;
        static constexpr auto host = vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent;

        vk::UniqueInstance instance;
        vk::PhysicalDevice physical;
        vk::UniqueDevice device;
        vk::Queue queue;
        vk::UniqueCommandPool pool;
        vk::UniqueRenderPass pass;
        vk::UniquePipelineLayout layout;
        vk::UniquePipeline pipeline;
        std::mutex mutex;

        State() {
            constexpr auto portability = "VK_KHR_portability_subset";
            const vk::ApplicationInfo application("libgfx", 0, nullptr, 0, VK_API_VERSION_1_1);
            vk::InstanceCreateInfo configuration;
            configuration.setPApplicationInfo(&application);
            const char *enumeration = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            if (has(vk::enumerateInstanceExtensionProperties(), enumeration)) {
                configuration.setEnabledExtensionCount(1).setPpEnabledExtensionNames(&enumeration);
                configuration.setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR);
            }
            instance = vk::createInstanceUnique(configuration);

            std::uint32_t family = 0;
            for (const auto candidate : instance->enumeratePhysicalDevices()) {
                if (candidate.getProperties().apiVersion < VK_API_VERSION_1_1) continue;
                const auto queues = candidate.getQueueFamilyProperties();
                const auto graphics = std::ranges::find_if(queues, [](const auto &properties) {
                    return properties.queueCount && (properties.queueFlags & vk::QueueFlagBits::eGraphics);
                });
                if (graphics == queues.end()) continue;
                physical = candidate;
                family = static_cast<std::uint32_t>(graphics - queues.begin());
                break;
            }
            if (!physical) throw std::runtime_error("no Vulkan 1.1 graphics device");

            const float priority = 1.0f;
            const vk::DeviceQueueCreateInfo queues({}, family, 1, &priority);
            vk::DeviceCreateInfo settings;
            settings.setQueueCreateInfos(queues);
            if (has(physical.enumerateDeviceExtensionProperties(), portability))
                settings.setEnabledExtensionCount(1).setPpEnabledExtensionNames(&portability);
            device = physical.createDeviceUnique(settings);
            queue = device->getQueue(family, 0);
            pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo(
                vk::CommandPoolCreateFlagBits::eTransient, family));
            compose();
        }

        static bool has(const std::vector<vk::ExtensionProperties> &extensions, std::string_view name) {
            return std::ranges::any_of(extensions, [&](const auto &extension) {
                return name == extension.extensionName.data();
            });
        }

        void compose() {
            const std::array attachments{
                vk::AttachmentDescription({}, color, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal),
                vk::AttachmentDescription({}, depth, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
                    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal)
            };
            const vk::AttachmentReference output(0, vk::ImageLayout::eColorAttachmentOptimal);
            const vk::AttachmentReference distance(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            vk::SubpassDescription subpass;
            subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                   .setColorAttachments(output).setPDepthStencilAttachment(&distance);

            const std::array dependencies{
                vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    {}, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite),
                vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead)
            };
            pass = device->createRenderPassUnique(vk::RenderPassCreateInfo()
                .setAttachments(attachments).setSubpasses(subpass).setDependencies(dependencies));

            const vk::PushConstantRange transform(vk::ShaderStageFlagBits::eVertex, 0, sizeof(Matrix));
            layout = device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo().setPushConstantRanges(transform));

            const auto vertex = device->createShaderModuleUnique(vk::ShaderModuleCreateInfo().setCode(::vertex));
            const auto fragment = device->createShaderModuleUnique(vk::ShaderModuleCreateInfo().setCode(::fragment));
            const std::array stages{
                vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertex, "main"),
                vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragment, "main")
            };
            const vk::VertexInputBindingDescription binding(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
            const std::array attributes{
                vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, color))
            };
            const auto vertices = vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(binding).setVertexAttributeDescriptions(attributes);
            const auto assembly = vk::PipelineInputAssemblyStateCreateInfo()
                .setTopology(vk::PrimitiveTopology::eTriangleList);
            const auto viewport = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);
            const auto raster = vk::PipelineRasterizationStateCreateInfo()
                .setPolygonMode(vk::PolygonMode::eFill).setLineWidth(1.0f);
            const auto samples = vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(vk::SampleCountFlagBits::e1);
            const auto testing = vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(vk::CompareOp::eLess);
            const auto channels = vk::PipelineColorBlendAttachmentState().setColorWriteMask(
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
            const auto blend = vk::PipelineColorBlendStateCreateInfo().setAttachments(channels);
            const std::array states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
            const auto dynamic = vk::PipelineDynamicStateCreateInfo().setDynamicStates(states);

            const auto configuration = vk::GraphicsPipelineCreateInfo()
                .setStages(stages).setPVertexInputState(&vertices).setPInputAssemblyState(&assembly)
                .setPViewportState(&viewport).setPRasterizationState(&raster).setPMultisampleState(&samples)
                .setPDepthStencilState(&testing).setPColorBlendState(&blend).setPDynamicState(&dynamic)
                .setLayout(*layout).setRenderPass(*pass);
            pipeline = std::move(device->createGraphicsPipelineUnique({}, configuration).value);
        }

        vk::UniqueDeviceMemory allocate(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags flags) {
            const auto properties = physical.getMemoryProperties();
            for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
                if ((requirements.memoryTypeBits & (1u << index)) &&
                    (properties.memoryTypes[index].propertyFlags & flags) == flags)
                    return device->allocateMemoryUnique(vk::MemoryAllocateInfo(requirements.size, index));
            }
            throw std::runtime_error("no compatible Vulkan memory type");
        }

        Buffer buffer(vk::BufferUsageFlags usage, std::size_t size, const void *data = nullptr) {
            Buffer result;
            result.buffer = device->createBufferUnique(vk::BufferCreateInfo({}, size, usage));
            result.memory = allocate(device->getBufferMemoryRequirements(*result.buffer), host);
            device->bindBufferMemory(*result.buffer, *result.memory, 0);
            if (data) {
                std::memcpy(device->mapMemory(*result.memory, 0, size), data, size);
                device->unmapMemory(*result.memory);
            }
            return result;
        }

        Attachment attachment(Extent size, vk::Format format, vk::ImageUsageFlags usage,
                              vk::ImageAspectFlags aspect) {
            Attachment result;
            result.image = device->createImageUnique(vk::ImageCreateInfo({}, vk::ImageType::e2D, format,
                vk::Extent3D(size.width, size.height, 1), 1, 1, vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal, usage));
            result.memory = allocate(device->getImageMemoryRequirements(*result.image),
                                     vk::MemoryPropertyFlagBits::eDeviceLocal);
            device->bindImageMemory(*result.image, *result.memory, 0);
            result.view = device->createImageViewUnique(vk::ImageViewCreateInfo({}, *result.image,
                vk::ImageViewType::e2D, format, {}, vk::ImageSubresourceRange(aspect, 0, 1, 0, 1)));
            return result;
        }
    };

    Renderer::Renderer() : state(std::make_unique<State>()) {}
    Renderer::~Renderer() = default;
    Renderer::Renderer(Renderer &&) noexcept = default;
    Renderer &Renderer::operator=(Renderer &&) noexcept = default;

    Image Renderer::render(const Scene &scene, Extent size, const Matrix &camera) {
        const auto &limits = state->physical.getProperties().limits;
        if (size.width == 0 || size.height == 0 ||
            size.width > limits.maxFramebufferWidth || size.height > limits.maxFramebufferHeight ||
            size.width > limits.maxImageDimension2D || size.height > limits.maxImageDimension2D)
            throw std::invalid_argument("invalid render dimensions");
        const auto count = scene.indices.empty() ? scene.vertices.size() : scene.indices.size();
        if (count % 3 || count > std::numeric_limits<std::uint32_t>::max() ||
            std::ranges::any_of(scene.indices, [&](auto index) { return index >= scene.vertices.size(); }))
            throw std::invalid_argument("invalid triangle mesh");

        const std::scoped_lock lock(state->mutex);
        Image image(size);
        auto &device = *state->device;
        const auto color = state->attachment(size, State::color,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
            vk::ImageAspectFlagBits::eColor);
        const auto depth = state->attachment(size, State::depth,
            vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageAspectFlagBits::eDepth);
        const std::array views{*color.view, *depth.view};
        const auto framebuffer = device.createFramebufferUnique(vk::FramebufferCreateInfo()
            .setRenderPass(*state->pass).setAttachments(views).setWidth(size.width).setHeight(size.height).setLayers(1));
        const auto staging = state->buffer(vk::BufferUsageFlagBits::eTransferDst, image.pixels().size());

        State::Buffer vertices;
        State::Buffer indices;
        if (count) {
            const std::span points(scene.vertices);
            vertices = state->buffer(vk::BufferUsageFlagBits::eVertexBuffer, points.size_bytes(), points.data());
            if (!scene.indices.empty()) {
                const std::span elements(scene.indices);
                indices = state->buffer(vk::BufferUsageFlagBits::eIndexBuffer, elements.size_bytes(), elements.data());
            }
        }

        const auto commands = device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(
            *state->pool, vk::CommandBufferLevel::ePrimary, 1));
        const auto command = *commands.front();
        const auto background = scene.background;
        const std::array clear{
            vk::ClearValue(vk::ClearColorValue(std::array{background.red, background.green, background.blue, background.alpha})),
            vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0))
        };
        const vk::Rect2D area({0, 0}, {size.width, size.height});
        command.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        command.beginRenderPass(vk::RenderPassBeginInfo().setRenderPass(*state->pass)
            .setFramebuffer(*framebuffer).setRenderArea(area).setClearValues(clear), vk::SubpassContents::eInline);
        if (count) {
            const auto width = static_cast<float>(size.width);
            const auto height = static_cast<float>(size.height);
            command.bindPipeline(vk::PipelineBindPoint::eGraphics, *state->pipeline);
            command.setViewport(0, vk::Viewport(0.0f, height, width, -height, 0.0f, 1.0f));
            command.setScissor(0, area);
            command.pushConstants(*state->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Matrix), camera.data());
            const vk::DeviceSize offset = 0;
            command.bindVertexBuffers(0, *vertices.buffer, offset);
            if (indices.buffer) {
                command.bindIndexBuffer(*indices.buffer, 0, vk::IndexType::eUint32);
                command.drawIndexed(static_cast<std::uint32_t>(count), 1, 0, 0, 0);
            } else {
                command.draw(static_cast<std::uint32_t>(count), 1, 0, 0);
            }
        }
        command.endRenderPass();

        const auto copy = vk::BufferImageCopy()
            .setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setImageExtent(vk::Extent3D(size.width, size.height, 1));
        command.copyImageToBuffer(*color.image, vk::ImageLayout::eTransferSrcOptimal, *staging.buffer, copy);
        const auto barrier = vk::MemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eHostRead);
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
                                {}, barrier, {}, {});
        command.end();

        const auto fence = device.createFenceUnique(vk::FenceCreateInfo());
        state->queue.submit(vk::SubmitInfo().setCommandBuffers(command), *fence);
        if (device.waitForFences(*fence, true, std::numeric_limits<std::uint64_t>::max()) != vk::Result::eSuccess)
            throw std::runtime_error("Vulkan frame did not complete");

        std::memcpy(image.pixels().data(),
                    device.mapMemory(*staging.memory, 0, image.pixels().size()), image.pixels().size());
        device.unmapMemory(*staging.memory);
        return image;
    }
}
