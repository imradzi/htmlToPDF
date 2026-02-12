#include "wx/wxprec.h"
#include <stdexcept>

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "pdf_generator.h"
#include <wkhtmltox/pdf.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include "logging.hpp"
#include "global.h"
wxDEFINE_EVENT(wpEVT_PDF_GENERATE, wxCommandEvent);

namespace htmlToPDF {


wxEvtHandler* PdfGeneratorProxy::eventHandler_ = nullptr;
PdfGenerator PdfGeneratorProxy::generator_;

PdfGeneratorProxy::PdfGeneratorProxy() : config_() {}

PdfGeneratorProxy::PdfGeneratorProxy(const PdfConfig& config) : config_(config) {}

void PdfGeneratorProxy::SetEventHandler(wxEvtHandler* handler) {eventHandler_ = handler;}

bool PdfGeneratorProxy::generateFromHtml(const std::string& htmlContent, const std::string& outputPath, const PdfGenerator::PdfSettings& settings) {
    PdfGenerateRequest request;
    request.type = PdfGenerateRequest::RequestType::GenerateFromHtml;
    request.htmlContent = htmlContent;
    request.outputPath = outputPath;
    request.settings = settings;
    
    auto result = executeOnMainThread(request);
    return result.success;
}

bool PdfGeneratorProxy::generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, const PdfGenerator::PdfSettings& settings) {
    PdfGenerateRequest request;
    request.type = PdfGenerateRequest::RequestType::GenerateMultiPage;
    request.htmlPages = htmlPages;
    request.outputPath = outputPath;
    request.settings = settings;
    
    auto result = executeOnMainThread(request);
    return result.success;
}

bool PdfGeneratorProxy::generateToBuffer(const std::string& htmlContent, std::string& outputBuffer) {
    PdfGenerateRequest request;
    request.type = PdfGenerateRequest::RequestType::GenerateToBuffer;
    request.htmlContent = htmlContent;
    request.outputBuffer = &outputBuffer;
    
    auto result = executeOnMainThread(request);
    return result.success;
}

using CallBackFunction = std::function<void(PdfGenerateResult&&)>;
struct EventData {
    PdfGenerateRequest request;
    CallBackFunction callback;
};

PdfGenerateResult PdfGeneratorProxy::executeOnMainThread(const PdfGenerateRequest& request) {
    if (eventHandler_ == nullptr) {
        LOG_ERROR("PdfGeneratorProxy: Event handler not set");
        return PdfGenerateResult{ false, "Event handler not set" };
    }
    PdfGenerateResult result;
    result.success = false;

    LOG_INFO("PdfGeneratorProxy: executeOnMainThread called");
   
    // We're on a worker thread - send event to main thread and wait
    std::mutex completionMutex;
    std::condition_variable completionCV;
    bool completed = false;
  
    EventData evData { request, [&completionMutex, &completionCV, &completed, &result](PdfGenerateResult &&res) {
        LOG_INFO("PdfGeneratorProxy: PDF generation callback called");
        std::lock_guard<std::mutex> lock(completionMutex);
        result = std::move(res);
        completed = true;
        completionCV.notify_one();
    }};

    wxCommandEvent event(wpEVT_PDF_GENERATE);
    event.SetClientData(&evData);

    // Send event to main thread
    LOG_INFO("PdfGeneratorProxy: Posting event to main thread");
    eventHandler_->QueueEvent(event.Clone());

    std::unique_lock<std::mutex> lock(completionMutex);

    // Wait for completion
    LOG_INFO("PdfGeneratorProxy: Waiting for PDF generation to complete");
    completionCV.wait(lock, [&completed]() { return completed || global::g.isAppShuttingDown.load(std::memory_order_acquire); });

    if (completed) LOG_INFO("PdfGeneratorProxy: PDF generation completed");
    else LOG_ERROR("PdfGeneratorProxy: PDF generation interrupted due to shutdown");
    return result;
}

