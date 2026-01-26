cmake_minimum_required(VERSION 3.16)
project(handlebars_pdf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# wkhtmltox download settings
set(WKHTMLTOX_VERSION "0.12.6-1")
set(WKHTMLTOX_DIR "${CMAKE_BINARY_DIR}/wkhtmltox")

# Try to find existing installation first
find_path(WKHTMLTOX_INCLUDE_DIR wkhtmltox/pdf.h
    PATHS 
        /usr/include 
        /usr/local/include
        "C:/Program Files/wkhtmltopdf/include"
        "$ENV{ProgramFiles}/wkhtmltopdf/include"
        "${WKHTMLTOX_DIR}/include"
)
find_library(WKHTMLTOX_LIBRARY NAMES wkhtmltox
    PATHS 
        /usr/lib 
        /usr/local/lib 
        /usr/lib/x86_64-linux-gnu
        "C:/Program Files/wkhtmltopdf/lib"
        "$ENV{ProgramFiles}/wkhtmltopdf/lib"
        "${WKHTMLTOX_DIR}/lib"
)

# Download if not found
if(NOT WKHTMLTOX_INCLUDE_DIR OR NOT WKHTMLTOX_LIBRARY)
    message(STATUS "wkhtmltox not found, downloading...")
    
    if(WIN32)
        set(WKHTMLTOX_URL "https://github.com/wkhtmltopdf/packaging/releases/download/${WKHTMLTOX_VERSION}/wkhtmltox-${WKHTMLTOX_VERSION}.msvc2015-win64.exe")
        set(WKHTMLTOX_ARCHIVE "${CMAKE_BINARY_DIR}/wkhtmltox.exe")
    else()
        message(FATAL_ERROR "Automatic download not supported on this platform. Install wkhtmltox manually.")
    endif()
    
    if(NOT EXISTS "${WKHTMLTOX_DIR}/include/wkhtmltox/pdf.h")
        message(STATUS "Downloading wkhtmltox from ${WKHTMLTOX_URL}")
        file(DOWNLOAD 
            "${WKHTMLTOX_URL}" 
            "${WKHTMLTOX_ARCHIVE}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download wkhtmltox")
        endif()
        
        # Extract using 7z (the exe is a 7z self-extracting archive)
        message(STATUS "Extracting wkhtmltox...")
        file(MAKE_DIRECTORY "${WKHTMLTOX_DIR}")
        execute_process(
            COMMAND 7z x -y "-o${WKHTMLTOX_DIR}" "${WKHTMLTOX_ARCHIVE}"
            RESULT_VARIABLE EXTRACT_RESULT
            OUTPUT_QUIET
        )
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract wkhtmltox. Make sure 7z is installed and in PATH.")
        endif()
    endif()
    
    # Set paths to downloaded library
    set(WKHTMLTOX_INCLUDE_DIR "${WKHTMLTOX_DIR}/include" CACHE PATH "" FORCE)
    set(WKHTMLTOX_LIBRARY "${WKHTMLTOX_DIR}/lib/wkhtmltox.lib" CACHE FILEPATH "" FORCE)
endif()

if(NOT EXISTS "${WKHTMLTOX_INCLUDE_DIR}/wkhtmltox/pdf.h")
    message(FATAL_ERROR "wkhtmltox headers not found at ${WKHTMLTOX_INCLUDE_DIR}")
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
    get_filename_component(WKHTMLTOX_LIB_DIR "${WKHTMLTOX_LIBRARY}" DIRECTORY)
    get_filename_component(WKHTMLTOX_ROOT_DIR "${WKHTMLTOX_LIB_DIR}" DIRECTORY)
    set(WKHTMLTOX_DLL "${WKHTMLTOX_ROOT_DIR}/bin/wkhtmltox.dll")
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
