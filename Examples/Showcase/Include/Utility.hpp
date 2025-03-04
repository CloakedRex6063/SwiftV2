#pragma once
#include "vector"
#include "fstream"
#include "string"

namespace Utility
{
    inline std::vector<char> ReadBinaryFile(const std::string_view filePath)
    {
        std::ifstream file(filePath.data(), std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            //Log::Error("File {} with full path {} was not found!", path, fullPath);
            return {};
        }
        const std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (file.read(buffer.data(), size)) return buffer;
        return {};
    };
}
