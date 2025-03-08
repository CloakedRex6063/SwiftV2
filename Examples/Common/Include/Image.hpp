#pragma once
#include "SwiftStructs.hpp"
#include "expected"

namespace Swift::Image
{
    enum class Error
    {
        eFileNotFound,
        eBufferCreationFailed,
        eImageCreationFailed,
        eMapFailed,
    };
   std::expected<ImageHandle, Swift::Image::Error> LoadImage(const std::string& path);
}
