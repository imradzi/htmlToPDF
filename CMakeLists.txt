cmake_minimum_required(VERSION 3.16)
project(handlebars_pdf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# wkhtmltox settings - bundled libraries for Windows
set(WKHTMLTOX_VERSION "0.12.6-1")

if(WIN32)
    # Use bundled wkhtmltox libraries based on target architecture
    if(CMAKE_CL_64 STREQUAL "0")
        # 32-bit build
        set(WKHTMLTOX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/win32")
        message(STATUS "Using 32-bit wkhtmltox library")
    else()
        # 64-bit build
        set(WKHTMLTOX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/win64")
        message(STATUS "Using 64-bit wkhtmltox library")
    endif()
    
    set(WKHTMLTOX_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(WKHTMLTOX_LIBRARY "${WKHTMLTOX_ROOT}/wkhtmltox.lib")
    set(WKHTMLTOX_DLL "${WKHTMLTOX_ROOT}/wkhtmltox.dll")
    
    if(NOT EXISTS "${WKHTMLTOX_LIBRARY}")
        message(FATAL_ERROR "wkhtmltox library not found at ${WKHTMLTOX_LIBRARY}. "
            "Please place the appropriate wkhtmltox.lib in lib/win32 or lib/win64.")
    endif()
else()
    # Linux: Try to find system installation
    find_path(WKHTMLTOX_INCLUDE_DIR wkhtmltox/pdf.h
        PATHS 
            /usr/include 
            /usr/local/include
    )
    find_library(WKHTMLTOX_LIBRARY NAMES wkhtmltox
        PATHS 
            /usr/lib 
            /usr/local/lib 
            /usr/lib/x86_64-linux-gnu
    )
    
    if(NOT WKHTMLTOX_INCLUDE_DIR OR NOT WKHTMLTOX_LIBRARY)
        message(FATAL_ERROR "wkhtmltox not found. Install libwkhtmltox-dev or equivalent.")
    endif()
endif()

if(NOT EXISTS "${WKHTMLTOX_INCLUDE_DIR}/wkhtmltox/pdf.h")
    message(FATAL_ERROR "wkhtmltox headers not found at ${WKHTMLTOX_INCLUDE_DIR}/wkhtmltox")
endif()

message(STATUS "Found libwkhtmltox: ${WKHTMLTOX_LIBRARY}")

# ========== Template Generation ==========
# Generate C++ header from HTML templates
set(TEMPLATE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/invoice.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/report.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/letter.html
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/sales_summary.html
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
    src/sales_summary_builder.cpp
    ${GENERATED_TEMPLATES_HEADER}  # Add as source so it's tracked as dependency
)

target_include_directories(htmlToPDF PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${WKHTMLTOX_INCLUDE_DIR}
)

# Ensure template generation happens before compiling
add_dependencies(htmlToPDF generate_templates)

target_link_libraries(htmlToPDF PUBLIC
    ${WKHTMLTOX_LIBRARY}
)

# Create standalone executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WKHTMLTOX_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    htmlToPDF
    ${WKHTMLTOX_LIBRARY}
)

# Copy wkhtmltox DLL to output directory (Windows)
if(WIN32)
    if(EXISTS "${WKHTMLTOX_DLL}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WKHTMLTOX_DLL}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying wkhtmltox.dll to output directory"
        )
    endif()
endif()

# Copy templates directory to build folder
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/templates DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
