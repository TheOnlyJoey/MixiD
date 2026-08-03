// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#include "imgui-custom.h"
#include "imgui-knobs.h"
//#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <vector>
#include <string>
#include <iostream>
#include "driver.h"
#include "tray.h"

#include "Raw_Assets.h"

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static int driver_indicator = 0;
static bool connected = false;
static bool tray_active = false;
static bool force_quit = false;
std::vector<float> levels = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static std::vector <float> bar_value;
static std::vector <bool> phase_value;
static std::vector <bool> master_bools = {false,false,false,false,false,false};

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static GLFWwindow* window = nullptr;
static const char* glsl_version = nullptr;
static float main_scale = 1.0f;
static bool want_hide = false;

static void window_close_callback(GLFWwindow* win)
{
	// only flag it here, the window is torn down from the main loop:
	// destroying a window inside its own callback is not safe
	if (tray_active && !force_quit) {
		glfwSetWindowShouldClose(win, GLFW_FALSE);
		want_hide = true;
	}
}

// Hiding is a full window teardown rather than glfwHideWindow(): on Wayland a
// re-shown window does not reliably get a configured surface back, and the
// first buffer swap then blocks forever waiting for a frame callback.
static void window_open()
{
	if (window)
		return;
#ifdef GLFW_WAYLAND_APP_ID
	glfwWindowHintString(GLFW_WAYLAND_APP_ID, "mixid");
#endif
#ifdef GLFW_X11_CLASS_NAME
	glfwWindowHintString(GLFW_X11_CLASS_NAME, "mixid");
	glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "mixid");
#endif
	window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "MixiD - Open Source Audient mixer for Linux", nullptr, nullptr);
	if (!window)
		return;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	glfwSetWindowCloseCallback(window, window_close_callback);
}

static void window_close()
{
	if (!window)
		return;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	glfwDestroyWindow(window);
	window = nullptr;
}

bool toggleButton(std::string name, ImVec2 size, std::vector<bool>::reference value) {
	int mastercol = 0;
	bool state = false;
	if (value){
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)); mastercol++;
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1,0.1,0.1,1.0)); mastercol++;
	}
	if (ImGui::Button(name.c_str(), size)) {state = true; value.flip();};
	
	if (mastercol > 0)
		ImGui::PopStyleColor(mastercol);

	return state;
}

// Main code
int main(int, char**)
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100 (WebGL 1.0)
	glsl_version ="#version 100";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
	// GL ES 3.0 + GLSL 300 es (WebGL 2.0)
	glsl_version ="#version 300 es";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
	// GL 3.2 + GLSL 150
	glsl_version ="#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
	// GL 3.0 + GLSL 130
	glsl_version ="#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
	int absX = 1280 * main_scale;
	int absY = 800 * main_scale;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsMixiD(&style);
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends (window and backends are torn down and
	// rebuilt together whenever MixiD hides to the tray)
	window_open();
	if (window == nullptr)
		return 1;
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif

	// Load Fonts
	style.FontSizeBase = 20.0f;
    ImFontConfig fc;
    fc.FontDataOwnedByAtlas = false;
    fc.OversampleH = 4;
    fc.OversampleV = 4;
    fc.PixelSnapH = true;
    fc.MergeMode = false;

	ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)GORDITA_REGULAR_OTF, GORDITA_REGULAR_OTF_SIZE, 20, &fc);
	ImFont* audiofont = io.Fonts->AddFontFromMemoryTTF((void*)FONTAUDIO_TTF, FONTAUDIO_TTF_SIZE, 20, &fc);
	io.Fonts->Build();

	// Our state
	bool show_routing = false;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	//Tray icon: while one is available, closing the window hides MixiD
	//instead of quitting (quit through Menu->Quit or the tray-less close)
	tray_active = tray_init() != 0;

	//Init all the device properties
	setup_devices();
	//Probe for known usb devices
	int _dev = device_probe();
	if (_dev >= 0) {
		driver_indicator = _dev;
	    bar_value.clear();
	}

	// Main loop
#ifdef __EMSCRIPTEN__
	// For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
	// You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
	io.IniFilename = nullptr;
	EMSCRIPTEN_MAINLOOP_BEGIN
#else
	while (!force_quit && (window == nullptr || !glfwWindowShouldClose(window)))
