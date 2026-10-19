#include <stdexcept>

#include "pdf_generator.h"
#include <plutobook.hpp>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include "logging.hpp"
#include "fmt/format.h"
#ifdef _WIN32
#include <windows.h>
#endif
extern std::string GetVersionNo();

namespace htmlToPDF {

namespace {

constexpr double kMmToPt = 72.0 / 25.4;

double MmToPt(double mm) { return mm * kMmToPt; }

// ── wkhtmltopdf content scale ──────────────────────────────────
// wkhtmltopdf's default "smart shrinking" lays the document out in a 1/scale
// larger viewport and paints it at `scale`, so every CSS length (pt, mm, px)
// ends up at 80% of nominal while the page box - paper size, margins and the
// header/footer - keeps its real size. Every PharmaPOS template was tuned
// against that behaviour, so PdfSettings::contentScale defaults to 0.8.
constexpr double kMinContentScale = 0.2;
constexpr double kMaxContentScale = 2.0;

double EffectiveContentScale(const PdfGenerator::PdfSettings& settings) {
    const double scale = settings.contentScale > 0 ? settings.contentScale : 1.0;
    return std::clamp(scale, kMinContentScale, kMaxContentScale);
}

// Document-space factor: page size, margins and page-furniture font sizes are
// inflated by this so painting at contentScale yields the real paper size.
double ContentLayoutFactor(const PdfGenerator::PdfSettings& settings) {
    return 1.0 / EffectiveContentScale(settings);
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

// Parse a font size setting ("8", "8pt", "12px") into points.
double ParseFontSizePt(const std::string& value, double fallback) {
    if (value.empty()) return fallback;
    char* end = nullptr;
    const double number = std::strtod(value.c_str(), &end);
    if (end == value.c_str()) return fallback;
    const std::string unit = ToLower(Trim(std::string(end)));
    if (unit == "px") return number * 0.75;
    return number;  // pt or unitless
}

plutobook::PageSize ResolvePageSize(const std::string& name) {
    const std::string n = ToLower(Trim(name));
    if (n == "a3") return plutobook::PageSize::A3;
    if (n == "a5") return plutobook::PageSize::A5;
    if (n == "b4") return plutobook::PageSize::B4;
    if (n == "b5") return plutobook::PageSize::B5;
    if (n == "letter") return plutobook::PageSize::Letter;
    if (n == "legal") return plutobook::PageSize::Legal;
    if (n == "ledger") return plutobook::PageSize::Ledger;
    return plutobook::PageSize::A4;
}

bool IsLandscape(const std::string& orientation) {
    const std::string o = ToLower(Trim(orientation));
    return o == "landscape" || o == "l";
}

// Parse a CSS-ish length ("20mm", "1.5cm", "0.5in", "12pt", "px") into points.
double ParseLengthToPoints(const std::string& value, double fallbackPoints) {
    const std::string text = Trim(value);
    if (text.empty()) return fallbackPoints;

    char* end = nullptr;
    const double number = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) return fallbackPoints;

    const std::string unit = ToLower(Trim(std::string(end)));
    if (unit == "mm") return MmToPt(number);
    if (unit == "cm") return number * 72.0 / 2.54;
    if (unit == "in") return number * 72.0;
    if (unit == "px") return number * 72.0 / 96.0;
    if (unit == "pc") return number * 12.0;
    return number;  // pt or unitless
}

// Escape a value for use inside a CSS double-quoted string.
std::string CssString(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\A "; break;
            case '\r': break;
            case '\t': out += ' '; break;
            default:
                if (c < 0x20) out += ' ';
                else out += static_cast<char>(c);
                break;
        }
    }
    return out;
}

std::string MarginBoxRule(const char* box, const std::string& text, double fontSizePt,
                          const std::string& extraDeclarations = {}) {
    if (text.empty()) return {};
    std::string css = fmt::format("  @{} {{ content: \"{}\"; font-size: {:.2f}pt; white-space: pre-line;",
                                  box, CssString(text), fontSizePt);
    if (!extraDeclarations.empty()) css += " " + extraDeclarations + ";";
    css += " }\n";
    return css;
}

