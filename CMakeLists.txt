cmake_minimum_required(VERSION 3.16)

project(Sift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

qt_add_executable(Sift
    WIN32
    main.cpp
    MainWindow.h
    MainWindow.cpp
)

target_link_libraries(Sift PRIVATE Qt6::Widgets)

if(WIN32)
    target_link_libraries(Sift PRIVATE dwmapi)
endif()