#endif
	{
		glfwPollEvents();
		int tray_action = tray_pump();
		if (tray_action == TRAY_TOGGLE) {
			if (window)
				want_hide = true;
			else
				window_open();
		}
		else if (tray_action == TRAY_QUIT) {
			force_quit = true;
		}
		else if (tray_action >= TRAY_MASTER) {
			int idx = tray_action - TRAY_MASTER;
			master_bools[idx] = !master_bools[idx];
			if (connected)
				set_bool_state(idx);
			tray_set_master(idx, master_bools[idx]);
		}
		if (want_hide) {
			want_hide = false;
			window_close();
		}
		if (window == nullptr)
		{
			// hidden in the tray: no rendering, just keep the tray responsive
			ImGui_ImplGlfw_Sleep(100);
			continue;
		}
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		int stylecount = 0;
		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f,0.5f)); stylecount++;
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f); stylecount++;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f); stylecount++;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f); stylecount++;

		{
			static float f = 0.0f;
			static int counter = 0;
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(absX*0.8,absY));
			ImGui::Begin("MixiD - Open Source Audient mixer for Linux", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Menu"))
				{
					if (ImGui::MenuItem("Driver Select")) {show_another_window = !show_another_window;};
					if (ImGui::MenuItem("Routing")) {show_routing = !show_routing;};
					ImGui::Separator();
					if (ImGui::MenuItem("Quit")) {force_quit = true; glfwSetWindowShouldClose(window, GLFW_TRUE);};
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}
			//TODO: Remove temp
			//TEMP "OFFLINE" UI
			bool test = true;

			if (connected || test) { //Main controls
				if (bar_value.size() == 0) {
					for (size_t i = 0; i < devices[driver_indicator].mic_inputs+devices[driver_indicator].digital_inputs; i++) {
						bar_value.push_back(0.0f);
						phase_value.push_back(false);
					}
				}
				ImGui::SetNextWindowPos(ImVec2(absX*0.1,absY*0.1));
				ImGui::SetNextWindowSize(ImVec2(absX*0.6, absY*0.8));
				ImGui::BeginChild("Faders", ImVec2(0,0),0,ImGuiWindowFlags_HorizontalScrollbar);
				ImVec2 ogpos = ImGui::GetCursorPos();
				int inputcounter = 0;
				for (size_t i = 0; i < (devices[driver_indicator].mic_inputs); i++) {
					ImGui::BeginGroup();
					if (i == 0)
						ImGui::SetCursorPosY(3);
					ImGui::Text("%s",(std::string("Mic ")+std::to_string(i+1)).c_str());
					ImGui::Dummy(ImVec2(0,24));
					if (ImGui::VFaderFloat((std::to_string(i)+"##vMic").c_str(), ImVec2(42, absY/1.8), &bar_value[inputcounter], 0.0f, 1.0f, "%.2f")) {
						if (connected)
							set_channel_volume(i, bar_value[inputcounter]);
					};
					ImGui::Dummy(ImVec2(0,32));
					ImGui::PushFont(audiofont, 32);
					//if (toggleButton("Dim", ImVec2((absX*0.2)*0.3, 40), master_bools[0])) { if (connected) {set_bool_state(0);}};
					if (toggleButton("###MicPhase"+std::to_string(i), ImVec2(0,0), phase_value[inputcounter])) {if (connected) set_phase_state(inputcounter);};
					inputcounter++;
					ImGui::PopFont();
					ImGui::EndGroup();
					ImGui::SameLine();
				}
				for (size_t i = 0; i < (devices[driver_indicator].digital_inputs); i++) {
					ImGui::BeginGroup();
					ImGui::Text("%s", (std::string("Digi ")+std::to_string(i+1)).c_str());
					ImGui::Dummy(ImVec2(0,24));
					if (ImGui::VFaderFloat((std::to_string(i)+"##vDigi").c_str(), ImVec2(42, absY/1.8), &bar_value[inputcounter], 0.0f, 1.0f, "%.2f")) {
						if (connected)
							set_channel_volume(i, bar_value[inputcounter]);
					};
					ImGui::Dummy(ImVec2(0,32));
					ImGui::PushFont(audiofont, 32);
					if (toggleButton("###DigiPhase"+std::to_string(i), ImVec2(0,0), phase_value[inputcounter])) {if (connected) set_phase_state(inputcounter);};
					inputcounter++;
					ImGui::PopFont();
					ImGui::EndGroup();
					if (i < (devices[driver_indicator].digital_inputs)-1)
						ImGui::SameLine();
				}

				ImGui::EndChild();
			}
			ImGui::SetCursorPosY(absY*0.965);
			ImGui::Text(VERSION_MIXID);
			//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(absX*0.8, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(absX*0.2, absY));
			ImGui::Begin("Monitor", nullptr, ImGuiWindowFlags_NoDecoration);
			ImGui::SeparatorText("Connection");
			
			ImGui::Text("  Selected Driver: %s", devices[driver_indicator].name.c_str());
			if (connected) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0,1.0,0.0,0.8));
				ImGui::Text("               Connected");
			}   
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0,0.0,0.0,0.8));
				ImGui::Text("               Disconnected");
			} ImGui::PopStyleColor();

			std::string name = "Connect";
			if (connected)
				name = "Disconnect";
			
			if (ImGui::Button(name.c_str(),ImVec2(ImGui::GetContentRegionAvail().x, 40))) {
				connected = !connected;
				if (connected) {
					if (!driver_init(devices[driver_indicator].usb_id)) {
						connected = false;
						ImGui::OpenPopup("No connection possible");
					};
				}
				else
					driver_shutdown();
			};
		   // Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if (ImGui::BeginPopupModal("No connection possible", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("USB Device can not be opened.");
				ImGui::Text("Make sure you have selected the correct driver and your usb permissions are correct.");
				ImGui::Text("This can either be done by adding the usb device to the udev rules,");
				ImGui::Text("or running MixiD with sudo permissions.");
				ImGui::Dummy(ImVec2(10,absY*0.1));
				ImGui::Separator();
				if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
				ImGui::SetItemDefaultFocus();
				ImGui::EndPopup();
			}

			ImGui::SeparatorText("Monitor");
			ImGui::Dummy(ImVec2(10,absY*0.10));
			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x/3.5,20)); ImGui::SameLine();
			if (ImGuiKnobs::Knob("Main LR", &levels[0], 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper)) {if (connected) set_speaker_volume(levels[0]);}
			ImGui::Dummy(ImVec2(10,absY*0.05));
			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x/3.5,20)); ImGui::SameLine();
			if (ImGuiKnobs::Knob("Phones", &levels[1], 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper)) {if (connected) set_hp_volume(levels[1]);}

			ImGui::SetCursorPosY(absY*0.88);
			ImGui::BeginGroup();
			if (toggleButton("Dim", ImVec2((absX*0.2)*0.3, 40), master_bools[0])) { if (connected) {set_bool_state(0);} tray_set_master(0, master_bools[0]);};
			ImGui::SameLine();
			if (toggleButton("Alt", ImVec2((absX*0.2)*0.3, 40), master_bools[1])) { if (connected) {set_bool_state(1);} tray_set_master(1, master_bools[1]);};
			ImGui::SameLine();
			if (toggleButton("Talk", ImVec2((absX*0.2)*0.3, 40), master_bools[2])) { if (connected) {set_bool_state(2);} tray_set_master(2, master_bools[2]);};
			ImGui::BeginGroup();
			if (toggleButton("Phase", ImVec2((absX*0.2)*0.3, 40), master_bools[3])) { if (connected) {set_bool_state(3);} tray_set_master(3, master_bools[3]);};
			ImGui::EndGroup();
			ImGui::SameLine();
			if (toggleButton("Mono", ImVec2((absX*0.2)*0.3, 40), master_bools[4])) { if (connected) {set_bool_state(4);} tray_set_master(4, master_bools[4]);};
			ImGui::EndGroup();


			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{
			ImGui::Begin("Driver Select", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	        
	        //const char* combo_preview_value = items[item_selected_idx];
	        if (ImGui::BeginCombo("Interface type", devices[driver_indicator].name.c_str()))
	        {
	            for (int n = 0; n < devices.size(); n++)
	            {
	                const bool is_selected = (driver_indicator == n);
	                if (ImGui::Selectable(devices[n].name.c_str(), is_selected)) {
	                    driver_indicator = n;
	                    bar_value.clear();
	                }

	                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
	                if (is_selected)
	                    ImGui::SetItemDefaultFocus();
	            }
	            ImGui::EndCombo();
	        }

			//ImGui::Combo("Interface type", &driver_indicator, items, IM_ARRAYSIZE(items));
			ImGui::Text("USB ID: 0x%04X", devices[driver_indicator].usb_id);
			ImGui::End();
		}
		if (show_routing)
		{
			ImGui::Begin("Routing", &show_routing);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			const char* column_names[] = { "","Main Mix", "Alt Spkr", "Cue A", "Cue B", "DAW Mix"};
			const int columns_count = IM_ARRAYSIZE(column_names);
			const int rows_count = 6;
			static bool bools[columns_count * rows_count] = {}; // Dummy storage selection storage

			static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_HighlightHoveredColumn;
			static ImGuiTableColumnFlags column_flags = ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed;
			static int frozen_cols = 1;
			static int frozen_rows = 2;
			static std::vector<int> row_selected = {0,0,0,0,0,0};

			if (ImGui::BeginTable("table_angled_headers", columns_count, table_flags, ImVec2(0.0f, 30 * 12)))
			{
				ImGui::TableSetupColumn(column_names[0], ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
				for (int n = 1; n < columns_count; n++)
					ImGui::TableSetupColumn(column_names[n], column_flags);
				ImGui::TableSetupScrollFreeze(frozen_cols, frozen_rows);

				ImGui::TableAngledHeadersRow(); // Draw angled headers for all columns with the ImGuiTableColumnFlags_AngledHeader flag.
				ImGui::TableHeadersRow();       // Draw remaining headers and allow access to context-menu and other functions.
				for (int row = 0; row < rows_count; row++)
				{
					ImGui::PushID(row);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("Channel %d", row+1);
					for (int column = 1; column < columns_count; column++)
						if (ImGui::TableSetColumnIndex(column))
						{
							ImGui::PushID(column);
							if (ImGui::RadioButton("", &row_selected[row], column)) {
								std::cout << row << "\n";
								if (connected)
									set_routing_value(row,column-1);
							};
							ImGui::PopID();
						}
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::End();
		}
		ImGui::PopStyleVar(stylecount);
		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_MAINLOOP_END;
#endif

	tray_shutdown();
	driver_shutdown(); //just in case disconnect
	// Cleanup (window_close() is a no-op when already hidden to the tray)
	window_close();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}
