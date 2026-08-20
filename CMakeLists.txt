cmake_minimum_required(VERSION 3.24)

project(room_engine
    VERSION 0.1.0
    DESCRIPTION "Cross-platform 2D/3D room-design engine foundation"
    LANGUAGES CXX
)

include(CTest)
include(FetchContent)

option(ROOM_ENGINE_FETCH_SDL3 "Download SDL3 when it is not provided by the system" ON)
option(ROOM_ENGINE_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ROOM_ENGINE_FETCH_SDL3)
    # Keep the dependency on a deliberate SDL3 release. Update this tag only
    # as an explicit dependency change.
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

add_library(room_engine_core STATIC
    src/room_engine/application.cpp
)

target_include_directories(room_engine_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_compile_features(room_engine_core PUBLIC cxx_std_20)

if(ROOM_ENGINE_ENABLE_WARNINGS)
    include(cmake/CompilerWarnings.cmake)
    set_project_warnings(room_engine_core)
endif()

add_executable(room_engine_app src/main.cpp)
target_link_libraries(room_engine_app PRIVATE room_engine_core SDL3::SDL3)

if(ROOM_ENGINE_ENABLE_WARNINGS)
    set_project_warnings(room_engine_app)
endif()

if(BUILD_TESTING)
    add_executable(room_engine_smoke_tests tests/smoke_test.cpp)
    target_link_libraries(room_engine_smoke_tests PRIVATE room_engine_core)

    if(ROOM_ENGINE_ENABLE_WARNINGS)
        set_project_warnings(room_engine_smoke_tests)
    endif()

    add_test(NAME room_engine.smoke COMMAND room_engine_smoke_tests)
endif()

add_custom_target(format
    COMMAND clang-format -i
        ${PROJECT_SOURCE_DIR}/src/main.cpp
        ${PROJECT_SOURCE_DIR}/src/room_engine/application.cpp
        ${PROJECT_SOURCE_DIR}/src/room_engine/application.hpp
        ${PROJECT_SOURCE_DIR}/tests/smoke_test.cpp
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM
)