// Builds the @page rule that replaces wkhtmltopdf's header.* / footer.* settings.
// "[section]" and "[subsection]" are markers used by HtmlReportBuilder: the actual
// content is supplied by CSS running elements embedded in the report HTML.
std::string BuildPageStyle(const PdfGenerator::PdfSettings& settings,
                           uint32_t pageOffset = 0, uint32_t totalPages = 0) {
    const bool runningHeader = settings.headerCenter == "[section]";
    const bool runningFooter = settings.footerCenter == "[subsection]";
    // Page furniture lives in the scaled document too, so its sizes are
    // inflated to come out at their real physical size on the paper.
    const double k = ContentLayoutFactor(settings);

    plutobook::PageSize pageSize = ResolvePageSize(settings.pageSize);
    if (IsLandscape(settings.orientation)) pageSize = pageSize.landscape();

    const double headerPt = ParseFontSizePt(settings.headerFontSize, 8.0) * k;
    const double footerPt = ParseFontSizePt(settings.footerFontSize, 4.0) * k;

    std::string css;
    css.reserve(1024);

    // Templates size their full-page containers from these variables instead of
    // hard-coding 287mm, so the content area always matches the real page margins.
    css += fmt::format(
        ":root {{\n"
        "  --ppos-page-width: {:.3f}mm;\n"
        "  --ppos-page-height: {:.3f}mm;\n"
        "  --ppos-margin-top: {:.3f}mm;\n"
        "  --ppos-margin-bottom: {:.3f}mm;\n"
        "  --ppos-margin-left: {:.3f}mm;\n"
        "  --ppos-margin-right: {:.3f}mm;\n"
        "}}\n",
        pageSize.width() / kMmToPt * k, pageSize.height() / kMmToPt * k,
        settings.marginTop * k, settings.marginBottom * k,
        settings.marginLeft * k, settings.marginRight * k);

    if (totalPages > 0) {
        // Continue numbering across documents: reset only on this document's
        // first page, then let the page counter increment normally.
        css += fmt::format("  @page :nth(1) {{ counter-reset: page {}; counter-increment: page 1; }}\n",
                           pageOffset);
    }

    // wkhtmltopdf bottom-aligns the header block inside the top margin and
    // leaves header.spacing (5mm) plus the header template's 4px padding above
    // the content area; its footer starts right at the content bottom. Keeping
    // the same alignment restores the top margin the old output had.
    const double headerGapMm = (5.0 + 4.0 * 0.75 / kMmToPt) * k;

    css += "@page {\n";

    if (runningHeader) {
        css += fmt::format("  @top-center {{ content: element(page-header); width: 100%; "
                           "vertical-align: bottom; padding-bottom: {:.2f}mm; }}\n", headerGapMm);
    } else {
        const std::string align = fmt::format("vertical-align: bottom; padding-bottom: {:.2f}mm",
                                              headerGapMm);
        css += MarginBoxRule("top-left", settings.headerLeft, headerPt, align);

        std::string center = settings.headerCenter;
        auto appendLine = [&center](const std::string& line) {
            if (line.empty()) return;
            if (!center.empty()) center += '\n';
            center += line;
        };
        appendLine(settings.headerTitle);
        appendLine(settings.headerSubtitle);
        css += MarginBoxRule("top-center", center, headerPt, align);
        css += MarginBoxRule("top-right", settings.headerRight, headerPt, align);
    }

    if (runningFooter) {
        css += fmt::format("  @bottom-center {{ content: element(page-footer); font-size: {:.2f}pt; "
                           "vertical-align: top; }}\n", footerPt);
    } else {
        css += MarginBoxRule("bottom-center", settings.footerCenter, footerPt, "vertical-align: top");
    }

    css += MarginBoxRule("bottom-left", fmt::format("ppos {}", GetVersionNo()), footerPt,
                         "vertical-align: top");
    if (totalPages > 0) {
        css += fmt::format("  @bottom-right {{ content: \"Page \" counter(page) \" of {}\"; "
                           "font-size: {:.2f}pt; vertical-align: top; }}\n", totalPages, footerPt);
    } else {
        css += fmt::format("  @bottom-right {{ content: \"Page \" counter(page) \" of \" counter(pages); "
                           "font-size: {:.2f}pt; vertical-align: top; }}\n", footerPt);
    }
    css += "}\n";

    if (runningHeader) {
        // HtmlReportBuilder leaves .po-header unsized (its children use em), so
        // the legacy header font size can be applied here.
        css += fmt::format(".po-header {{ font-size: {:.2f}pt; }}\n", headerPt);
    }
    return css;
}

