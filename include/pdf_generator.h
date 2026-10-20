#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace htmlToPDF {

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
    // Settings struct for more flexible configuration
    struct PdfSettings {
        std::string pageSize = "A4";
        std::string orientation = "Portrait";  // "Portrait" or "Landscape"
        int marginTop = 10;
        int marginBottom = 10;
        int marginLeft = 10;
        int marginRight = 10;
        std::string headerLeft;
        std::string headerCenter;   // static centered text; "[section]" selects the running page-header element
        std::string headerRight;
        std::string headerTitle;    // appears below headerLeft on a second line
        std::string headerSubtitle; // appears below headerTitle on a third line
        std::string headerFontSize = "8";
        std::string footerCenter;   // centered footer text; "[subsection]" uses the running page-footer element
        std::string footerFontSize = "4";
        // Base URL used to resolve relative image/resource paths (e.g. "templates/letterhead.png").
        // Empty means the working directory is used, matching the old wkhtmltopdf behaviour.
        std::string baseUrl;
        // When true, generateMultiPagePdf numbers pages continuously across all
        // documents (e.g. report sections) instead of restarting at 1 per document.
        bool continuePageNumbering = false;
        // Render scale for the document content. wkhtmltopdf's default "smart
        // shrinking" renders every CSS length at 80% of its nominal size while
        // leaving the page box (paper size, margins, header/footer) alone, and
        // all existing templates were tuned against that. 1.0 gives standard
        // CSS sizing.
        double contentScale = 0.8;
    };

    PdfGenerator();
    explicit PdfGenerator(const PdfConfig& config);
    ~PdfGenerator();
    
    // Initialize/deinitialize the PDF engine (call once at app start/end).
    // PlutoBook needs no process-wide GUI initialization; initLibrary() only
    // sets up the Windows fontconfig path.
    static bool initLibrary();
    static void deinitLibrary();
    
    // Generate PDF from HTML content (string)
    bool generate(const std::string& htmlContent, const std::string& outputPath);
    
    // Generate PDF from HTML string with settings
    bool generateFromHtml(const std::string& htmlContent, const std::string& outputPath, const PdfSettings& settings);
    
    // Generate multi-page PDF from multiple HTML strings
    bool generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, const PdfSettings& settings);
    
    // Generate PDF from HTML file
    bool generateFromFile(const std::string& htmlPath, const std::string& outputPath);
    
    // Generate PDF to memory buffer
    bool generateToBuffer(const std::string& htmlContent, std::string& outputBuffer);
    
private:
    PdfConfig config_;
    static bool initialized_;
    static std::mutex mutex_;  // Serializes conversions; PlutoBook Book instances are independent,
                               // but keeping this preserves the previous single-conversion behaviour.
    
    bool doConvert(const std::string& htmlContent, const std::string& outputPath, std::string* outputBuffer = nullptr);
    bool doConvertWithSettings(const std::string& htmlContent, const std::string& outputPath, const PdfSettings& settings);
};

// Request data structure for PDF generation
struct PdfGenerateRequest {
    enum class RequestType {
        GenerateFromHtml,
        GenerateMultiPage,
        GenerateToBuffer
    };
    
    RequestType type = RequestType::GenerateFromHtml;
    std::string htmlContent;
    std::vector<std::string> htmlPages;  // for multi-page
    std::string outputPath;
    PdfGenerator::PdfSettings settings;
    std::string* outputBuffer = nullptr;  // for generateToBuffer
};

// Result data structure
struct PdfGenerateResult {
    bool success = false;
    std::string errorMessage;
};

// ============================================================================
// PdfGeneratorProxy - backwards-compatible wrapper for worker threads
// ============================================================================
// The class name is historical: it used to marshal work to the wx main thread
// for wkhtmltox/Qt. PlutoBook has no such requirement, so these methods now
// render directly on the calling thread. PdfGenerator's internal mutex still
// serializes concurrent conversions.

class PdfGeneratorProxy {
public:
    PdfGeneratorProxy();
    explicit PdfGeneratorProxy(const PdfConfig& config);

    // Thread-safe: renders on the calling thread.
    bool generateFromHtml(const std::string& htmlContent, const std::string& outputPath, const PdfGenerator::PdfSettings& settings);
    bool generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, const PdfGenerator::PdfSettings& settings);
    bool generateToBuffer(const std::string& htmlContent, std::string& outputBuffer);

private:
    PdfConfig config_;
    static PdfGenerator generator_;
};

} // namespace htmlToPDF

// For backward compatibility
using PdfGenerator = htmlToPDF::PdfGenerator;
using PdfConfig = htmlToPDF::PdfConfig;
using PdfGeneratorProxy = htmlToPDF::PdfGeneratorProxy;
