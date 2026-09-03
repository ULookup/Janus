#include "Asset/Loader/LuaScriptSourceLoader.h"

#include "Core/FileSystem/FileSystem.h"

namespace Janus
{

Result<std::string> LuaScriptSourceLoader::Load(
    const std::filesystem::path& path)
{
    return FileSystem::ReadText(path);
}

} // namespace Janus