class StringOutputStream final : public plutobook::OutputStream {
public:
    explicit StringOutputStream(std::string& output) : output_(output) {}

    bool write(const char* data, size_t length) override {
        output_.append(data, length);
        return true;
    }

private:
    std::string& output_;
};

// Real paper size, i.e. what the PDF pages must end up being.
plutobook::PageSize ResolveOutputPageSize(const PdfGenerator::PdfSettings& settings) {
    plutobook::PageSize pageSize = ResolvePageSize(settings.pageSize);
    return IsLandscape(settings.orientation) ? pageSize.landscape() : pageSize;
}

// Page size handed to the layout engine: inflated by 1/contentScale so that
// painting at contentScale reproduces the real paper size.
plutobook::PageSize ResolveBookPageSize(const PdfGenerator::PdfSettings& settings) {
    const plutobook::PageSize pageSize = ResolveOutputPageSize(settings);
    const double k = ContentLayoutFactor(settings);
    return plutobook::PageSize(static_cast<float>(pageSize.width() * k),
                               static_cast<float>(pageSize.height() * k));
}

plutobook::PageMargins ResolveMargins(const PdfGenerator::PdfSettings& settings) {
    const double k = ContentLayoutFactor(settings);
    return plutobook::PageMargins(MmToPt(settings.marginTop * k), MmToPt(settings.marginRight * k),
                                  MmToPt(settings.marginBottom * k), MmToPt(settings.marginLeft * k));
}

std::string MakeFileUrl(const std::filesystem::path& path) {
    const std::string generic = path.generic_string();
#ifdef _WIN32
    return "file:///" + generic;
#else
    return "file://" + generic;
#endif
}

// wkhtmltopdf resolved relative resources against the process working
// directory; keep that behaviour when no explicit base URL is supplied.
std::string ResolveBaseUrl(const PdfGenerator::PdfSettings& settings) {
    if (!settings.baseUrl.empty()) return settings.baseUrl;

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) return {};

    std::string baseUrl = MakeFileUrl(cwd);
    if (!baseUrl.empty() && baseUrl.back() != '/') baseUrl += '/';
    return baseUrl;
}

// Paints every page of the given books onto `canvas`, applying the PlutoBook
// px->pt conversion on top of the wkhtmltopdf-compatible content scale. The
// page size is scaled back down so the PDF keeps the real paper size.
void PaintBooks(const std::vector<plutobook::Book*>& books, plutobook::PDFCanvas& canvas,
                double contentScale) {
    const auto scale = static_cast<float>(contentScale);
    canvas.scale(plutobook::units::px * scale, plutobook::units::px * scale);

    for (auto* book : books) {
        const uint32_t pageCount = book->pageCount();
        for (uint32_t index = 0; index < pageCount; ++index) {
            const plutobook::PageSize pageSize = book->pageSizeAt(index);
            canvas.setPageSize(plutobook::PageSize(pageSize.width() * scale,
                                                   pageSize.height() * scale));
            book->renderPage(canvas, index);
            canvas.showPage();
        }
    }
    canvas.finish();
}

