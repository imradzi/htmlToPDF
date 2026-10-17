#include "html_report_builder.h"
#include "pdf_generator.h"

#include <iostream>
#include <string>

// Standalone probe: links PdfGenerator without the application's version.cpp.
std::string GetVersionNo() { return "probe"; }

int main() {
    if (!htmlToPDF::PdfGenerator::initLibrary()) {
        std::cerr << "initLibrary failed\n";
        return 1;
    }

    HtmlReportBuilder builder("Monthly Sales Report", "Probe Outlet", "Portrait");
    builder.setSubtitle("Verification run");
    builder.addColumn("Region", 2.0, false);
    builder.addColumn("Sales", 1.0, true);

    auto& first = builder.newSection("Summary");
    (void)first;
    for (int i = 0; i < 40; ++i) {
        builder.addRow({"Region " + std::to_string(i + 1), std::to_string(1000 + i * 10) + ".00"});
    }
    builder.setPageTotal({"", "1,000.00"});

    builder.clearColumns();
    builder.addColumn("Item", 2.0, false);
    builder.addColumn("Qty", 1.0, true);
    builder.newSection("Details");
    for (int i = 0; i < 5; ++i) {
        builder.addRow({"Item " + std::to_string(i + 1), std::to_string(i + 1)});
    }
    builder.setPageTotal({"", "15"});

    const bool ok = builder.generatePdf("/tmp/report_probe.pdf");

    // Verify the in-memory OutputStream path as well.
    htmlToPDF::PdfGenerator generator;
    std::string buffer;
    const bool bufferOk = generator.generateToBuffer(
        "<!DOCTYPE html><html><body><p>Buffer test</p></body></html>", buffer);
    std::cout << "buffer bytes: " << buffer.size() << "\n";

    std::cout << (ok ? "report_probe ok" : "report_probe failed") << "\n";
    std::cout << (bufferOk && buffer.size() > 100 ? "buffer ok" : "buffer failed") << "\n";
    htmlToPDF::PdfGenerator::deinitLibrary();
    return ok ? 0 : 1;
}
