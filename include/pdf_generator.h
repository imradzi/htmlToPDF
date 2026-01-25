#pragma once

#include <string>
#include <mutex>

struct PdfConfig {
    std::string pageSize = "A4";
    std::string marginTop = "20mm";
    std::string marginBottom = "20mm";
    std::string marginLeft = "15mm";
    std::string marginRight = "15mm";
    bool enableLocalFileAccess = true;  // needed for local images
};

// Thread-safe PDF generator (serializes all conversions via mutex)
class PdfGenerator {
public:
    PdfGenerator();
    explicit PdfGenerator(const PdfConfig& config);
    ~PdfGenerator();
    
    // Initialize/deinitialize the library (call once at app start/end)
    static bool initLibrary();
    static void deinitLibrary();
    
    // Generate PDF from HTML content (string)
    bool generate(const std::string& htmlContent, const std::string& outputPath);
    
    // Generate PDF from HTML file
    bool generateFromFile(const std::string& htmlPath, const std::string& outputPath);
    
    // Generate PDF to memory buffer
    bool generateToBuffer(const std::string& htmlContent, std::string& outputBuffer);
    
private:
    PdfConfig config_;
    static bool initialized_;
    static std::mutex mutex_;  // Serializes all PDF generation (wkhtmltopdf is not thread-safe)
    
    bool doConvert(const std::string& htmlContent, const std::string& outputPath, 
                   std::string* outputBuffer = nullptr);
};
