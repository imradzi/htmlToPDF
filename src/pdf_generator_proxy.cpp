#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

// The proxy dispatches conversions to the wx main thread and is only used by
// ppServer. Keeping it in a separate translation unit lets the standalone
// handlebars_pdf tool link PdfGenerator without pulling in `global::g`.
#include "pdf_generator.h"
#include "pdfevent.h"
#include "logging.hpp"
#include "global.h"

// The event must be defined in a translation unit compiled with RTTI. Defining
// it in pdf_generator.cpp (which is compiled with -fno-rtti for PlutoBook)
// produces a second, differently-sized wxObjectEventFunctor vtable.
wxDEFINE_EVENT(wpEVT_PDF_GENERATE, wxCommandEvent);

#include <condition_variable>
#include <functional>
#include <mutex>

namespace htmlToPDF {

wxEvtHandler* PdfGeneratorProxy::eventHandler_ = nullptr;
PdfGenerator PdfGeneratorProxy::generator_;

PdfGeneratorProxy::PdfGeneratorProxy() : config_() {}

PdfGeneratorProxy::PdfGeneratorProxy(const PdfConfig& config) : config_(config) {}

void PdfGeneratorProxy::SetEventHandler(wxEvtHandler* handler) { eventHandler_ = handler; }

bool PdfGeneratorProxy::generateFromHtml(const std::string& htmlContent, const std::string& outputPath,
                                         const PdfGenerator::PdfSettings& settings) {
    PdfGenerateRequest request;
    request.type = PdfGenerateRequest::RequestType::GenerateFromHtml;
    request.htmlContent = htmlContent;
    request.outputPath = outputPath;
    request.settings = settings;

    auto result = executeOnMainThread(request);
    return result.success;
}

bool PdfGeneratorProxy::generateMultiPagePdf(const std::vector<std::string>& htmlPages,
                                             const std::string& outputPath,
                                             const PdfGenerator::PdfSettings& settings) {
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

    EventData evData { request, [&completionMutex, &completionCV, &completed, &result](PdfGenerateResult&& res) {
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
    auto p = static_cast<EventData*>(event.GetClientData());
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
                result.success = generator_.generateFromHtml(request.htmlContent, request.outputPath,
                                                             request.settings);
                break;

            case PdfGenerateRequest::RequestType::GenerateMultiPage:
                LOG_INFO("PdfGeneratorProxy: Generating PDF from multiple HTML pages");
                result.success = generator_.generateMultiPagePdf(request.htmlPages, request.outputPath,
                                                                 request.settings);
                break;

            case PdfGenerateRequest::RequestType::GenerateToBuffer:
                LOG_INFO("PdfGeneratorProxy: Generating PDF to memory buffer");
                if (request.outputBuffer) {
                    result.success = generator_.generateToBuffer(
                        request.htmlContent, *const_cast<std::string*>(request.outputBuffer));
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

} // namespace htmlToPDF
