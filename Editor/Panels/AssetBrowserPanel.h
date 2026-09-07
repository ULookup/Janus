#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Error/Error.h"

#include <array>
#include <optional>
#include <string>

namespace Janus::Editor
{

class EditorActions;
struct EditorContext;

class AssetBrowserPanel final
{
public:
    AssetBrowserPanel(
        EditorContext& context,
        EditorActions& actions) noexcept;

    [[nodiscard]] std::optional<Error> DrawContents();

private:
    EditorContext& m_Context;
    EditorActions& m_Actions;
    AssetHandle m_SelectedAsset;
    int m_TypeFilter = 0;
    std::array<char, 128> m_Search{};
    std::string m_Folder;
    bool m_Grid = true;
};

} // namespace Janus::Editor
