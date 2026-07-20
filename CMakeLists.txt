cmake_minimum_required(VERSION 3.20)
project(pickup_elite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glm tinyobjloader)

# ---- shader compilation (glslc -> SPIR-V) ----
find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found; set VULKAN_SDK environment variable")
endif()

file(GLOB SHADER_SOURCES ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

set(COMPILED_SHADERS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND COMPILED_SHADERS ${SPIRV})
endforeach()

add_custom_target(shaders DEPENDS ${COMPILED_SHADERS})

# ---- main executable ----
add_executable(pickup_elite
    src/main.cpp
    src/vk_core.cpp
    src/renderer.cpp
    src/terrain.cpp
    src/vehicle.cpp
    src/vecfont.cpp
)

add_dependencies(pickup_elite shaders)

target_include_directories(pickup_elite PRIVATE src ${tinyobjloader_SOURCE_DIR})
target_link_libraries(pickup_elite PRIVATE Vulkan::Vulkan glfw glm::glm)

target_compile_definitions(pickup_elite PRIVATE
    SHADER_DIR="${SHADER_BINARY_DIR}/"
    RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources/"
)

if(MSVC)
    target_compile_options(pickup_elite PRIVATE /W4)
endif()
