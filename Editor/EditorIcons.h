#pragma once

#include "Icons/EditorIconCatalog.h"
#include <imgui.h>
#include <string_view>

namespace Janus::Editor
{
ImVec4 IconColor(Icon icon);
void DrawIcon(Icon icon, ImVec2 origin, float size);
void DrawItemIcon(Icon icon, float inset = 0.0f);
void DrawTitleIcon(Icon icon);
void IconText(Icon icon, const char* text);
void DrawEllipsizedText(ImDrawList* draw, ImVec2 pos, float width, ImU32 color,
                        std::string_view text);
bool IconButton(Icon icon, const char* label, ImVec2 size = {});
bool IconOnlyButton(Icon icon, const char* label);
bool IconTab(Icon icon, const char* label, ImGuiTabItemFlags flags = 0);
bool IconHeader(Icon icon, const char* label, ImGuiTreeNodeFlags flags = 0);
bool IconSelectable(Icon icon, const char* label, bool selected);
Icon ComponentIcon(std::string_view name);
Icon AssetIcon(std::string_view type);
} // namespace Janus::Editor
