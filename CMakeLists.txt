cmake_minimum_required(VERSION 3.16)
project(NodeDisplaySystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type option
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")

# cmake .. -DCMAKE_BUILD_TYPE=Debug ninja && NodeDisplaySystem.exe
# cmake .. -DCMAKE_BUILD_TYPE=Debug
# Find packages
find_package(OpenGL REQUIRED)

# Include directories
include_directories(imnodes)
include_directories(imgui)
include_directories(imgui/backends)
include_directories(glfw/include)

# ImGui and ImNodes source files
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imnodes/imnodes.cpp
)

# GLFW build
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(glfw)

# Source files
set(SOURCES
    src/main.cpp
    src/NodeEditor.cpp
    src/AIModel.cpp
    src/SyncManager.cpp
    ${IMGUI_SOURCES}
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Headless execution test (does not depend on GLFW/ImGui or SyncManager)
add_executable(ai_execution_test src/ai_execution_test.cpp src/AIModel.cpp)

# Link libraries
target_link_libraries(${PROJECT_NAME} glfw OpenGL::GL)
