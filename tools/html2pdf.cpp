// Standalone HTML -> PDF converter built on PdfGenerator.
//
// It exists to compare PlutoBook output against the old wkhtmltopdf output on
// identical input (see htmlToPDF/comparison), and to reproduce report issues
// without starting ppServer.
//
//   html2pdf <input.html> <output.pdf> [options]
//
//     --margin-top N        top margin in mm       (default 10)
//     --margin-bottom N     bottom margin in mm    (default 10)
//     --margin-left N       left margin in mm      (default 10)
//     --margin-right N      right margin in mm     (default 10)
//     --page-size NAME      A4, A3, A5, Letter, ... (default A4)
//     --landscape           landscape orientation
//     --scale X             content scale          (default 0.8, wkhtmltopdf)
//     --header-left TEXT    static header, left box
//     --header-center TEXT  static header, centre box ("[section]" = running)
//     --header-title TEXT   second header line
//     --header-subtitle TEXT third header line
//     --footer-center TEXT  centre footer text ("[subsection]" = running)

#include "pdf_generator.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// The standalone tools link PdfGenerator without the application's version.cpp.
std::string GetVersionNo() { return "standalone"; }

namespace {

bool ReadFile(const std::string& path, std::string& content) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    return true;
}

double ToNumber(const char* text, double fallback) {
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    return end == text ? fallback : value;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: html2pdf <input.html> <output.pdf> [--margin-top N] "
                     "[--margin-bottom N] [--margin-left N] [--margin-right N] "
                     "[--page-size NAME] [--landscape] [--scale X] "
                     "[--header-left TEXT] [--header-center TEXT] [--header-title TEXT] "
                     "[--header-subtitle TEXT] [--footer-center TEXT]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    htmlToPDF::PdfGenerator::PdfSettings settings;
    settings.marginTop = 10;
    settings.marginBottom = 10;
    settings.marginLeft = 10;
    settings.marginRight = 10;

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;

        if (arg == "--landscape") {
            settings.orientation = "Landscape";
        } else if (arg == "--margin-top" && hasValue) {
            settings.marginTop = static_cast<int>(ToNumber(argv[++i], 10));
        } else if (arg == "--margin-bottom" && hasValue) {
            settings.marginBottom = static_cast<int>(ToNumber(argv[++i], 10));
        } else if (arg == "--margin-left" && hasValue) {
            settings.marginLeft = static_cast<int>(ToNumber(argv[++i], 10));
        } else if (arg == "--margin-right" && hasValue) {
            settings.marginRight = static_cast<int>(ToNumber(argv[++i], 10));
        } else if (arg == "--page-size" && hasValue) {
            settings.pageSize = argv[++i];
        } else if (arg == "--scale" && hasValue) {
            settings.contentScale = ToNumber(argv[++i], settings.contentScale);
        } else if (arg == "--header-left" && hasValue) {
            settings.headerLeft = argv[++i];
        } else if (arg == "--header-center" && hasValue) {
            settings.headerCenter = argv[++i];
        } else if (arg == "--header-title" && hasValue) {
            settings.headerTitle = argv[++i];
        } else if (arg == "--header-subtitle" && hasValue) {
            settings.headerSubtitle = argv[++i];
        } else if (arg == "--footer-center" && hasValue) {
            settings.footerCenter = argv[++i];
        } else {
            std::cerr << "unknown or incomplete option: " << arg << "\n";
            return 2;
        }
    }

    std::string html;
    if (!ReadFile(inputPath, html)) {
        std::cerr << "cannot read " << inputPath << "\n";
        return 1;
    }

    if (!htmlToPDF::PdfGenerator::initLibrary()) {
        std::cerr << "initLibrary failed\n";
        return 1;
    }

    htmlToPDF::PdfGenerator generator;
    const bool ok = generator.generateFromHtml(html, outputPath, settings);

    htmlToPDF::PdfGenerator::deinitLibrary();

    std::cout << (ok ? "ok: " : "failed: ") << outputPath
              << " (scale " << settings.contentScale << ")\n";
    return ok ? 0 : 1;
}
