#include "Image.hpp"
#include "Swift.hpp"
#include "dds.hpp"
#include "fstream"

namespace
{
    uint32_t CalculateImageOffset(const VkFormat format, const Swift::Int2 extent)
    {
        uint32_t blockWidth, blockHeight, offset = 0;
        switch (format)
        {
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                blockWidth = (extent.x + 3) / 4;
                blockHeight = (extent.y + 3) / 4;
                offset = blockWidth * blockHeight * 16;
                break;
            default:
                offset += extent.x * extent.y * 4;
                break;
        }
        return offset;
    }

    std::vector<Swift::BufferImageCopy> GetBufferImageCopies(const dds::Header &header)
    {
        std::vector<Swift::BufferImageCopy> copyRegions;
        copyRegions.reserve(header.MipLevels() * header.ArraySize());
        uint32_t offset = 0;
        for (uint32_t arrayLayer = 0; arrayLayer < header.ArraySize(); arrayLayer++)
        {
            for (uint32_t mipLevel = 0; mipLevel < header.MipLevels(); mipLevel++)
            {
                const auto extent = Swift::Int2(header.Width() >> mipLevel, header.Height() >> mipLevel);
                Swift::BufferImageCopy bufferImageCopy
                {
                    .BufferOffset = offset,
                    .MipLevel = mipLevel,
                    .ArrayLayer = arrayLayer,
                    .Extent = Swift::Int2(header.Width() >> mipLevel, header.Height() >> mipLevel),
                };
                copyRegions.emplace_back(bufferImageCopy);
                offset += CalculateImageOffset(header.GetVulkanFormat(), extent);
            }
        }
        return copyRegions;
    }
}

namespace Swift
{
    std::expected<ImageHandle, Swift::Image::Error> Image::LoadImage(const std::string &path)
    {
        Swift::BeginTransfer();
        const dds::Header header = dds::ReadHeader(path);

        Swift::BufferCreateInfo createInfo{
            .Usage = Swift::BufferUsage::eStorage,
            .Size = header.DataSize(),
        };
        const auto bufferResult = Swift::CreateBuffer(createInfo);
        if (!bufferResult)
        {
            return std::unexpected(Image::Error::eBufferCreationFailed);
        }
        const auto buffer = bufferResult.value();

        auto mappedResult = Swift::MapBuffer(buffer);
        if (!mappedResult)
        {
            return std::unexpected(Image::Error::eMapFailed);
        }

        std::ifstream stream;
        stream.open(path, std::ios::binary);
        if (!stream.is_open())
        {
            return std::unexpected(Image::Error::eFileNotFound);
        }
        stream.seekg(header.DataOffset(), std::ios::beg);
        stream.read(static_cast<char *>(mappedResult.value()), header.DataSize());

        Swift::UnmapBuffer(buffer);

        ImageCreateInfo imageCreateInfo
        {
            .Format = header.GetVulkanFormat(),
            .Extent = Int2(header.Width(), header.Height()),
            .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Samples = VK_SAMPLE_COUNT_1_BIT,
            .MipLevels = header.MipLevels(),
            .ArrayLayers = header.ArraySize(),
        };

        const auto imageResult = Swift::CreateImage(imageCreateInfo);
        if (!imageResult)
        {
            return std::unexpected(Image::Error::eImageCreationFailed);
        }
        const auto image = imageResult.value();

        const auto bufferImageCopies = GetBufferImageCopies(header);

        Swift::CopyBufferToImage(buffer, image, bufferImageCopies);

        Swift::EndTransfer();

        Swift::DestroyBuffer(buffer);

        return image;
    }
}
