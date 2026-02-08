#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <wx/event.h>
#include <wx/thread.h>

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
    };

    PdfGenerator();
    explicit PdfGenerator(const PdfConfig& config);
    ~PdfGenerator();
    
    // Initialize/deinitialize the library (call once at app start/end)
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
    static std::mutex mutex_;  // Serializes all PDF generation (wkhtmltopdf is not thread-safe)
    
    bool doConvert(const std::string& htmlContent, const std::string& outputPath, 
                   std::string* outputBuffer = nullptr);
    bool doConvertWithSettings(const std::string& htmlContent, const std::string& outputPath, 
                               const PdfSettings& settings);
};

// ============================================================================
// PDF Generation Event - for running PDF generation on main thread
// ============================================================================

// Forward declaration
class PdfGenerateEvent;

// Declare the custom event type
wxDECLARE_EVENT(wpEVT_PDF_GENERATE, PdfGenerateEvent);

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

// Custom event class for PDF generation
class PdfGenerateEvent : public wxEvent {
public:
    PdfGenerateEvent(wxEventType eventType = wpEVT_PDF_GENERATE, int winid = 0);
    PdfGenerateEvent(const PdfGenerateEvent& other);
    
    virtual wxEvent* Clone() const override { return new PdfGenerateEvent(*this); }
    
    // Set request data
    void SetRequest(const PdfGenerateRequest& req) { request_ = req; }
    const PdfGenerateRequest& GetRequest() const { return request_; }
    
    // Set/Get result
    void SetResult(const PdfGenerateResult& res) { result_ = res; }
    const PdfGenerateResult& GetResult() const { return result_; }
    
    // Completion signaling
    void SetCompletionCallback(std::function<void()> callback) { completionCallback_ = callback; }
    void SignalCompletion() { if (completionCallback_) completionCallback_(); }
    
private:
    PdfGenerateRequest request_;
    PdfGenerateResult result_;
    std::function<void()> completionCallback_;
};

// Event handler type
typedef void (wxEvtHandler::*PdfGenerateEventFunction)(PdfGenerateEvent&);
#define PdfGenerateEventHandler(func) wxEVENT_HANDLER_CAST(PdfGenerateEventFunction, func)
#define EVT_PDF_GENERATE(id, func) wx__DECLARE_EVT1(wpEVT_PDF_GENERATE, id, PdfGenerateEventHandler(func))

// ============================================================================
// PdfGeneratorProxy - call from worker threads, executes on main thread
// ============================================================================

class PdfGeneratorProxy {
public:
    PdfGeneratorProxy();
    explicit PdfGeneratorProxy(const PdfConfig& config);
    
    // Set the event handler (usually wxTheApp or main frame) - must be called before use
    static void SetEventHandler(wxEvtHandler* handler);
    
    // Thread-safe methods that dispatch to main thread and wait for completion
    bool generateFromHtml(const std::string& htmlContent, const std::string& outputPath, 
                          const PdfGenerator::PdfSettings& settings);
    bool generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, 
                              const PdfGenerator::PdfSettings& settings);
    bool generateToBuffer(const std::string& htmlContent, std::string& outputBuffer);
    
    // Handler to be called on main thread (register this with your event handler)
    static void OnPdfGenerateEvent(PdfGenerateEvent& event);
    
private:
    PdfConfig config_;
    static wxEvtHandler* eventHandler_;
    static PdfGenerator generator_;  // The actual generator, used only on main thread
    
    // Execute request on main thread and wait for completion
    PdfGenerateResult executeOnMainThread(const PdfGenerateRequest& request);
};

} // namespace htmlToPDF

// For backward compatibility
using PdfGenerator = htmlToPDF::PdfGenerator;
using PdfConfig = htmlToPDF::PdfConfig;
using PdfGeneratorProxy = htmlToPDF::PdfGeneratorProxy;
using PdfGenerateEvent = htmlToPDF::PdfGenerateEvent;
