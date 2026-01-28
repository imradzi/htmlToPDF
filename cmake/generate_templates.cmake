# CMake script to generate C++ header from HTML templates
# Usage: cmake -DTEMPLATES_DIR=<path> -DOUTPUT_FILE=<path> -P generate_templates.cmake

if(NOT DEFINED TEMPLATES_DIR)
    message(FATAL_ERROR "TEMPLATES_DIR not defined")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not defined")
endif()

# Helper function to convert file content to C++ raw string literal
function(file_to_cpp_string FILEPATH VAR_NAME OUTPUT_VAR)
    file(READ "${FILEPATH}" FILE_CONTENT)
    # The content goes directly into R"(...)" literal
    set(${OUTPUT_VAR} "R\"(${FILE_CONTENT})\"" PARENT_SCOPE)
endfunction()

# Read all template files
file_to_cpp_string("${TEMPLATES_DIR}/invoice.html" "INVOICE" INVOICE_TEMPLATE)
file_to_cpp_string("${TEMPLATES_DIR}/report.html" "REPORT" REPORT_TEMPLATE)
file_to_cpp_string("${TEMPLATES_DIR}/letter.html" "LETTER" LETTER_TEMPLATE)
file_to_cpp_string("${TEMPLATES_DIR}/sales_summary.html" "SALES_SUMMARY" SALES_SUMMARY_TEMPLATE)
file_to_cpp_string("${TEMPLATES_DIR}/purchase_summary.html" "PURCHASE_SUMMARY" PURCHASE_SUMMARY_TEMPLATE)

# Generate the header file
file(WRITE "${OUTPUT_FILE}" 
"// Auto-generated file - DO NOT EDIT
// Generated from HTML templates in htmlToPDF/templates/
// Regenerate by running: cmake --build . --target generate_templates

#pragma once

namespace TemplateStrings {

inline const char* getInvoiceTemplate() {
    return ${INVOICE_TEMPLATE};
}

inline const char* getReportTemplate() {
    return ${REPORT_TEMPLATE};
}

inline const char* getLetterTemplate() {
    return ${LETTER_TEMPLATE};
}

inline const char* getSalesSummaryTemplate() {
    return ${SALES_SUMMARY_TEMPLATE};
}

inline const char* getPurchaseSummaryTemplate() {
    return ${PURCHASE_SUMMARY_TEMPLATE};
}

} // namespace TemplateStrings
")

message(STATUS "Generated ${OUTPUT_FILE} from HTML templates")
