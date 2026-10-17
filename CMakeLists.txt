cmake_minimum_required(VERSION 3.16)
project(handlebars_pdf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# PlutoBook is provided by the parent build (see the top-level CMakeLists.txt):
# PLUTOBOOK_INCLUDE_DIR / PLUTOBOOK_LIBRARY plus the PkgConfig::PLUTOBOOK_PC
# imported target that carries cairo/fontconfig/freetype/harfbuzz/ICU/etc.
if(NOT TARGET PkgConfig::PLUTOBOOK_PC)
    # Standalone build (e.g. re_cmake64.bat): locate the vendored prefix.
    if(WIN32)
        set(PLUTOBOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/prefix/win64")
    else()
        set(PLUTOBOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/prefix/linux-x64")
    endif()
    find_path(PLUTOBOOK_INCLUDE_DIR plutobook/plutobook.hpp
        PATHS "${PLUTOBOOK_ROOT}/include" /usr/include /usr/local/include
    )
    find_package(PkgConfig REQUIRED)
    if(CMAKE_HOST_WIN32)
        set(_PPOS_PC_SEP ";")
    else()
        set(_PPOS_PC_SEP ":")
    endif()
    set(ENV{PKG_CONFIG_PATH}
        "${PLUTOBOOK_ROOT}/lib/pkgconfig${_PPOS_PC_SEP}${PLUTOBOOK_ROOT}/lib/x86_64-linux-gnu/pkgconfig${_PPOS_PC_SEP}$ENV{PKG_CONFIG_PATH}")
    pkg_check_modules(PLUTOBOOK_PC REQUIRED IMPORTED_TARGET plutobook)
endif()

# ========== Template Generation ==========
# Generate C++ header from HTML templates
set(TEMPLATE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/invoice.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/report.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/letter.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/sales_summary.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/purchase_summary.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/poison_order.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/billing_statement.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/purchase_order.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/tabular_report.html
)

set(GENERATED_TEMPLATES_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/template_strings.h)

# Custom command to generate the header from HTML files
add_custom_command(
    OUTPUT ${GENERATED_TEMPLATES_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND ${CMAKE_COMMAND}
        -DTEMPLATES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/templates
        -DOUTPUT_FILE=${GENERATED_TEMPLATES_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_templates.cmake
    DEPENDS ${TEMPLATE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_templates.cmake
    COMMENT "Generating template_strings.h from HTML templates"
    VERBATIM
)

# Custom target for manual regeneration
add_custom_target(generate_templates
    DEPENDS ${GENERATED_TEMPLATES_HEADER}
    COMMENT "Regenerating template strings from HTML files"
)

# Create a static library for use by other targets
add_library(htmlToPDF STATIC
    src/template_engine.cpp
    src/pdf_generator.cpp
    src/pdf_generator_proxy.cpp
    src/html_report_builder.cpp
    src/html_report_builder_pdf.cpp
    src/sales_summary_builder.cpp
    src/purchase_summary_builder.cpp
    src/invoice_builder.cpp
    ${GENERATED_TEMPLATES_HEADER}  # Add as source so it's tracked as dependency
)

target_compile_features(htmlToPDF PUBLIC cxx_std_20)

# PlutoBook is built with -fno-rtti. Only pdf_generator.cpp instantiates its
# canvas classes; compiling just that file the same way avoids a link-time
# reference to typeinfo for plutobook::Canvas, which the library does not emit.
if(NOT MSVC)
    set_source_files_properties(src/pdf_generator.cpp PROPERTIES COMPILE_OPTIONS "-fno-rtti")
endif()

target_include_directories(htmlToPDF PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${PLUTOBOOK_INCLUDE_DIR}
    ${wxWidgets_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../wpObject
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../util
)

# Ensure consistent Debug/Release settings with parent project
target_compile_definitions(htmlToPDF PRIVATE NDEBUG wxDEBUG_LEVEL=0)

# Ensure template generation happens before compiling
add_dependencies(htmlToPDF generate_templates)

# Link with logger from parent project if available.
# PlutoBook and its dependencies are PRIVATE: only htmlToPDF needs them, but
# CMake still propagates static-library dependencies to the final link.
if(TARGET logger)
    target_link_libraries(htmlToPDF PUBLIC
        ${wxWidgets_LIBRARIES}
        logger
    )
    target_link_libraries(htmlToPDF PRIVATE
        PkgConfig::PLUTOBOOK_PC
    )
else()
    target_link_libraries(htmlToPDF PUBLIC
        ${wxWidgets_LIBRARIES}
    )
    target_link_libraries(htmlToPDF PRIVATE
        PkgConfig::PLUTOBOOK_PC
    )
endif()

# Create standalone executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PLUTOBOOK_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    htmlToPDF
    PkgConfig::PLUTOBOOK_PC
)

# Standalone probe used to verify report-builder headers/footers.
add_executable(report_probe tools/report_probe.cpp)
target_include_directories(report_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PLUTOBOOK_INCLUDE_DIR}
)
target_link_libraries(report_probe PRIVATE htmlToPDF)
target_compile_features(report_probe PRIVATE cxx_std_20)

# Copy templates directory to build folder
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/templates DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
