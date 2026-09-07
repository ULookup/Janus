#include "Asset/Loader/AudioClipLoader.h"
#include <fstream>
#include <string_view>

namespace Janus
{
Result<AudioClip> AudioClipLoader::Parse(std::span<const u8> bytes)
{
    using Output = Result<AudioClip>;
    const auto invalid = []
    {
        return Output::Failure(ErrorCode::AssetDecodeFailed,
                               "AudioClip requires bounded RIFF/WAVE PCM16 mono/stereo, 8000-96000 "
                               "Hz, <=120 seconds.");
    };
    if (bytes.size() < 12 || bytes.size() > MaxFileBytes)
        return invalid();
    const auto tag = [&](usize offset)
    { return std::string_view(reinterpret_cast<const char*>(bytes.data() + offset), 4); };
    const auto word = [&](usize offset, usize size)
    {
        u32 value = 0;
        for (usize i = 0; i < size; ++i)
            value |= static_cast<u32>(bytes[offset + i]) << (i * 8);
        return value;
    };
    if (tag(0) != "RIFF" || tag(8) != "WAVE" || word(4, 4) != bytes.size() - 8)
        return invalid();
    AudioClip clip;
    bool formatFound = false, dataFound = false;
    std::span<const u8> data;
    for (usize offset = 12; offset < bytes.size();)
    {
        if (bytes.size() - offset < 8)
            return invalid();
        const auto kind = tag(offset);
        const usize size = word(offset + 4, 4);
        offset += 8;
        if (size > bytes.size() - offset || (size & 1) > bytes.size() - offset - size)
            return invalid();
        if (kind == "fmt ")
        {
            if (formatFound || size < 16 || word(offset, 2) != 1 || word(offset + 14, 2) != 16)
                return invalid();
            formatFound = true;
            clip.channels = word(offset + 2, 2);
            clip.sampleRate = word(offset + 4, 4);
            if ((clip.channels != 1 && clip.channels != 2) || clip.sampleRate < 8000 ||
                clip.sampleRate > 96000 || word(offset + 12, 2) != clip.channels * 2 ||
                word(offset + 8, 4) != clip.sampleRate * clip.channels * 2)
                return invalid();
        }
        else if (kind == "data")
        {
            if (dataFound)
                return invalid();
            dataFound = true;
            data = bytes.subspan(offset, size);
        }
        offset += size + (size & 1);
    }
    if (!formatFound || !dataFound || data.empty() || data.size() % (clip.channels * 2) != 0 ||
        data.size() / (clip.channels * 2) > static_cast<usize>(clip.sampleRate) * 120)
        return invalid();
    clip.samples.reserve(data.size() / 2);
    for (usize i = 0; i < data.size(); i += 2)
    {
        const i32 sample = static_cast<i32>(data[i]) | (static_cast<i32>(data[i + 1]) << 8);
        clip.samples.push_back(static_cast<f32>(sample >= 32768 ? sample - 65536 : sample) / 32768);
    }
    return Output::Success(std::move(clip));
}

Result<AudioClip> AudioClipLoader::Load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return Result<AudioClip>::Failure(ErrorCode::FileReadFailed,
                                          "Cannot open AudioClip: " + path.generic_string());
    const auto size = file.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(MaxFileBytes))
        return Result<AudioClip>::Failure(ErrorCode::AssetDecodeFailed,
                                          "AudioClip exceeds 32 MiB.");
    std::vector<u8> bytes(static_cast<usize>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size())) ||
        file.peek() != std::char_traits<char>::eof())
        return Result<AudioClip>::Failure(ErrorCode::FileReadFailed,
                                          "AudioClip changed or could not be read: " +
                                              path.generic_string());
    return Parse(bytes);
}
} // namespace Janus
