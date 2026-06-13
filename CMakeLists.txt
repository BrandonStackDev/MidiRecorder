cmake_minimum_required(VERSION 3.20)
project(MidiSheetRaylib C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Where you installed raylib with "cmake --install"
list(APPEND CMAKE_PREFIX_PATH "C:/raylib")
find_package(raylib CONFIG REQUIRED)

set(SRC
    src/main.c
    src/midi_input.c
    src/song.c
    src/editor.c
    src/lilypond_export.c
)

add_executable(midi_sheet ${SRC})

# Static CRT like your existing raylib test projects.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

foreach(tgt IN ITEMS midi_sheet)
    set_target_properties(${tgt} PROPERTIES LINK_FLAGS "/STACK:16777216")
    target_link_libraries(${tgt} PRIVATE raylib opengl32 gdi32 winmm user32 shell32)
    target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(${tgt} PRIVATE "C:/raylib/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
endforeach()
