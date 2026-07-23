#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/format.h>

namespace ImGui
{
	namespace fonts {
		extern ImFont* pDefault; // default font used everywhere implicitly
		extern ImFont* pHeading;
	}

	#define IM_COL32_SET_ALPHA(col, alpha) \
		(((col) & ~(0xFF << IM_COL32_A_SHIFT)) | (((ImU32)(alpha) & 0xFF) << IM_COL32_A_SHIFT))

	constexpr float k_BORDER_WIDTH = 1.0f;
	constexpr float k_TITLE_FONT_SIZE = 32.0f;

	// Group panels taken from https://github.com/ocornut/imgui/issues/1496#issuecomment-655048353
	// Licensed under CC0 license as per https://github.com/ocornut/imgui/issues/1496#issuecomment-1287772456

	void BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0.0f, 0.0f));
	void EndGroupPanel();

	inline void TextTitle(const char* fmt, ...) {
		PushFont(fonts::pHeading, k_TITLE_FONT_SIZE);
		va_list args;
		va_start(args, fmt);
		TextV(fmt, args);
		va_end(args);
		PopFont();
	}

	inline void TextHeading(const char* fmt, ...) {
		PushFont(fonts::pHeading);
		va_list args;
		va_start(args, fmt);
		TextV(fmt, args);
		va_end(args);
		PopFont();
	}

	// @TODO: remove begin child crap? it seems more problematic here

	inline void TextWithWidth(const char* text, float width) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
	}

	inline void HeadingWithWidth(const char* text, float width) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		TextHeading("%s", text);
		ImGui::PopTextWrapPos();
	}

	inline void TextDisabledWithWidth(const char* text, float width) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextDisabled("%s", text);
		ImGui::PopTextWrapPos();
	}

	inline void TextWrappedDisabledWithWidth(const char* text, float width) {
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextWrapped("%s", text);
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}

	inline void TextWrappedDisabled(const char* text) {
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		ImGui::TextWrapped("%s", text);
		ImGui::PopStyleColor();
	}

	inline void TextWrappedColored(const ImVec4& col, const char* text) {
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		PushStyleColor(ImGuiCol_Text, col);
		ImGui::TextWrapped("%s", text);
		PopStyleColor();
		ImGui::PopStyleColor();
	}

	inline bool IconButton(const char* icon, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0) {
		if (!icon || icon[0] == '\0') return ImGui::ButtonEx(label, size_arg, flags);
		if (!label || label[0] == '\0') return ImGui::ButtonEx(icon, size_arg, flags);
		
		return ImGui::ButtonEx(fmt::format("{} {}", icon, label).c_str(), size_arg, flags);
	}

	inline bool IconButtonPrimary(const char* icon, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0) {
		// @TODO: styling
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		bool res = ImGui::IconButton(icon, label, size_arg, flags);
		ImGui::PopStyleColor();
		return res;
	}

	inline bool IconButtonSecondary(const char* icon, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0) {
		// @TODO: styling
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		bool res = ImGui::IconButton(icon, label, size_arg, flags);
		ImGui::PopStyleColor();
		return res;
	}

	inline bool IconButtonDanger(const char* icon, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags flags = 0) {
		// @TODO: styling
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		bool res = ImGui::IconButton(icon, label, size_arg, flags);
		ImGui::PopStyleColor();
		return res;
	}

	inline bool BeginCard(const char* str_id, const ImVec2& size_arg = ImVec2(0.0f, 0.0f), ImGuiChildFlags child_flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.17f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.33f, 0.42f, 0.40f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, k_BORDER_WIDTH);
		return ImGui::BeginChild(str_id, size_arg, child_flags);
	}

	inline void EndCard() {
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(2);;
	}

	inline bool CheckboxWithDescription(const char* title, bool* v, const char* desc) {
		ImGui::BeginGroup();
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
		ImGui::Spacing();
		return changed;
	}

	inline bool RadioButtonWithDescription(const char* title, bool v, const char* desc) {
		ImGui::BeginGroup();
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
		ImGui::Spacing();
		return changed;
	}

	// @TODO: advanced settings helpers for setting colours etc

	inline float GetWindowContentRegionWidth() { return ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x; }
}