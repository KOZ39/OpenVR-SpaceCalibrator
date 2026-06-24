#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
	#define IM_COL32_SET_ALPHA(col, alpha) \
		(((col) & ~(0xFF << IM_COL32_A_SHIFT)) | (((ImU32)(alpha) & 0xFF) << IM_COL32_A_SHIFT))

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

	inline float GetWindowContentRegionWidth() { return ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x; }
}