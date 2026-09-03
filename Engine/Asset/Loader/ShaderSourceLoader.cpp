#include "Asset/Loader/ShaderSourceLoader.h"

#include "Core/FileSystem/FileSystem.h"

namespace Janus
{

Result<std::string> ShaderSourceLoader::Load(
    const std::filesystem::path& path)
{
    return FileSystem::ReadText(path);
}

} // namespace Janus
