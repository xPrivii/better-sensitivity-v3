cmake_minimum_required(VERSION 3.22)
project(PrecisionSensitivity CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dobby (inline hook library) ───────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG        master       # <-- poprawka: Dobby używa "master" nie "main"
    GIT_SHALLOW    TRUE
)
set(DOBBY_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dobby)

# ── Main library ──────────────────────────────────────────────────────────────
add_library(lib SHARED
    src/main.cpp
)

target_include_directories(lib PRIVATE
    include
    ${dobby_SOURCE_DIR}/include
)

target_link_libraries(lib PRIVATE
    dobby_static
    android
    log
    dl
)

target_compile_options(lib PRIVATE
    -Wall
    -Wextra
    -O2
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

target_link_options(lib PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
)

if(NOT DEFINED MCBE_VERSION)
    set(MCBE_VERSION 12100)
endif()
target_compile_definitions(lib PRIVATE
    MCBE_VERSION=${MCBE_VERSION}
)
