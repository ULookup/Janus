#include "EditorIcons.h"

#include <algorithm>
#include <string>

namespace Janus::Editor
{
#include "Icons/EditorIconGeometry.inl"

namespace
{
std::string SpacedLabel(const char* label)
{
    // Stable IDs preserve existing tab/header state while making room for the icon.
    return std::string("      ") + label +
           (std::string_view(label).find("###") == std::string_view::npos
                ? std::string("###") + label
                : std::string{});
}
} // namespace

void DrawItemIcon(Icon icon, float inset)
{
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float size = ImGui::GetFontSize();
    DrawIcon(icon, {min.x + inset, min.y + (max.y - min.y - size) * 0.5f}, size);
}

void IconText(Icon icon, const char* text)
{
    const float size = ImGui::GetFontSize();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({size, size});
    DrawIcon(icon, pos, size);
    ImGui::SameLine();
    ImGui::TextUnformatted(text);
}

void DrawTitleIcon(Icon icon)
{
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    auto* draw = ImGui::GetWindowDrawList();
    // Title bars sit outside ImGui's content clip rectangle.
    draw->PushClipRect(pos, {pos.x + size.x, pos.y + size.y}, false);
    DrawIcon(icon, {pos.x + ImGui::GetFontSize() * 0.5f, pos.y + ImGui::GetStyle().FramePadding.y},
             ImGui::GetFontSize());
    draw->PopClipRect();
}

void DrawEllipsizedText(ImDrawList* draw, ImVec2 pos, float width, ImU32 color,
                        std::string_view text)
{
    if (width <= 0 || text.empty())
        return;
    const auto lineEnd = text.find_first_of("\r\n");
    const bool multiline = lineEnd != std::string_view::npos;
    if (multiline)
        text = text.substr(0, lineEnd);
    const char* remaining = nullptr;
    ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), width, 0, text.data(),
                                    text.data() + text.size(), &remaining);
    const bool clipped = remaining < text.data() + text.size();
    std::string label;
    if (clipped || multiline)
    {
        const float dots = ImGui::CalcTextSize("...").x;
        // Font measurement stops on a UTF-8 boundary and only scans the visible prefix.
        ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), std::max(1.0f, width - dots), 0,
                                        text.data(), text.data() + text.size(), &remaining);
        label.assign(text.data(), remaining);
        label += "...";
    }
    else
        label.assign(text);
    draw->PushClipRect(pos, {pos.x + width, pos.y + ImGui::GetFontSize()}, true);
    draw->AddText(pos, color, label.c_str());
    draw->PopClipRect();
}

bool IconButton(Icon icon, const char* label, ImVec2 size)
{
    const float iconSize = ImGui::GetFontSize();
    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    const std::string visible = std::string(label).substr(0, std::string_view(label).find("##"));
    const ImVec2 textSize = ImGui::CalcTextSize(visible.c_str());
    const float content = iconSize + gap + textSize.x;
    if (size.x == 0)
        size.x = content + ImGui::GetStyle().FramePadding.x * 2;
    const bool clicked = ImGui::Button((std::string("###") + label).c_str(), size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float x = min.x + std::max(0.0f, (max.x - min.x - content) * 0.5f);
    const float y = min.y + (max.y - min.y - iconSize) * 0.5f;
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(min, max, true);
    DrawIcon(icon, {x, y}, iconSize);
    draw->AddText({x + iconSize + gap, min.y + (max.y - min.y - textSize.y) * 0.5f},
                  ImGui::GetColorU32(ImGuiCol_Text), visible.c_str());
    draw->PopClipRect();
    return clicked;
}

bool IconTab(Icon icon, const char* label, ImGuiTabItemFlags flags)
{
    const bool open = ImGui::BeginTabItem(SpacedLabel(label).c_str(), nullptr, flags);
    DrawItemIcon(icon, ImGui::GetStyle().FramePadding.x);
    return open;
}

bool IconOnlyButton(Icon icon, const char* label)
{
    const float side = ImGui::GetFrameHeight();
    const bool clicked = ImGui::Button((std::string("###") + label).c_str(), {side, side});
    DrawItemIcon(icon, (side - ImGui::GetFontSize()) * 0.5f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s",
                          std::string(label).substr(0, std::string_view(label).find("##")).c_str());
    return clicked;
}

bool IconHeader(Icon icon, const char* label, ImGuiTreeNodeFlags flags)
{
    const bool open = ImGui::CollapsingHeader(SpacedLabel(label).c_str(), flags);
    DrawItemIcon(icon, ImGui::GetFontSize() * 1.5f + ImGui::GetStyle().FramePadding.x);
    return open;
}

bool IconSelectable(Icon icon, const char* label, bool selected)
{
    const bool clicked = ImGui::Selectable(SpacedLabel(label).c_str(), selected);
    DrawItemIcon(icon, ImGui::GetStyle().ItemSpacing.x * 0.5f);
    return clicked;
}

Icon ComponentIcon(std::string_view name)
{
    if (name == "Transform")
        return Icon::Transform;
    if (name == "SpriteRenderer" || name == "Image")
        return Icon::Texture;
    if (name == "LuaScript")
        return Icon::Script;
    if (name == "Animator")
        return Icon::Animation;
    if (name == "AudioSource")
        return Icon::Audio;
    if (name == "Text")
        return Icon::Font;
    if (name == "Camera")
        return Icon::Camera;
    if (name == "Collider2D")
        return Icon::Collider;
    if (name == "RigidBody2D")
        return Icon::Physics;
    if (name == "Canvas" || name == "UIRect" || name == "Panel" || name == "Button")
        return Icon::Canvas;
    return Icon::Entity;
}

Icon AssetIcon(std::string_view type)
{
    if (type == "texture")
        return Icon::Texture;
    if (type == "lua-script" || type == "shader_source")
        return Icon::Script;
    if (type == "font")
        return Icon::Font;
    if (type == "audio-clip")
        return Icon::Audio;
    if (type == "animation-clip")
        return Icon::Animation;
    if (type == "prefab")
        return Icon::Prefab;
    return Icon::Entity;
}
} // namespace Janus::Editor
