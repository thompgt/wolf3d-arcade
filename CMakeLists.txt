# The source of truth for how this project is built. The Makefile and
# build.bat predate it and are kept as thin conveniences, but the two of them
# had already drifted -- one passed -Wpedantic and -MMD -MP and the other did
# not, and -mwindows was a link flag in one and a compile flag in the other.
# Anything that matters to the build belongs here now.
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ./build/wolf3d.exe --selftest
#
# Also opens the project to MSVC and to any IDE that reads CMake, neither of
# which could touch a MinGW-only makefile.

cmake_minimum_required(VERSION 3.16)
project(wolf3d_arcade CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "wolf3d-arcade targets Win32/GDI only")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

file(GLOB WOLF_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/core/*.cpp
    src/game/*.cpp
    src/platform/*.cpp
    src/render/*.cpp
)

add_executable(wolf3d ${WOLF_SOURCES})
set_target_properties(wolf3d PROPERTIES
    OUTPUT_NAME wolf3d
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# gdi32/user32 for the window and the blit; winmm for timeBeginPeriod, which
# the frame pacing in main() depends on for a Sleep that lands on time.
target_link_libraries(wolf3d PRIVATE gdi32 user32 winmm)

if(MSVC)
    target_compile_options(wolf3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(wolf3d PRIVATE -Wall -Wextra -Wpedantic)

    # Static linking is not a preference here. A dynamically linked MinGW
    # binary trips Windows Defender heuristics on some machines and fails to
    # launch at all, with no diagnostic.
    target_link_options(wolf3d PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()

# The release build is a windowed application: no console flashing up behind
# the game. Debug keeps the console so printf and the selftest report are
# visible while working on it.
if(MSVC)
    set_target_properties(wolf3d PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
else()
    target_link_options(wolf3d PRIVATE $<$<CONFIG:Release>:-mwindows>)
endif()

target_compile_definitions(wolf3d PRIVATE $<$<CONFIG:Debug>:WOLF_DEBUG>)

# The headless harness is the whole test suite. It is the same binary, so
# there is no way for the tests to be built differently from the game.
enable_testing()
add_test(NAME selftest COMMAND wolf3d --selftest)
