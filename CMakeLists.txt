cmake_minimum_required(VERSION 3.14)
project(ambient_lounge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Raylib FetchContent
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
FetchContent_MakeAvailable(raylib)

# nlohmann_json FetchContent
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(ambient_lounge src/main.cpp)

target_link_libraries(ambient_lounge PRIVATE raylib nlohmann_json::nlohmann_json)

if (WIN32)
    target_link_libraries(ambient_lounge PRIVATE winmm opengl32 gdi32 user32 shell32)
endif()
