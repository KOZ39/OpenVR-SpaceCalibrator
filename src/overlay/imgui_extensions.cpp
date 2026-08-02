#include "imgui_extensions.h"

#include "imgui_internal.h"
#include <fmt/format.h>

namespace ImGui::fonts {
    ImFont* pDefault = nullptr;
    ImFont* pHeading = nullptr;
}

static ImVector<ImRect> s_GroupPanelLabelStack;

void ImGui::BeginGroupPanel(const char* name, const ImVec2& size)
{
    ImGui::BeginGroup();

    auto cursorPos = ImGui::GetCursorScreenPos();
    auto itemSpacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();
    ImGui::BeginGroup();

    ImVec2 effectiveSize = size;
    if (size.x < 0.0f)
        effectiveSize.x = ImGui::GetContentRegionAvail().x;
    else
        effectiveSize.x = size.x;
    ImGui::Dummy(ImVec2(effectiveSize.x, 0.0f));

    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextUnformatted(name);
    auto labelMin = ImGui::GetItemRectMin();
    auto labelMax = ImGui::GetItemRectMax();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(0.0, frameHeight + itemSpacing.y));
    ImGui::BeginGroup();

    //ImGui::GetWindowDrawList()->AddRect(labelMin, labelMax, IM_COL32(255, 0, 255, 255));

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x -= frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x -= frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x -= frameHeight;

    auto itemWidth = ImGui::CalcItemWidth();
    ImGui::PushItemWidth(ImMax(0.0f, itemWidth - frameHeight));

    s_GroupPanelLabelStack.push_back(ImRect(labelMin, labelMax));
}

void ImGui::EndGroupPanel()
{
    ImGui::PopItemWidth();

    auto itemSpacing = ImGui::GetStyle().ItemSpacing;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();

    ImGui::EndGroup();

    //ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 64), 4.0f);

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::Dummy(ImVec2(0.0, frameHeight - frameHeight * 0.5f - itemSpacing.y));

    ImGui::EndGroup();

    auto itemMin = ImGui::GetItemRectMin();
    auto itemMax = ImGui::GetItemRectMax();
    //ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax, IM_COL32(255, 0, 0, 64), 4.0f);

    auto labelRect = s_GroupPanelLabelStack.back();
    s_GroupPanelLabelStack.pop_back();

    ImVec2 halfFrame = ImVec2(frameHeight * 0.25f, frameHeight) * 0.5f;
    ImRect frameRect = ImRect(itemMin + halfFrame, itemMax - ImVec2(halfFrame.x, 0.0f));
    labelRect.Min.x -= itemSpacing.x;
    labelRect.Max.x += itemSpacing.x;
    for (int i = 0; i < 4; ++i)
    {
        switch (i)
        {
            // left half-plane
        case 0: ImGui::PushClipRect(ImVec2(-FLT_MAX, -FLT_MAX), ImVec2(labelRect.Min.x, FLT_MAX), true); break;
            // right half-plane
        case 1: ImGui::PushClipRect(ImVec2(labelRect.Max.x, -FLT_MAX), ImVec2(FLT_MAX, FLT_MAX), true); break;
            // top
        case 2: ImGui::PushClipRect(ImVec2(labelRect.Min.x, -FLT_MAX), ImVec2(labelRect.Max.x, labelRect.Min.y), true); break;
            // bottom
        case 3: ImGui::PushClipRect(ImVec2(labelRect.Min.x, labelRect.Max.y), ImVec2(labelRect.Max.x, FLT_MAX), true); break;
        }

        ImGui::GetWindowDrawList()->AddRect(
            frameRect.Min, frameRect.Max,
            ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)),
            halfFrame.x);

        ImGui::PopClipRect();
    }

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x += frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x += frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x += frameHeight;

    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    ImGui::EndGroup();
}

bool ImGui::CheckboxWithDescription(const char* title, bool* v, const char* desc) {
    ImGui::BeginGroup();

    ImVec2 startPos = ImGui::GetCursorPos();
    float contentWidth = ImGui::GetContentRegionAvail().x;
    
    bool changed = ImGui::Checkbox(fmt::format("##{}", title).c_str(), v);
    
    ImGui::SameLine();
    float yOffsetToTop = ImGui::GetStyle().FramePadding.y +
        (ImGui::GetFrameHeight() - (ImGui::GetStyle().FramePadding.y * 2.0f) - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - yOffsetToTop);
    
    ImGui::BeginGroup();
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y + 2.0f);
    ImGui::TextWrappedDisabled(desc);
    ImGui::EndGroup();
    ImGui::EndGroup();

    ImVec2 size = ImGui::GetItemRectSize();
    ImVec2 endPos = ImGui::GetCursorPos();
    float height = endPos.y - startPos.y;
    ImGui::SetCursorPos(startPos);
    if (ImGui::InvisibleButton(fmt::format("##hit_{}", title).c_str(), size)) {
        *v = !(*v);
        changed = true;
    }
    
    ImGui::SetCursorPos(endPos);
    ImGui::Spacing();
    return changed;
}

bool ImGui::RadioButtonWithDescription(const char* title, bool v, const char* desc) {
    ImGui::BeginGroup();

    ImVec2 startPos = ImGui::GetCursorPos();
    float contentWidth = ImGui::GetContentRegionAvail().x;

    bool changed = ImGui::RadioButton(fmt::format("##{}", title).c_str(), v);
    ImGui::SameLine();
    float yOffsetToTop = ImGui::GetStyle().FramePadding.y +
        (ImGui::GetFrameHeight() - (ImGui::GetStyle().FramePadding.y * 2.0f) - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - yOffsetToTop);

    ImGui::BeginGroup();
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y + 2.0f);
    ImGui::TextWrappedDisabled(desc);
    ImGui::EndGroup();
    ImGui::EndGroup();

    ImVec2 size = ImGui::GetItemRectSize();
    ImVec2 endPos = ImGui::GetCursorPos();
    float height = endPos.y - startPos.y;
    ImGui::SetCursorPos(startPos);
    if (ImGui::InvisibleButton(fmt::format("##hit_{}", title).c_str(), size)) {
        changed = true;
    }

    ImGui::SetCursorPos(endPos);
    ImGui::Spacing();
    return changed;
}

void ImGui::PillText(const char* text, const ImVec4& bgColor, const ImVec4& textColor) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(text);

    const ImVec2 padding = ImVec2(k_PILL_PADDING_X, k_PILL_PADDING_Y);
    const ImVec2 textSize = ImGui::CalcTextSize(text, NULL, true);

    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size = ImVec2(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return;

    const float rounding = size.y * 0.5f;
    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bgColor), rounding);

    const ImVec2 textPos = ImVec2(bb.Min.x + padding.x, bb.Min.y + padding.y);
    window->DrawList->AddText(textPos, ImGui::ColorConvertFloat4ToU32(textColor), text);
}