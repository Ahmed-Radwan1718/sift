cmake_minimum_required(VERSION 3.16)

project(Sift VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt PDF includes PDF rendering and the QPdfView widget.
# If CMake cannot find PdfWidgets, install the Qt PDF component
# matching your Qt kit using the Qt Maintenance Tool.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Pdf PdfWidgets)

qt_add_executable(Sift
    WIN32
    main.cpp
    MainWindow.h
    MainWindow.cpp
    DocumentDashboard.h
    DocumentDashboard.cpp
    Sidebar.h
    Sidebar.cpp
    PdfViewerPage.h
    PdfViewerPage.cpp
)

target_link_libraries(Sift PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets)

if(WIN32)
    target_link_libraries(Sift PRIVATE dwmapi)
endif()