void PdfGeneratorProxy::OnEvent(wxCommandEvent& event) {
    LOG_INFO("PdfGeneratorProxy: OnEvent called on main thread");
    auto p = static_cast<EventData *>(event.GetClientData());
    if (p == nullptr) {
        LOG_ERROR("PdfGeneratorProxy: Invalid event client data");
        return;
    }

    auto& request = p->request;
    PdfGenerateResult result;
   
    try {
        switch (request.type) {
            case PdfGenerateRequest::RequestType::GenerateFromHtml:
            LOG_INFO("PdfGeneratorProxy: Generating PDF from HTML");
                result.success = generator_.generateFromHtml(request.htmlContent, request.outputPath, request.settings);
                break;
                
            case PdfGenerateRequest::RequestType::GenerateMultiPage:
                LOG_INFO("PdfGeneratorProxy: Generating PDF from multiple HTML pages");
                result.success = generator_.generateMultiPagePdf(request.htmlPages,request.outputPath,request.settings);
                break;
                
            case PdfGenerateRequest::RequestType::GenerateToBuffer:
                LOG_INFO("PdfGeneratorProxy: Generating PDF to memory buffer");
                if (request.outputBuffer) {
                    result.success = generator_.generateToBuffer(request.htmlContent, *const_cast<std::string*>(request.outputBuffer));
                } else {
                    result.success = false;
                    result.errorMessage = "Output buffer is null";
                }
                break;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
        LOG_ERROR("PdfGeneratorProxy: Exception during PDF generation: {}", e.what());
    }
    auto completionCallBack = p->callback;
    if (completionCallBack) completionCallBack(std::move(result));
}

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

PdfGenerator::PdfGenerator() : config_() {}

PdfGenerator::PdfGenerator(const PdfConfig& config) : config_(config) {}

PdfGenerator::~PdfGenerator() {}

// Call this ONCE from main() before creating any threads
bool PdfGenerator::initLibrary() {
    if (initialized_) return true;
    if (wkhtmltopdf_init(0) != 1) {
        LOG_ERROR("Failed to initialize wkhtmltopdf library");
        return false;
    }
    initialized_ = true;
    LOG_INFO("wkhtmltopdf library initialized");
    return true;
}

// Call this at application shutdown (optional)
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
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("wkhtmltopdf not initialized - call initLibrary() from main thread at startup");
        return false;
    }
    
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
    
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        return false;
    }
    
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    for (const auto& html : htmlPages) {
        wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
        wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
        wkhtmltopdf_set_object_setting(os, "footer.right", "Generated by PharmaPOS");
        wkhtmltopdf_set_object_setting(os, "footer.fontSize", "4");
        wkhtmltopdf_add_object(converter, os, html.c_str());
    }
    
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    if (!success) {
        LOG_ERROR("Multi-page PDF conversion failed");
    } else {
        LOG_INFO("Multi-page PDF generated: {}", outputPath);
    }
    
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

bool PdfGenerator::generateFromFile(const std::string& htmlPath, const std::string& outputPath) {
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
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("wkhtmltopdf not initialized - call initLibrary() from main thread at startup");
        return false;
    }
    
    wkhtmltopdf_global_settings* gs = wkhtmltopdf_create_global_settings();
    if (!gs) {
        LOG_ERROR("Failed to create global settings");
        return false;
    }
    
    if (!outputPath.empty()) {
        wkhtmltopdf_set_global_setting(gs, "out", outputPath.c_str());
    }
    
    wkhtmltopdf_set_global_setting(gs, "size.pageSize", config_.pageSize.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.top", config_.marginTop.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.bottom", config_.marginBottom.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.left", config_.marginLeft.c_str());
    wkhtmltopdf_set_global_setting(gs, "margin.right", config_.marginRight.c_str());
    
    wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
    if (!os) {
        LOG_ERROR("Failed to create object settings");
        return false;
    }
    
    if (config_.enableLocalFileAccess) {
        wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
    }
    wkhtmltopdf_set_object_setting(os, "footer.right", "Generated by PharmaPOS");
    wkhtmltopdf_set_object_setting(os, "footer.fontSize", "4");
    
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        return false;
    }
    
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    wkhtmltopdf_add_object(converter, os, htmlContent.c_str());
    
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    if (success && outputBuffer != nullptr) {
        const unsigned char* data = nullptr;
        long len = wkhtmltopdf_get_output(converter, &data);
        if (len > 0 && data != nullptr) {
            outputBuffer->assign(reinterpret_cast<const char*>(data), len);
        }
    }
    
    if (!success) {
        LOG_ERROR("PDF conversion failed");
    } else if (!outputPath.empty()) {
        LOG_INFO("PDF generated: {}", outputPath);
    }
    
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

bool PdfGenerator::doConvertWithSettings(const std::string& htmlContent, const std::string& outputPath,
                                          const PdfSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("wkhtmltopdf not initialized - call initLibrary() from main thread at startup");
        return false;
    }
    
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
    
    wkhtmltopdf_object_settings* os = wkhtmltopdf_create_object_settings();
    if (!os) {
        LOG_ERROR("Failed to create object settings");
        return false;
    }
    wkhtmltopdf_set_object_setting(os, "load.blockLocalFileAccess", "false");
    wkhtmltopdf_set_object_setting(os, "footer.right", "Generated by PharmaPOS");
    wkhtmltopdf_set_object_setting(os, "footer.fontSize", "4");
    
    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        return false;
    }
    
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    wkhtmltopdf_add_object(converter, os, htmlContent.c_str());
    
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    if (!success) {
        LOG_ERROR("PDF conversion failed");
    } else {
        LOG_INFO("PDF generated: {}", outputPath);
    }
    
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

} // namespace htmlToPDF
