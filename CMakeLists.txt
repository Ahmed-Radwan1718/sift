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
)
target_link_libraries(Sift PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::Xml)

if(WIN32)
    target_link_libraries(Sift PRIVATE dwmapi)
endif()


# Commercial Aspose.Slides for C++ replaces the lightweight PPTX renderer
# when an SDK is supplied at build time. Sift itself never downloads a package.
# Shipping a public build requires reviewing Aspose redistribution/license terms.
option(SIFT_USE_ASPOSE "Build the offline Aspose.Slides PPT/PPTX rendering engine" OFF)
set(ASPOSE_SLIDES_ROOT "" CACHE PATH "Directory containing the extracted Aspose.Slides C++ SDK")
set(ASPOSE_SLIDES_INCLUDE_DIR "" CACHE PATH "Include directory containing DOM/Presentation.h")
set(ASPOSE_SLIDES_LIBRARY "" CACHE FILEPATH "Full path to Aspose.Slides C++ import/static .lib")
set(ASPOSE_SLIDES_EXTRA_INCLUDE_DIRS "" CACHE STRING "Additional SDK header directories separated by semicolons")
set(ASPOSE_SLIDES_EXTRA_LIBS "" CACHE STRING "Additional SDK libraries separated by semicolons")
set(ASPOSE_RUNTIME_DLLS "" CACHE STRING "Required SDK DLL paths separated by semicolons")
if(SIFT_USE_ASPOSE)
    if(NOT WIN32)
        message(FATAL_ERROR "Aspose integration is currently configured for Windows x64 only.")
    endif()
    if(NOT ASPOSE_SLIDES_INCLUDE_DIR AND ASPOSE_SLIDES_ROOT)
        find_path(ASPOSE_SLIDES_INCLUDE_DIR DOM/Presentation.h
            HINTS
                "${ASPOSE_SLIDES_ROOT}/include"
                "${ASPOSE_SLIDES_ROOT}/Include"
                "${ASPOSE_SLIDES_ROOT}/build/native/include/aspose.slides.cpp"
                "${ASPOSE_SLIDES_ROOT}/build/native/include"
                "${ASPOSE_SLIDES_ROOT}/build/native"
                "${ASPOSE_SLIDES_ROOT}"
        )
    endif()
    if(NOT ASPOSE_SLIDES_INCLUDE_DIR OR
       NOT EXISTS "${ASPOSE_SLIDES_INCLUDE_DIR}/DOM/Presentation.h")
        message(FATAL_ERROR
            "Aspose C++ headers not found. Set ASPOSE_SLIDES_INCLUDE_DIR to the "
            "folder containing DOM/Presentation.h. SDK is never fetched by Sift.")
    endif()
    if(NOT ASPOSE_SLIDES_LIBRARY AND ASPOSE_SLIDES_ROOT)
        find_library(ASPOSE_SLIDES_LIBRARY NAMES
            Aspose.Slides_vc14x64 Aspose.Slides.Cpp Aspose.Slides
            HINTS
                "${ASPOSE_SLIDES_ROOT}/build/native/lib"
                "${ASPOSE_SLIDES_ROOT}/lib"
                "${ASPOSE_SLIDES_ROOT}/lib/x64"
        )
    endif()
    if(NOT ASPOSE_SLIDES_LIBRARY OR NOT EXISTS "${ASPOSE_SLIDES_LIBRARY}")
        message(FATAL_ERROR
            "Aspose C++ library missing. Set ASPOSE_SLIDES_LIBRARY to its .lib, "
            "and ASPOSE_SLIDES_EXTRA_LIBS for additional CodePorting dependencies.")
    endif()
    target_sources(Sift PRIVATE AsposePresentationDocument.h AsposePresentationDocument.cpp)
    target_include_directories(Sift PRIVATE "${ASPOSE_SLIDES_INCLUDE_DIR}"
        ${ASPOSE_SLIDES_EXTRA_INCLUDE_DIRS})
    target_link_libraries(Sift PRIVATE "${ASPOSE_SLIDES_LIBRARY}" ${ASPOSE_SLIDES_EXTRA_LIBS})
    target_compile_definitions(Sift PRIVATE SIFT_WITH_ASPOSE=1)
    # The official NuGet native package places the release DLL next to
    # Aspose.Slides_vc14x64.lib. Runtime DLLs must ship alongside Sift.exe.
    if(NOT ASPOSE_RUNTIME_DLLS)
        get_filename_component(_aspose_libdir "${ASPOSE_SLIDES_LIBRARY}" DIRECTORY)
        set(_aspose_default_dll "${_aspose_libdir}/Aspose.Slides_vc14x64.dll")
        if(EXISTS "${_aspose_default_dll}")
            list(APPEND ASPOSE_RUNTIME_DLLS "${_aspose_default_dll}")
        endif()
    endif()
    foreach(dll IN LISTS ASPOSE_RUNTIME_DLLS)
        if(NOT EXISTS "${dll}")
            message(FATAL_ERROR "Aspose runtime DLL not found: ${dll}")
        endif()
        add_custom_command(TARGET Sift POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${dll}" "$<TARGET_FILE_DIR:Sift>"
            VERBATIM
        )
    endforeach()
    message(STATUS "Sift PowerPoint engine: commercial Aspose.Slides for C++")
else()
    message(STATUS "Sift PowerPoint engine: legacy lightweight preview")
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

# The SDK-dependent test is enabled only for builds that have linked Aspose.
# It validates both modern PPTX and legacy binary PPT rendering in evaluation
# mode, without embedding an SDK binary or license into the repository.
if(BUILD_TESTING AND SIFT_USE_ASPOSE AND Python3_Interpreter_FOUND)
    add_executable(AsposeSmokeTest tests/AsposeSmokeTest.cpp AsposePresentationDocument.cpp)
    target_include_directories(AsposeSmokeTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${ASPOSE_SLIDES_INCLUDE_DIR}"
        ${ASPOSE_SLIDES_EXTRA_INCLUDE_DIRS})
    target_link_libraries(AsposeSmokeTest PRIVATE
        Qt6::Widgets "${ASPOSE_SLIDES_LIBRARY}" ${ASPOSE_SLIDES_EXTRA_LIBS})
    foreach(dll IN LISTS ASPOSE_RUNTIME_DLLS)
        add_custom_command(TARGET AsposeSmokeTest POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${dll}" "$<TARGET_FILE_DIR:AsposeSmokeTest>"
            VERBATIM
        )
    endforeach()
    add_test(NAME GenerateAsposePresentation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_aspose_sample.py"
            "${CMAKE_CURRENT_BINARY_DIR}/aspose-example.pptx")
    add_test(NAME AsposePptAndPptx
        COMMAND AsposeSmokeTest "${CMAKE_CURRENT_BINARY_DIR}/aspose-example.pptx")
    set_tests_properties(AsposePptAndPptx PROPERTIES
        DEPENDS GenerateAsposePresentation
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
