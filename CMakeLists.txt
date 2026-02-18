
if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
  message(FATAL_ERROR "Missing COMMONLIB_SSE_FOLDER environment variable")
endif()

include(cmake/headerlist.cmake)
include(cmake/sourcelist.cmake)
include(cmake/lib/copyOutputs.cmake)
include(cmake/lib/automaticGameFolderOutput.cmake)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)
# Set your project name. This will be the name of your SKSE .dll file.
set(AUTHOR_NAME "Quantumyilmaz")
set(PRODUCT_NAME "Containerize")
project(${PRODUCT_NAME} VERSION 1.0.10.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

set(RESOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
set(RESOURCE_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.res)

add_custom_command(
    OUTPUT ${RESOURCE_OUTPUT}
    COMMAND rc /fo ${RESOURCE_OUTPUT} ${RESOURCE_FILE}
    DEPENDS ${RESOURCE_FILE}
)

# Add a custom target to ensure the resource file is generated
add_custom_target(Resource ALL DEPENDS ${RESOURCE_OUTPUT})

set(ENABLE_SKYRIM_SE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL " " FORCE)
set(BUILD_TESTS OFF CACHE BOOL " " FORCE)

add_subdirectory("$ENV{COMMONLIB_SSE_FOLDER}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)

find_path(CLIB_UTIL_INCLUDE_DIRS "ClibUtil/utils.hpp")
find_path(SKSE_MCP_INCLUDE_DIRS "SKSEMCP/utils.hpp")
find_path(SkyPromptAPI_INCLUDE_DIRS "SkyPrompt/API.hpp")
find_path(DAF_API_INCLUDE_DIRS "DynamicAnimationFramework/API.hpp")
find_package(yaml-cpp CONFIG REQUIRED)

file(
  GLOB_RECURSE
  sources
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  ${sources}
)

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${headers} ${sources}
)

set(BUILD_TESTS OFF)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
  ${PROJECT_NAME}
  AUTHOR AUTHOR_NAME
  SOURCES ${headers} ${sources}
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) # <--- use C++23 standard
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h) # <--- PCH.h is required!
target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CLIB_UTIL_INCLUDE_DIRS}
    ${SKSE_MCP_INCLUDE_DIRS}
    ${SkyPromptAPI_INCLUDE_DIRS}
    ${DAF_API_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp::yaml-cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE IS_HOST_PLUGIN)


# Ensure MSVC warning level4 is applied last (covers cl.exe and clang-cl)
if (MSVC)
 set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_WARNING_LEVEL4)
endif()

# Stricter warnings and unused checks per toolchain
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
 if (MSVC) # clang-cl
 target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall -Wextra -Wpedantic
 -Wunused-function
 -Wunused-member-function
 -Wunused-private-field
 -Wunused-variable
 -Wunused-parameter
 -Wunreachable-code
 )
 else() # clang (GNU-like)
 target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall -Wextra -Wpedantic
 -Wunused-function
 -Wunused-member-function
 -Wunused-private-field
 -Wunused-variable
 -Wunused-parameter
 -Wunreachable-code
 )
 endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
 target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall -Wextra -Wpedantic
 -Wunused
 -Wunused-function
 -Wunused-variable
 -Wunused-parameter
 -Wunused-but-set-variable
 -Wunused-but-set-parameter
 )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
 # Add MSVC code analysis optionally
 if (ENABLE_MSVC_ANALYZE)
 target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
 endif()
endif()

# Optional: clang-tidy integration focused on unused checks
if (ENABLE_CLANG_TIDY)
 find_program(CLANG_TIDY_EXE NAMES clang-tidy)
 if (CLANG_TIDY_EXE)
 # Use project .clang-tidy if present; restrict headers to project dirs
 set_target_properties(${PROJECT_NAME} PROPERTIES
 CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-header-filter=^(include|src)/"
 )
 else()
 message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found in PATH")
 endif()
endif()

set(wildlander_output false)
set(steam_owrt_output false)
set(steam_mods_output true)
set(lorerim_mods_output false)
set(lorerim3_mods_output true)
automaticGameFolderOutput(wildlander_output, steam_owrt_output, steam_mods_output lorerim_mods_output lorerim3_mods_output)
