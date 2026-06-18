#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
	// Group panels taken from https://github.com/ocornut/imgui/issues/1496#issuecomment-655048353
	// Licensed under CC0 license as per https://github.com/ocornut/imgui/issues/1496#issuecomment-1287772456

	void BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0.0f, 0.0f));
	void EndGroupPanel();

	inline void TextHeading(const char* fmt, ...) {
		// @TODO: push pop font
		// PushStyleColor(ImGuiCol_Text, col);
		va_list args;
		va_start(args, fmt);
		TextV(fmt, args);
		va_end(args);
		// PopStyleColor();
	}

	// @TODO: remove begin child crap? it seems more problematic here

	inline void TextWithWidth(const char* label, const char* text, float width) {
		ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
		ImGui::TextUnformatted(text);
		ImGui::EndChild();
	}

	inline void HeadingWithWidth(const char* label, const char* text, float width) {
		ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
		ImGui::TextHeading("%s", text);
		ImGui::EndChild();
	}

	inline void TextDisabledWithWidth(const char* label, const char* text, float width) {
		ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing()));
		ImGui::TextDisabled("%s", text);
		ImGui::EndChild();
	}

	inline void TextWrappedDisabledWithWidth(const char* label, const char* text, float width) {
		(void)label;
		ImGuiContext& g = *GImGui;
		ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextWrapped("%s", text);
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
	}

	inline float GetWindowContentRegionWidth() { return ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x; }
}