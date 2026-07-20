cmake_minimum_required(VERSION 3.20)
project(pickup_elite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Ob2 /DNDEBUG /GL /Gy /Gw" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /LTCG /OPT:REF /OPT:ICF" CACHE STRING "" FORCE)
endif()

find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm miniaudio)

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

add_executable(pickup_elite
    src/main.cpp
    src/window.cpp
    src/vk_core.cpp
    src/renderer.cpp
    src/noise.cpp
    src/terrain_shapes.cpp
    src/roads.cpp
    src/landscape.cpp
    src/terrain.cpp
    src/vehicle.cpp
    src/vecfont.cpp
    src/audio.cpp
)

add_dependencies(pickup_elite shaders)

target_include_directories(pickup_elite PRIVATE src ${miniaudio_SOURCE_DIR})
target_link_libraries(pickup_elite PRIVATE Vulkan::Vulkan glm::glm user32)

target_compile_definitions(pickup_elite PRIVATE
    SHADER_DIR="${SHADER_BINARY_DIR}/"
    RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources/"
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(pickup_elite PRIVATE /W4)
    target_compile_options(pickup_elite PRIVATE $<$<CONFIG:Release>:/GR->)
endif()

if(WIN32)
    add_custom_command(TARGET pickup_elite POST_BUILD
        COMMAND "$<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/pack_release.cmd>" "$<TARGET_FILE:pickup_elite>"
        VERBATIM
    )
endif()
