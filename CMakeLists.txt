cmake_minimum_required(VERSION 3.22)

project(vellum VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Sql)

add_executable(vellum
  src/main.cpp
  src/app/MainWindow.h
  src/app/MainWindow.cpp
  src/canvas/CanvasWidget.h
  src/canvas/CanvasWidget.cpp
)

target_include_directories(vellum PRIVATE src)

target_link_libraries(vellum PRIVATE
  Qt6::Widgets
  Qt6::PrintSupport
  Qt6::Sql
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vellum PRIVATE -Wall -Wextra -Wpedantic)
endif()

