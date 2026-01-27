#include "imgui-custom.h"

namespace ImGui {

bool VFaderScalar(const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);

	const ImVec2 label_size = CalcTextSize(label, NULL, true);
	ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
	ImRect bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

	ItemSize(bb, style.FramePadding.y);
	if (!ItemAdd(frame_bb, id))
		return false;

	// Default format string when passing NULL
	if (format == NULL)
		format = DataTypeGetInfo(data_type)->PrintFmt;

	const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
	const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
	if (clicked || g.NavActivateId == id)
	{
		if (clicked)
			SetKeyOwner(ImGuiKey_MouseLeft, id);
		SetActiveID(id, window);
		SetFocusID(id, window);
		FocusWindow(window);
		g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
	}

	// Draw frame
	//const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	float contrast = 0.1;
	const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg, 1.0f + contrast);
	const ImU32 frame_col_after = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg, 1.0f - contrast);
	RenderNavCursor(frame_bb, id);
	//RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

	// Slider behavior
	ImRect grab_bb;
	const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags | ImGuiSliderFlags_Vertical, &grab_bb);
	if (value_changed)
		MarkItemEdited(id);

	float thickness = 0.4;
	ImRect draw_bb = frame_bb;
	if (thickness != 1.0f)
	{
		float shrink_amount = (float)(int)((frame_bb.Max.x - frame_bb.Min.x) * 0.5f * (1.0f - thickness));
		draw_bb.Min.x += shrink_amount;
		draw_bb.Max.x -= shrink_amount;
	}

	// Render track
	window->DrawList->AddRectFilled(ImVec2(draw_bb.Min.x, grab_bb.Max.y - (grab_bb.Max.y - grab_bb.Min.y) * 0.65f), draw_bb.Max, frame_col, style.FrameRounding, ImDrawFlags_RoundCornersBottom);
	window->DrawList->AddRectFilled(draw_bb.Min, ImVec2(draw_bb.Max.x, grab_bb.Min.y + (grab_bb.Max.y - grab_bb.Min.y) * 0.35f), frame_col_after, style.FrameRounding, ImDrawFlags_RoundCornersTop);

	// Render grab
	ImRect modgrab_bb = grab_bb;
	modgrab_bb.Min.y -= 20;
	modgrab_bb.Max.y += 20;
	grab_bb.Min.y += 4;
	grab_bb.Max.y -= 4;
	if (grab_bb.Max.y > grab_bb.Min.y) {
		window->DrawList->AddRectFilled(modgrab_bb.Min, modgrab_bb.Max, GetColorU32(ImVec4(0,0,0.1,255)), style.GrabRounding);
		window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);
	}

	// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
	// For the vertical slider we allow centered text to overlap the frame padding
	//char value_buf[64];
	//const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
	//RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.0f));
	//if (label_size.x > 0.0f)
	//    RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

	return value_changed;
}

bool VFaderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return VFaderScalar(label, size, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool VFaderInt(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
	return VFaderScalar(label, size, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}


// Similar to `ImGui::ProgressBar`, but with a horizontal/vertical switch.
// The value text doesn't follow the value like `ImGui::ProgressBar`.
// Here it's simply displayed in the middle of the bar.
// Horizontal labels are placed to the right of the rect.
// Vertical labels are placed below the rect.
void ValueBar(const char *label, const float value, const ImVec2 &size, const float min_value, const float max_value, const ValueBarFlags flags) {
	const bool is_h = !(flags & ValueBarFlags_Vertical);
	const ImGuiStyle &style = GetStyle();
	const auto &draw_list = GetWindowDrawList();
	const ImVec2 &cursor_pos = GetCursorScreenPos();
	const float fraction = (value - min_value) / max_value;
	const float frame_height = GetFrameHeight();
	const ImVec2 &label_size = strlen(label) > 0 ? ImVec2{CalcTextSize(label).x, frame_height} : ImVec2{0, 0};
	const ImVec2 &rect_size = is_h ? ImVec2{CalcItemWidth(), frame_height} : size;//ImVec2{GetFontSize() * 2, size.y - label_size.y};
	const ImVec2 &rect_start = cursor_pos + ImVec2{is_h ? 0 : fmax(0.0f, (label_size.x - rect_size.x) / 2), 0};

	draw_list->AddRectFilled(rect_start, rect_start + rect_size, GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
	draw_list->AddRectFilled(
		rect_start + ImVec2{0, is_h ? 0 : (1 - fraction) * rect_size.y},
		rect_start + rect_size * ImVec2{is_h ? fraction : 1, 1},
		GetColorU32(ImGuiCol_Text),
		style.FrameRounding, is_h ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersBottom
	);
	//const std::string value_text = is_h ? std::format("{:.2f}", value) : std::format("{:.1f}", value);
	//draw_list->AddText(rect_start + (rect_size - CalcTextSize(value_text.c_str())) / 2, GetColorU32(ImGuiCol_Text), value_text.c_str());
	/*
	if (label) {
		draw_list->AddText(
			rect_start + ImVec2{is_h ? rect_size.x + style.ItemInnerSpacing.x : (rect_size.x - label_size.x) / 2, style.FramePadding.y + (is_h ? 0 : rect_size.y)},
			GetColorU32(ImGuiCol_Text), label);
	}*/
}

void StyleColorsMixiD(ImGuiStyle* dst)
{
	ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.04f, 0.04f, 0.04f, 0.94f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.29f, 0.29f, 0.29f, 0.54f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.29f, 0.38f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.29f, 0.48f, 0.67f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.44f, 0.44f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.18f, 0.40f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.88f, 0.88f, 0.98f, 1.00f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_InputTextCursor]        = colors[ImGuiCol_Text];
	colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
	colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
	colors[ImGuiCol_TabSelected]            = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
	colors[ImGuiCol_TabSelectedOverline]    = colors[ImGuiCol_HeaderActive];
	colors[ImGuiCol_TabDimmed]              = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
	colors[ImGuiCol_TabDimmedSelected]      = ImLerp(colors[ImGuiCol_TabSelected],  colors[ImGuiCol_TitleBg], 0.40f);
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextLink]               = colors[ImGuiCol_HeaderActive];
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_TreeLines]              = colors[ImGuiCol_Border];
	colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_UnsavedMarker]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_NavCursor]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
};
}