bool ConvertToPdf(const std::string& htmlContent, const std::string& outputPath,
                  std::string* outputBuffer, const PdfGenerator::PdfSettings& settings) {
    if (outputBuffer == nullptr && outputPath.empty()) {
        LOG_ERROR("PlutoBook: no output path or buffer provided");
        return false;
    }

    plutobook::Book book(ResolveBookPageSize(settings), ResolveMargins(settings),
                         plutobook::MediaType::Print);

    const std::string baseUrl = ResolveBaseUrl(settings);
    const std::string userStyle = BuildPageStyle(settings);
    if (!book.loadHtml(htmlContent, userStyle, {}, baseUrl)) {
        LOG_ERROR("PlutoBook: failed to load HTML document");
        return false;
    }

    const plutobook::PageSize outputPageSize = ResolveOutputPageSize(settings);
    const double contentScale = EffectiveContentScale(settings);

    // A PDFCanvas is used instead of Book::writeToPdf so the content scale can
    // be folded into the canvas transform.
    std::unique_ptr<StringOutputStream> stream;
    if (outputBuffer != nullptr) {
        outputBuffer->clear();
        stream = std::make_unique<StringOutputStream>(*outputBuffer);
    }

    std::unique_ptr<plutobook::PDFCanvas> canvas;
    if (stream) canvas = std::make_unique<plutobook::PDFCanvas>(*stream, outputPageSize);
    else canvas = std::make_unique<plutobook::PDFCanvas>(outputPath, outputPageSize);

    if (canvas->isNull()) {
        LOG_ERROR("PlutoBook: failed to create PDF canvas for {}", outputBuffer ? "<buffer>" : outputPath);
        return false;
    }

    if (book.pageCount() == 0) {
        LOG_ERROR("PlutoBook: document produced no pages");
        return false;
    }

    PaintBooks({&book}, *canvas, contentScale);

    LOG_INFO("PDF generated: {} ({} pages)", outputBuffer ? "<buffer>" : outputPath, book.pageCount());
    return true;
}

} // namespace


bool PdfGenerator::initialized_ = false;
std::mutex PdfGenerator::mutex_;

PdfGenerator::PdfGenerator() : config_() {}

PdfGenerator::PdfGenerator(const PdfConfig& config) : config_(config) {}

PdfGenerator::~PdfGenerator() {}

// Call this ONCE from main() before creating any threads. PlutoBook has no
// process-wide GUI initialization; on Windows this only points Fontconfig at
// the config directory shipped next to the executable.
bool PdfGenerator::initLibrary() {
    if (initialized_) return true;

#ifdef _WIN32
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0) {
        std::error_code ec;
        const std::filesystem::path fontsDir =
            std::filesystem::path(modulePath).parent_path() / "fonts";
        if (std::filesystem::exists(fontsDir, ec)) {
            plutobook_set_fontconfig_path(fontsDir.string().c_str());
            LOG_INFO("PlutoBook: fontconfig path set to {}", fontsDir.string());
        } else {
            LOG_WARN("PlutoBook: fontconfig directory not found at {}", fontsDir.string());
        }
    }
#endif

    initialized_ = true;
    LOG_INFO("PlutoBook PDF engine initialized");
    return true;
}

// Call this at application shutdown (optional)
void PdfGenerator::deinitLibrary() {
    initialized_ = false;
}

bool PdfGenerator::generate(const std::string& htmlContent, const std::string& outputPath) {
    return doConvert(htmlContent, outputPath, nullptr);
}

bool PdfGenerator::generateFromHtml(const std::string& htmlContent, const std::string& outputPath, const PdfSettings& settings) {
    return doConvertWithSettings(htmlContent, outputPath, settings);
}

