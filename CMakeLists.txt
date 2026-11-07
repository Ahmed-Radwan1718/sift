cmake_minimum_required(VERSION 3.16)

project(Sift VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt PDF includes PDF rendering and the QPdfView widget.
# If CMake cannot find PdfWidgets, install the Qt PDF component
# matching your Qt kit using the Qt Maintenance Tool.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Pdf PdfWidgets Xml)

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
    PptxDocument.h
    PptxDocument.cpp
    PresentationViewerPage.h
    PresentationViewerPage.cpp
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)

# Embed the original Settings icon so it works from any installation path.
qt_add_resources(Sift "sift_icons"
    PREFIX "/"
    FILES settings.png
)

# Only the read/decompression APIs are needed for PPTX files.
target_compile_definitions(Sift PRIVATE
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ARCHIVE_WRITING_APIS
)
target_link_libraries(Sift PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Xml)

if(WIN32)
    target_link_libraries(Sift PRIVATE dwmapi)
endif()

# Developer-only smoke tests; no Python or test runtime is required by Sift users.
include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_executable(PptxSmokeTest
            tests/PptxSmokeTest.cpp
            PptxDocument.h
            PptxDocument.cpp
            third_party/miniz/miniz.c
            third_party/miniz/miniz_tinfl.c
            third_party/miniz/miniz_zip.c
        )
        target_include_directories(PptxSmokeTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(PptxSmokeTest PRIVATE
            MINIZ_NO_DEFLATE_APIS
            MINIZ_NO_ARCHIVE_WRITING_APIS
        )
        target_link_libraries(PptxSmokeTest PRIVATE Qt6::Widgets Qt6::Xml)
        add_test(NAME GenerateExamplePresentation
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_sample.py"
                "${CMAKE_CURRENT_BINARY_DIR}/example.pptx"
        )
        add_test(NAME PptxReadAndRender
            COMMAND PptxSmokeTest "${CMAKE_CURRENT_BINARY_DIR}/example.pptx"
        )
        set_tests_properties(PptxReadAndRender PROPERTIES
            DEPENDS GenerateExamplePresentation
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()
