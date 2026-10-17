#include "html_report_builder.h"
#include "pdf_generator.h"

#include <string>
#include <vector>

// PDF output helpers live in their own translation unit so that callers which
// only render HTML (e.g. tools/report_probe) do not pull in PdfGeneratorProxy
// and the application-wide global state it depends on.
bool HtmlReportBuilder::generatePdf(const std::string& outputPath) const {
    htmlToPDF::PdfGenerator::PdfSettings settings;
    settings.orientation = isLandscape() ? "Landscape" : "Portrait";
    settings.pageSize = "A4";
    settings.marginTop = 25;   // room for the running multi-line header
    settings.marginBottom = 10;
    settings.marginLeft = 10;
    settings.marginRight = 10;
    // Markers: PdfGenerator maps these to the CSS running elements emitted by
    // renderHtmlImpl(). Each section is rendered as its own document because
    // PlutoBook running elements are document-wide, not page-dynamic.
    settings.headerCenter = "[section]";
    settings.footerCenter = "[subsection]";
    // Reports previously used a single wkhtmltopdf object, so [page]/[toPage]
    // ran across the whole report; keep that behaviour.
    settings.continuePageNumbering = true;

    htmlToPDF::PdfGenerator generator;

    if (sections_.empty()) {
        return generator.generateFromHtml(renderHtml(), outputPath, settings);
    }

    std::vector<std::string> documents;
    documents.reserve(sections_.size());
    for (size_t i = 0; i < sections_.size(); ++i) {
        documents.push_back(renderSectionHtml(i));
    }

    if (documents.size() == 1) {
        return generator.generateFromHtml(documents.front(), outputPath, settings);
    }
    return generator.generateMultiPagePdf(documents, outputPath, settings);
}

bool HtmlReportBuilder::saveAsFile(const std::wstring& filePath) const {
    // Convert wstring path to string (UTF-8)
    std::string path;
    for (auto c : filePath) {
        if (c < 128) path += static_cast<char>(c);
        else {
            // Simple UTF-8 encoding for non-ASCII
            if (c < 0x800) {
                path += static_cast<char>(0xC0 | (c >> 6));
                path += static_cast<char>(0x80 | (c & 0x3F));
            } else {
                path += static_cast<char>(0xE0 | (c >> 12));
                path += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                path += static_cast<char>(0x80 | (c & 0x3F));
            }
        }
    }
    return generatePdf(path);
}
