cmake_minimum_required(VERSION 3.15...4.0)
project(
  MixiD
  VERSION 0.1.6
  LANGUAGES CXX)

add_definitions(-DVERSION_MIXID="${MixiD_VERSION}")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)
include(FindPkgConfig)

set(IMGUI_RELEASE_DIR "" CACHE STRING
    "Directory containing the imgui release files")
if((IS_DIRECTORY ${IMGUI_RELEASE_DIR}))
    set(IMGUI_EXTERNAL ${IMGUI_RELEASE_DIR})
else()
    include(FetchContent)
    FetchContent_Declare(
        imgui_external
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.4.tar.gz
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(imgui_external)
    set(IMGUI_EXTERNAL ${imgui_external_SOURCE_DIR})
endif()
add_library(
    imgui
    ${IMGUI_EXTERNAL}/imgui.cpp
    ${IMGUI_EXTERNAL}/imgui_draw.cpp
    ${IMGUI_EXTERNAL}/imgui_tables.cpp
    ${IMGUI_EXTERNAL}/imgui_widgets.cpp
    ${IMGUI_EXTERNAL}/backends/imgui_impl_glfw.cpp
    ${IMGUI_EXTERNAL}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_EXTERNAL})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

find_package(LibUSB REQUIRED)
include_directories(${LIBUSB_INCLUDE_DIR})

# convert data into code
include(CMake/bin2h.cmake)
file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS Assets/*)
file(REMOVE "Raw_Assets.h")

foreach(file ${ASSET_FILES})
	get_filename_component(variableName ${file} NAME)
	message("   ${variableName}")
	list(LENGTH PLUGIN_ASSETLIST ASSET_COUNT)
	message("List size: ${ASSET_COUNT}")
	if (ASSET_COUNT EQUAL 0)
		message("Overwrite")
		bin2h(SOURCE_FILE ${file} HEADER_FILE "Raw_Assets.h" VARIABLE_NAME ${variableName} NULL_TERMINATE)
	else()
		bin2h(SOURCE_FILE ${file} HEADER_FILE "Raw_Assets.h" VARIABLE_NAME ${variableName} APPEND NULL_TERMINATE)
	endif()
	list(APPEND PLUGIN_ASSETLIST ${NEWASSET})
	string(REPLACE ";" "," PLUGIN_ASSETLIST_STRING "${PLUGIN_ASSETLIST}")
endforeach()
message("Converted to C header: ${ASSET_FILES}")

# Adding something we can run - Output name matches target name
add_executable(MixiD main.cpp imgui-knobs.cpp imgui-custom.cpp)

# Make sure you link your targets with this command. It can also link libraries and
# even flags, so linking a target that does not exist will not give a configure-time error.
target_link_libraries(MixiD PRIVATE imgui glfw ${OPENGL_LIBRARIES} ${LIBUSB_LIBRARIES})
