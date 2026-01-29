#include "pdf_generator.h"
#include <wkhtmltox/pdf.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include "logging.hpp"
namespace htmlToPDF {

// Callback functions for wkhtmltopdf error/warning reporting
static void pdfErrorCallback(wkhtmltopdf_converter* /*converter*/, const char* msg) {
    if (msg) {
        LOG_ERROR("wkhtmltopdf: {}", msg);
    }
}

static void pdfWarningCallback(wkhtmltopdf_converter* /*converter*/, const char* msg) {
    if (msg) {
        LOG_WARN("wkhtmltopdf: {}", msg);
    }
}

bool PdfGenerator::initialized_ = false;
std::mutex PdfGenerator::mutex_;

PdfGenerator::PdfGenerator() : config_() {
    if (!initialized_) {
        initLibrary();
    }
}

PdfGenerator::PdfGenerator(const PdfConfig& config) : config_(config) {
    if (!initialized_) {
        initLibrary();
    }
}

PdfGenerator::~PdfGenerator() {
    // Don't deinit here - let the app control library lifecycle
}

bool PdfGenerator::initLibrary() {
    if (initialized_) return true;
    
    // Initialize with graphics support disabled (headless mode)
    if (wkhtmltopdf_init(0) != 1) {
        LOG_ERROR("Failed to initialize wkhtmltopdf library");
        return false;
    }
    initialized_ = true;
    return true;
}

void PdfGenerator::deinitLibrary() {
    if (initialized_) {
        wkhtmltopdf_deinit();
        initialized_ = false;
    }
}

bool PdfGenerator::generate(const std::string& htmlContent, const std::string& outputPath) {
    return doConvert(htmlContent, outputPath, nullptr);
}

bool PdfGenerator::generateFromHtml(const std::string& htmlContent, const std::string& outputPath, const PdfSettings& settings) {
    return doConvertWithSettings(htmlContent, outputPath, settings);
}

bool PdfGenerator::generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, const PdfSettings& settings) {
    if (htmlPages.empty()) return false;
    
    // Serialize all PDF generation
    LOG_INFO("Starting multi-page PDF generation: {}", outputPath);
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Acquired lock for multi-page PDF generation");
    
    if (!initialized_) {
        LOG_ERROR("Library not initialized");
        return false;
    }
    
    // Create global settings
    wkhtmltopdf_global_settings* gs = wkhtmltopdf_create_global_settings();
    if (!gs) {
        LOG_ERROR("Failed to create global settings");
        return false;
    }
    wkhtmltopdf_set_global_setting(gs, "out", outputPath.c_str());
    wkhtmltopdf_set_global_setting(gs, "size.pageSize", settings.pageSize.c_str());
    wkhtmltopdf_set_global_setting(gs, "orientation", settings.orientation.c_str());
    
    std::string marginTop = std::to_string(settings.marginTop) + "mm";
    std::string marginBottom = std::to_string(settings.marginBottom) + "mm";
    std::string marginLeft = std::to_string(settings.marginLeft) + "mm";
    std::string marginRight = std::to_string(settings.marginRight) + "mm";
    
    wkhtmltopdf_set_global_setting(gs, "margin.top", marginTop.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.bottom", marginBottom.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.left", marginLeft.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.right", marginRight.c_str());
    
    // Create converter
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        wkhtmltopdf_destroy_global_settings(gs);
        return false;
    }
    
    // Register error/warning callbacks
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    // Add each HTML page as a separate object
    for (const auto& html : htmlPages) {
        wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
        wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
        wkhtmltopdf_add_object(converter, os, html.c_str());
    }
    
    // Perform conversion
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    if (!success) {
        int httpError = wkhtmltopdf_http_error_code(converter);
        if (httpError != 0) {
            LOG_ERROR("Multi-page PDF conversion failed with HTTP error: {}", httpError);
        } else {
            LOG_ERROR("Multi-page PDF conversion failed");
        }
    } else {
        LOG_INFO("Multi-page PDF generated: {}", outputPath);
    }
    
    // Cleanup
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

bool PdfGenerator::generateFromFile(const std::string& htmlPath, const std::string& outputPath) {
    // Read file content
    std::ifstream file(htmlPath);
    if (!file) {
        LOG_ERROR("Failed to open file: {}", htmlPath);
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return generate(buffer.str(), outputPath);
}

bool PdfGenerator::generateToBuffer(const std::string& htmlContent, std::string& outputBuffer) {
    return doConvert(htmlContent, "", &outputBuffer);
}

bool PdfGenerator::doConvert(const std::string& htmlContent, const std::string& outputPath,
                              std::string* outputBuffer) {
    // Serialize all PDF generation - wkhtmltopdf library is not thread-safe
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Library not initialized");
        return false;
    }
    
    // Create global settings
    wkhtmltopdf_global_settings* gs = wkhtmltopdf_create_global_settings();
    if (!gs) {
        LOG_ERROR("Failed to create global settings");
        return false;
    }
    
    // Set output file (empty string = output to memory)
    if (!outputPath.empty()) {
        wkhtmltopdf_set_global_setting(gs, "out", outputPath.c_str());
    }
    
    // Page settings
    wkhtmltopdf_set_global_setting(gs, "size.pageSize", config_.pageSize.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.top", config_.marginTop.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.bottom", config_.marginBottom.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.left", config_.marginLeft.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.right", config_.marginRight.c_str());
    
    // Create object settings for HTML content
    wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
    if (!os) {
        LOG_ERROR("Failed to create object settings");
        wkhtmltopdf_destroy_global_settings(gs);
        return false;
    }
    
    // Enable local file access for images
    if (config_.enableLocalFileAccess) {
        wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
    }
    
    // Create converter
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        wkhtmltopdf_destroy_object_settings(os);
        wkhtmltopdf_destroy_global_settings(gs);
        return false;
    }
    
    // Register error/warning callbacks
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    // Add the HTML content as an object
    wkhtmltopdf_add_object(converter, os, htmlContent.c_str());
    
    // Perform conversion
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    // If outputting to buffer, get the data
    if (success && outputBuffer != nullptr) {
        const unsigned char* data = nullptr;
        long len = wkhtmltopdf_get_output(converter, &data);
        if (len > 0 && data != nullptr) {
            outputBuffer->assign(reinterpret_cast<const char*>(data), len);
        }
    }
    
    if (!success) {
        int httpError = wkhtmltopdf_http_error_code(converter);
        if (httpError != 0) {
            LOG_ERROR("PDF conversion failed with HTTP error: {}", httpError);
        } else {
            LOG_ERROR("PDF conversion failed");
        }
    } else if (!outputPath.empty()) {
        LOG_INFO("PDF generated: {}", outputPath);
    }
    
    // Cleanup
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

bool PdfGenerator::doConvertWithSettings(const std::string& htmlContent, const std::string& outputPath,
                                          const PdfSettings& settings) {
    // Serialize all PDF generation
    LOG_INFO("Starting PDF conversion: waiting for mutex lock");
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Acquired mutex lock for PDF conversion");

    if (!initialized_) {
        LOG_ERROR("Library not initialized");
        return false;
    }
    
    // Create global settings
    wkhtmltopdf_global_settings* gs = wkhtmltopdf_create_global_settings();
    if (!gs) {
        LOG_ERROR("Failed to create global settings");
        return false;
    }
    wkhtmltopdf_set_global_setting(gs, "out", outputPath.c_str());
    wkhtmltopdf_set_global_setting(gs, "size.pageSize", settings.pageSize.c_str());
    wkhtmltopdf_set_global_setting(gs, "orientation", settings.orientation.c_str());
    
    std::string marginTop = std::to_string(settings.marginTop) + "mm";
    std::string marginBottom = std::to_string(settings.marginBottom) + "mm";
    std::string marginLeft = std::to_string(settings.marginLeft) + "mm";
    std::string marginRight = std::to_string(settings.marginRight) + "mm";
    
    wkhtmltopdf_set_global_setting(gs, "margin.top", marginTop.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.bottom", marginBottom.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.left", marginLeft.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.right", marginRight.c_str());
    
    LOG_INFO("Global settings configured for PDF conversion");

    // Create object settings
    wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
    if (!os) {
        LOG_ERROR("Failed to create object settings");
        wkhtmltopdf_destroy_global_settings(gs);
        return false;
    }
    wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
    LOG_INFO("Object settings configured for PDF conversion");

    // Create converter
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        wkhtmltopdf_destroy_object_settings(os);
        wkhtmltopdf_destroy_global_settings(gs);
        return false;
    }
    LOG_INFO("PDF converter created");

    // Register error/warning callbacks
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    wkhtmltopdf_add_object(converter, os, htmlContent.c_str());
    
    // Perform conversion
    bool success = (wkhtmltopdf_convert     (converter) == 1);
    
    if (!success) {
        int httpError = wkhtmltopdf_http_error_code(converter);
        if (httpError != 0) {
            LOG_ERROR("PDF conversion failed with HTTP error: {}", httpError);
        } else {
            LOG_ERROR("PDF conversion failed");
        }
    } else {
        LOG_INFO("PDF generated: {}", outputPath);
    }

    // Cleanup; this will also clean up global and object settings - since the converter owns them
    wkhtmltopdf_destroy_converter(converter);

    return success;
}

} // namespace htmlToPDF
