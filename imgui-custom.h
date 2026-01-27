#ifndef _H_IMGUI_CUSTOM_H
#define _H_IMGUI_CUSTOM_H

#define IMGUI_DEFINE_PLACEMENT_NEW
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <string>

namespace ImGui {

	bool VFaderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool VFaderInt(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);

	enum ValueBarFlags_ {
		ValueBarFlags_None = 0,
		ValueBarFlags_Vertical = 1 << 0,
	};
	using ValueBarFlags = int;

	// Similar to `ImGui::ProgressBar`, but with a horizontal/vertical switch.
	// The value text doesn't follow the value like `ImGui::ProgressBar`.
	// Here it's simply displayed in the middle of the bar.
	// Horizontal labels are placed to the right of the rect.
	// Vertical labels are placed below the rect.
	void ValueBar(const char *label, const float value, const ImVec2 &size, const float min_value = 0, const float max_value = 1, const ValueBarFlags flags = ValueBarFlags_Vertical);

	void StyleColorsMixiD(ImGuiStyle* dst);
};

#endif