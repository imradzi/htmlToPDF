#include "pdf_generator.h"
#include <wkhtmltox/pdf.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

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
        std::cerr << "Failed to initialize wkhtmltopdf library" << std::endl;
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

bool PdfGenerator::generateFromFile(const std::string& htmlPath, const std::string& outputPath) {
    // Read file content
    std::ifstream file(htmlPath);
    if (!file) {
        std::cerr << "Failed to open file: " << htmlPath << std::endl;
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
        std::cerr << "Library not initialized" << std::endl;
        return false;
    }
    
    // Create global settings
    wkhtmltopdf_global_settings* gs = wkhtmltopdf_create_global_settings();
    
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
    
    // Enable local file access for images
    if (config_.enableLocalFileAccess) {
        wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
    }
    
    // Create converter
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    
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
        std::cerr << "PDF conversion failed" << std::endl;
    } else if (!outputPath.empty()) {
        std::cout << "PDF generated: " << outputPath << std::endl;
    }
    
    // Cleanup
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}