bool PdfGenerator::generateMultiPagePdf(const std::vector<std::string>& htmlPages, const std::string& outputPath, const PdfSettings& settings) {
    if (htmlPages.empty()) return false;

    LOG_INFO("Generating multi-page PDF with {} documents", htmlPages.size());

    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("PlutoBook not initialized - call initLibrary() at startup");
        return false;
    }

    std::vector<std::string> styles(htmlPages.size(), BuildPageStyle(settings));
    const std::string baseUrl = ResolveBaseUrl(settings);

    if (settings.continuePageNumbering) {
        // First pass: paginate once to learn each document's page count.
        std::vector<uint32_t> pageCounts(htmlPages.size(), 0);
        uint32_t totalPages = 0;
        for (size_t i = 0; i < htmlPages.size(); ++i) {
            plutobook::Book book(ResolveBookPageSize(settings), ResolveMargins(settings),
                                 plutobook::MediaType::Print);
            if (!book.loadHtml(htmlPages[i], styles[i], {}, baseUrl)) {
                LOG_ERROR("PlutoBook: failed to load one of the multi-page documents");
                return false;
            }
            pageCounts[i] = book.pageCount();
            totalPages += pageCounts[i];
        }

        // Second pass: give each document a page-counter offset so numbering
        // continues across the whole report.
        uint32_t offset = 0;
        for (size_t i = 0; i < htmlPages.size(); ++i) {
            styles[i] = BuildPageStyle(settings, offset, totalPages);
            offset += pageCounts[i];
        }
    }

    std::vector<std::unique_ptr<plutobook::Book>> books;
    books.reserve(htmlPages.size());

    for (size_t i = 0; i < htmlPages.size(); ++i) {
        auto book = std::make_unique<plutobook::Book>(ResolveBookPageSize(settings),
                                                      ResolveMargins(settings),
                                                      plutobook::MediaType::Print);
        if (!book->loadHtml(htmlPages[i], styles[i], {}, baseUrl)) {
            LOG_ERROR("PlutoBook: failed to load one of the multi-page documents");
            return false;
        }
        if (book->pageCount() == 0) continue;
        books.push_back(std::move(book));
    }

    if (books.empty()) {
        LOG_ERROR("PlutoBook: multi-page conversion produced no pages");
        return false;
    }

    // Concatenate every document's pages onto a single PDF surface, preserving
    // each document's own page size (portrait/landscape).
    plutobook::PDFCanvas canvas(outputPath, ResolveOutputPageSize(settings));
    if (canvas.isNull()) {
        LOG_ERROR("PlutoBook: failed to create PDF canvas for {}", outputPath);
        return false;
    }

    std::vector<plutobook::Book*> pages;
    pages.reserve(books.size());
    for (auto& book : books) pages.push_back(book.get());
    PaintBooks(pages, canvas, EffectiveContentScale(settings));

    LOG_INFO("Multi-page PDF generated: {} ({} documents)", outputPath, books.size());
    return true;
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
        LOG_ERROR("PlutoBook not initialized - call initLibrary() at startup");
        return false;
    }

    // Map the legacy PdfConfig (string lengths) onto PdfSettings (millimetres).
    PdfSettings settings;
    settings.pageSize = config_.pageSize;
    settings.orientation = "Portrait";
    settings.marginTop = static_cast<int>(std::lround(ParseLengthToPoints(config_.marginTop, MmToPt(20)) / kMmToPt));
    settings.marginBottom = static_cast<int>(std::lround(ParseLengthToPoints(config_.marginBottom, MmToPt(20)) / kMmToPt));
    settings.marginLeft = static_cast<int>(std::lround(ParseLengthToPoints(config_.marginLeft, MmToPt(15)) / kMmToPt));
    settings.marginRight = static_cast<int>(std::lround(ParseLengthToPoints(config_.marginRight, MmToPt(15)) / kMmToPt));
    settings.footerFontSize = "4";

    // PlutoBook always permits file: URLs for local images; the old
    // config_.enableLocalFileAccess switch has no equivalent and is unused.

    return ConvertToPdf(htmlContent, outputPath, outputBuffer, settings);
}

bool PdfGenerator::doConvertWithSettings(const std::string& htmlContent, const std::string& outputPath,
                                          const PdfSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        LOG_ERROR("PlutoBook not initialized - call initLibrary() at startup");
        return false;
    }

    return ConvertToPdf(htmlContent, outputPath, nullptr, settings);
}

} // namespace htmlToPDF
