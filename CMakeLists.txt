cmake_minimum_required(VERSION 3.20)
project(AutoDOS VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# autodos.cpp #includes miniz.c (single translation unit); do not compile miniz.c again.
set(ENGINE_SOURCES src/autodos.cpp)
set(LAUNCH_SOURCES src/launch_posix.cpp)

add_executable(autodos-cli
    src/main_cli.cpp
    ${ENGINE_SOURCES}
    ${LAUNCH_SOURCES}
)
target_include_directories(autodos-cli PRIVATE src)
target_link_libraries(autodos-cli PRIVATE nlohmann_json::nlohmann_json)

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses QUIET)
if(CURSES_FOUND)
    add_executable(autodos-tui
        src/main_tui.cpp
        ${ENGINE_SOURCES}
        ${LAUNCH_SOURCES}
    )
    target_include_directories(autodos-tui PRIVATE src ${CURSES_INCLUDE_DIR})
    target_link_libraries(autodos-tui PRIVATE
        nlohmann_json::nlohmann_json
        ${CURSES_LIBRARIES}
    )
    target_compile_definitions(autodos-tui PRIVATE _DEFAULT_SOURCE)
else()
    message(STATUS "Curses/ncurses not found — autodos-tui target skipped (install ncurses dev package)")
endif()
