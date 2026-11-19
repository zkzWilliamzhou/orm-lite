cmake_minimum_required(VERSION 3.20)
project(orm-lite VERSION 1.0.0 LANGUAGES CXX)

# C++26 with reflection support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for C++26 reflection
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-std=c++2c -freflection)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-std=c++2c)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-std=c++2c)
endif()

# Header-only library
add_library(orm-lite INTERFACE)

target_include_directories(orm-lite INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Add alias for better usage
add_library(orm::lite ALIAS orm-lite)

# Test executable
add_executable(test_orm tests/test_orm.cpp)
target_link_libraries(test_orm PRIVATE orm-lite)

# Enable testing
enable_testing()
add_test(NAME ORMLiteTests COMMAND test_orm)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Optional: Build with sanitizers for development
option(ORM_LITE_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ORM_LITE_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Optional: Build tests with debug symbols
option(ORM_LITE_BUILD_TESTS "Build test executable" ON)
if(ORM_LITE_BUILD_TESTS)
    add_executable(test_orm tests/test_orm.cpp)
    target_link_libraries(test_orm PRIVATE orm-lite)
    enable_testing()
    add_test(NAME ORMLiteTests COMMAND test_orm)
endif()

# Installation
install(TARGETS orm-lite
    EXPORT orm-lite-targets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)