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
#include <cstdio>
#include <unistd.h>
#include "logging.hpp"
#include "global.h"
#include "fmt/format.h"
wxDEFINE_EVENT(wpEVT_PDF_GENERATE, wxCommandEvent);
extern std::string GetVersionNo();

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
    completionCV.wait(lock, [&completed]() { return completed || global::g.isAppShuttingDown.load(); });

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
    
    LOG_INFO("Generating multi-page PDF with {} pages", htmlPages.size());

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
        wkhtmltopdf_set_object_setting(os, "footer.right", "Page [page] of [toPage]");
        wkhtmltopdf_set_object_setting(os, "footer.left", fmt::format("ppos {}", GetVersionNo()).c_str());
        if (!settings.footerCenter.empty())
            wkhtmltopdf_set_object_setting(os, "footer.center", settings.footerCenter.c_str());
        wkhtmltopdf_set_object_setting(os, "footer.fontSize", settings.footerFontSize.c_str());
        if (!settings.headerLeft.empty()) {
            wkhtmltopdf_set_object_setting(os, "header.left", settings.headerLeft.c_str());
        }
        if (!settings.headerCenter.empty()) {
            wkhtmltopdf_set_object_setting(os, "header.center", settings.headerCenter.c_str());
        }
        if (!settings.headerRight.empty()) {
            wkhtmltopdf_set_object_setting(os, "header.right", settings.headerRight.c_str());
        }
        wkhtmltopdf_set_object_setting(os, "header.fontSize", settings.headerFontSize.c_str());
        wkhtmltopdf_set_object_setting(os, "header.fontName", "Arial");
        wkhtmltopdf_set_object_setting(os, "header.spacing", "5");
        wkhtmltopdf_set_object_setting(os, "header.line", "");
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
    wkhtmltopdf_set_object_setting(os, "footer.right", "Page [page] of [toPage]");
    wkhtmltopdf_set_object_setting(os, "footer.left", fmt::format("ppos {}", GetVersionNo()).c_str());
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
    wkhtmltopdf_set_object_setting(os, "footer.right", "Page [page] of [toPage]");
    wkhtmltopdf_set_object_setting(os, "footer.left", fmt::format("ppos {}", GetVersionNo()).c_str());
    if (!settings.footerCenter.empty())
        wkhtmltopdf_set_object_setting(os, "footer.center", settings.footerCenter.c_str());
    wkhtmltopdf_set_object_setting(os, "footer.fontSize", settings.footerFontSize.c_str());

    std::string headerTempPath;
    bool useHtmlHeader = !settings.headerTitle.empty() || !settings.headerSubtitle.empty();
    LOG_INFO("doConvertWithSettings: headerTitle='{}', headerSubtitle='{}', useHtmlHeader={}",
             settings.headerTitle, settings.headerSubtitle, useHtmlHeader);
    if (useHtmlHeader) {
        // Multi-line header via temp HTML file with dynamic [section]/[subsection] support
        headerTempPath = fmt::format("/tmp/ppos_hdr_{}.html", getpid());
        std::ofstream hf(headerTempPath);
        if (hf.is_open()) {
            // HTML-escape static text to prevent & from rendering as %26
            auto htmlEscape = [](const std::string& s) -> std::string {
                std::string out;
                out.reserve(s.size());
                for (char c : s) {
                    switch (c) {
                        case '&': out += "&amp;"; break;
                        case '<': out += "&lt;"; break;
                        case '>': out += "&gt;"; break;
                        case '"': out += "&quot;"; break;
                        default: out += c;
                    }
                }
                return out;
            };

            hf << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><script>"
               << "function urldecode(s){"
               << "return s.replace(/%([0-9A-Fa-f]{2})/g,function(m,h){"
               << "return String.fromCharCode(parseInt(h,16));});}"
               << "function subst(){"
               << "var vars={};"
               << "var qs=document.location.search.substring(1);"
               << "if(qs){"
               << "var pairs=qs.split('&');"
               << "for(var i=0;i<pairs.length;i++){"
               << "var p=pairs[i].split('=',2);"
               << "var k=p[0];var v=p[1]||'';"
               << "v=urldecode(v);"
               << "vars[k]=v;"
               << "}"
               << "}"
               << "vars['_rawsec']=vars['section']||'NOVALUE';"
               << "var cs=['section','subsection','page','topage','_rawsec'];"
               << "for(var j=0;j<cs.length;j++){"
               << "var els=document.getElementsByClassName(cs[j]);"
               << "for(var k=0;k<els.length;k++){"
               << "var t=vars[cs[j]]||'';"
               << "t=t.replace(/%26/g,'&');"
               << "els[k].textContent=t;"
               << "}"
               << "}"
               << "}"
               << "</script><style>"
               << "*{margin:0;padding:0;box-sizing:border-box;}"
               << "body{font-family:Arial,sans-serif;font-size:" << settings.headerFontSize << "pt;padding:4px 0 4px 0;}"
               << ".l{float:left;}.r{float:right;white-space:nowrap;}"
               << ".t{display:block;clear:both;font-size:" << (std::stoi(settings.headerFontSize) - 1) << "pt;margin-top:2px;}"
               << ".s{display:block;clear:both;font-size:" << (std::stoi(settings.headerFontSize) - 2) << "pt;}"
               << ".sec{display:block;clear:both;font-size:" << (std::stoi(settings.headerFontSize) - 1) << "pt;margin-top:3px;word-wrap:break-word;}"
               << "</style></head><body onload=\"subst()\">"
               << "<div class=\"l\">" << htmlEscape(settings.headerLeft) << "</div>"
               << "<div class=\"r\">" << htmlEscape(settings.headerRight) << "</div>";
            if (!settings.headerTitle.empty())
                hf << "<div class=\"t\">" << htmlEscape(settings.headerTitle) << "</div>";
            if (!settings.headerSubtitle.empty())
                hf << "<div class=\"s\">" << htmlEscape(settings.headerSubtitle) << "</div>";
            if (!settings.headerCenter.empty()) {
                if (settings.headerCenter == "[section]")
                    hf << "<div class=\"sec\"><span class=\"section\"></span></div>";
                else
                    hf << "<div class=\"sec\">" << htmlEscape(settings.headerCenter) << "</div>";
            }
            hf << "</body></html>";
            hf.close();
            wkhtmltopdf_set_object_setting(os, "header.htmlUrl", headerTempPath.c_str());
            LOG_INFO("Header HTML written to {}", headerTempPath);
        } else {
            LOG_ERROR("Failed to write header HTML to {}", headerTempPath);
            useHtmlHeader = false;
        }
    }
    if (!useHtmlHeader) {
        // Fallback: single-line plain text header with native [section] token support
        if (!settings.headerLeft.empty())
            wkhtmltopdf_set_object_setting(os, "header.left", settings.headerLeft.c_str());
        if (!settings.headerCenter.empty()) {
            wkhtmltopdf_set_object_setting(os, "header.center", settings.headerCenter.c_str());
        }
        if (!settings.headerRight.empty())
            wkhtmltopdf_set_object_setting(os, "header.right", settings.headerRight.c_str());
        wkhtmltopdf_set_object_setting(os, "header.fontSize", settings.headerFontSize.c_str());
        wkhtmltopdf_set_object_setting(os, "header.fontName", "Arial");
    }
    wkhtmltopdf_set_object_setting(os, "header.spacing", "5");
    wkhtmltopdf_set_object_setting(os, "header.line", "");

    wkhtmltopdf_converter* converter = wkhtmltopdf_create_converter(gs);
    if (!converter) {
        LOG_ERROR("Failed to create PDF converter");
        if (!headerTempPath.empty()) std::remove(headerTempPath.c_str());
        return false;
    }
    
    wkhtmltopdf_set_error_callback(converter, pdfErrorCallback);
    wkhtmltopdf_set_warning_callback(converter, pdfWarningCallback);
    
    wkhtmltopdf_add_object(converter, os, htmlContent.c_str());
    
    bool success = (wkhtmltopdf_convert(converter) == 1);
    
    if (!headerTempPath.empty()) std::remove(headerTempPath.c_str());
    
    if (!success) {
        LOG_ERROR("PDF conversion failed");
    } else {
        LOG_INFO("PDF generated: {}", outputPath);
    }
    
    wkhtmltopdf_destroy_converter(converter);
    
    return success;
}

} // namespace htmlToPDF
