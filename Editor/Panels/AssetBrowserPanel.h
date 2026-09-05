#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Error/Error.h"

#include <optional>

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

    [[nodiscard]] std::optional<Error> Draw();

private:
    EditorContext& m_Context;
    EditorActions& m_Actions;
    AssetHandle m_SelectedAsset;
    int m_TypeFilter = 0;
};

} // namespace Janus::Editor
