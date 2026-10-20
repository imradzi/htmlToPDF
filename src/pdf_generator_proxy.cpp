// PdfGeneratorProxy is kept for source compatibility with ppServer, but it no
// longer dispatches through wxWidgets. PlutoBook has no Qt-style main-thread
// requirement; running the conversion directly on the gRPC worker thread also
// avoids consuming the wx event loop and the main thread's smaller stack.
#include "pdf_generator.h"

#include "logging.hpp"

#include <exception>
#include <mutex>

namespace htmlToPDF {

PdfGenerator PdfGeneratorProxy::generator_;

PdfGeneratorProxy::PdfGeneratorProxy() : config_() {}

PdfGeneratorProxy::PdfGeneratorProxy(const PdfConfig& config) : config_(config) {}

bool PdfGeneratorProxy::generateFromHtml(const std::string& htmlContent, const std::string& outputPath,
                                         const PdfGenerator::PdfSettings& settings) {
    LOG_INFO("PdfGeneratorProxy: Generating PDF from HTML (worker thread)");
    try {
        const bool success = generator_.generateFromHtml(htmlContent, outputPath, settings);
        LOG_INFO("PdfGeneratorProxy: PDF from HTML finished (success={})", success);
        return success;
    } catch (const std::exception& e) {
        LOG_ERROR("PdfGeneratorProxy: PDF from HTML failed with exception: {}", e.what());
    } catch (...) {
        LOG_ERROR("PdfGeneratorProxy: PDF from HTML failed with an unknown exception");
    }
    return false;
}

bool PdfGeneratorProxy::generateMultiPagePdf(const std::vector<std::string>& htmlPages,
                                             const std::string& outputPath,
                                             const PdfGenerator::PdfSettings& settings) {
    LOG_INFO("PdfGeneratorProxy: Generating PDF from {} HTML pages (worker thread)",
             htmlPages.size());
    try {
        const bool success = generator_.generateMultiPagePdf(htmlPages, outputPath, settings);
        LOG_INFO("PdfGeneratorProxy: multi-page PDF finished (success={})", success);
        return success;
    } catch (const std::exception& e) {
        LOG_ERROR("PdfGeneratorProxy: multi-page PDF failed with exception: {}", e.what());
    } catch (...) {
        LOG_ERROR("PdfGeneratorProxy: multi-page PDF failed with an unknown exception");
    }
    return false;
}

bool PdfGeneratorProxy::generateToBuffer(const std::string& htmlContent, std::string& outputBuffer) {
    LOG_INFO("PdfGeneratorProxy: Generating PDF to memory buffer (worker thread)");
    try {
        const bool success = generator_.generateToBuffer(htmlContent, outputBuffer);
        LOG_INFO("PdfGeneratorProxy: PDF buffer finished (success={}, bytes={})", success,
                 outputBuffer.size());
        return success;
    } catch (const std::exception& e) {
        LOG_ERROR("PdfGeneratorProxy: PDF buffer failed with exception: {}", e.what());
    } catch (...) {
        LOG_ERROR("PdfGeneratorProxy: PDF buffer failed with an unknown exception");
    }
    return false;
}

} // namespace htmlToPDF
