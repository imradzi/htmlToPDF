#include "html_report_builder.h"
#include "pdf_generator.h"
#include "template_engine.h"
#include "logging.hpp"
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <iomanip>
#include <filesystem>

HtmlReportBuilder::HtmlReportBuilder(const std::string& title, const std::string& outletName, const std::string& orientation)
    : title_(title)
    , outletName_(outletName)
    , orientation_(orientation) {
}

bool HtmlReportBuilder::isLandscape() const {
    return boost::iequals(orientation_, "Landscape") || boost::iequals(orientation_, "L");
}

void HtmlReportBuilder::addColumn(const std::string& name, double weightage, bool isNumber, const std::string& sumFunction) {
    ColumnDef col;
    col.name = name;
    col.weightage = weightage;
    col.isNumber = isNumber;
    col.sumFunction = sumFunction;
    columns_.push_back(std::move(col));
}

HtmlReportBuilder::Section& HtmlReportBuilder::newSection(const std::string& pageTitle) {
    Section sec;
    sec.title = title_;
    sec.subtitle = subtitle_;
    sec.pageTitle = pageTitle;
    sec.pageNo = static_cast<int>(sections_.size()) + 1;
    sections_.push_back(std::move(sec));
    currentSection_ = &sections_.back();
    pageCount_ = static_cast<int>(sections_.size());
    return *currentSection_;
}

void HtmlReportBuilder::addRow(const std::vector<std::string>& cellValues) {
    if (!currentSection_) {
        newSection();
    }
    RowData row;
    row.cells = cellValues;
    currentSection_->rows.push_back(std::move(row));
}

void HtmlReportBuilder::addRow(const RowData& row) {
    if (!currentSection_) {
        newSection();
    }
    currentSection_->rows.push_back(row);
}

void HtmlReportBuilder::setPageTotal(const std::vector<std::string>& totalCells) {
    if (currentSection_) {
        currentSection_->pageTotalCells = totalCells;
        currentSection_->hasPageTotal = true;
    }
}

void HtmlReportBuilder::setGrandTotal(const std::vector<std::string>& totalCells) {
    grandTotalCells_ = totalCells;
    hasGrandTotal_ = true;
}

void HtmlReportBuilder::setNoData(const std::string& text) {
    noDataText_ = text;
    noData_ = true;
}

std::string HtmlReportBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    return fmt::format("#{:02x}{:02x}{:02x}", r, g, b);
}

std::string HtmlReportBuilder::formatNumber(double value, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    return oss.str();
}

TemplateContext HtmlReportBuilder::buildContext() const {
    TemplateContext ctx;

    // Page settings
    ctx.variables["page_size"] = isLandscape() ? "A4 landscape" : "A4";
    ctx.variables["orientation"] = isLandscape() ? "landscape" : "portrait";
    ctx.variables["is_landscape"] = isLandscape() ? "1" : "";
    ctx.variables["font_size"] = std::to_string(fontSize_.data);
    ctx.variables["label_font_size"] = std::to_string(fontSize_.label);
    ctx.variables["data_font_size"] = std::to_string(fontSize_.data);
    ctx.variables["total_font_size"] = std::to_string(fontSize_.total);
    ctx.variables["footer_font_size"] = std::to_string(fontSize_.footer);

    // Colors
    ctx.variables["header_fill_color"] = colorToHex(theme_.fillColorRed, theme_.fillColorGreen, theme_.fillColorBlue);
    ctx.variables["box_color"] = colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue);

    // Custom CSS
    if (!customCss_.empty()) {
        ctx.variables["custom_css"] = customCss_;
    }

    // No data
    if (noData_) {
        ctx.variables["no_data"] = "1";
        ctx.variables["no_data_text"] = noDataText_;
    }

    // Compute column widths as percentages
    double totalWeightage = 0;
    for (const auto& col : columns_) {
        totalWeightage += col.weightage;
    }

    // Build sections
    std::vector<Item> sectionItems;
    for (size_t si = 0; si < sections_.size(); ++si) {
        const auto& sec = sections_[si];
        Item sectionItem;
        sectionItem.fields["title"] = sec.title;
        sectionItem.fields["subtitle"] = sec.subtitle;
        sectionItem.fields["date"] = ""; // Will be filled by caller or left as current date
        sectionItem.fields["page_no"] = std::to_string(sec.pageNo);
        sectionItem.fields["section_class"] = (si > 0) ? "section-break" : "";
        sectionItem.fields["outlet_name"] = outletName_;
        sectionItem.fields["show_page_no"] = showFooterPageNo_ ? "1" : "";

        if (!sec.pageTitle.empty()) {
            sectionItem.fields["page_title"] = sec.pageTitle;
        }

        // Build columns sub-list (we embed as a serialized format since TemplateEngine
        // doesn't support nested {{#each}}. We'll use raw HTML injection instead.)
        // Actually, our template uses {{#each columns}} within {{#each sections}}.
        // The TemplateEngine doesn't support nested each blocks, so we need to
        // pre-render the inner HTML for columns and rows.
        // We'll do this by building the HTML directly for each section.

        sectionItems.push_back(std::move(sectionItem));
    }
    ctx.lists["sections"] = sectionItems;

    return ctx;
}

std::string HtmlReportBuilder::renderHtml() const {
    // Since TemplateEngine doesn't support nested {{#each}} blocks,
    // we build the HTML directly for maximum flexibility.

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    @page {
        size: )" << (isLandscape() ? "A4 landscape" : "A4") << R"(;
        margin: 10mm 10mm 15mm 10mm;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        font-family: Arial, sans-serif;
        font-size: )" << fontSize_.data << R"(pt;
        color: #333;
    }
    .header-row {
        display: flex;
        justify-content: space-between;
        align-items: baseline;
        margin-bottom: 2px;
    }
    .report-title { font-size: 10pt; }
    .report-subtitle { font-size: 10pt; }
    .report-date { font-size: 10pt; text-align: right; }
    .page-title {
        font-size: 8pt;
        margin-bottom: 4px;
        border-bottom: 1px solid #333;
        padding-bottom: 2px;
    }
    table {
        width: 100%;
        border-collapse: collapse;
        table-layout: fixed;
    }
    th {
        background: )" << colorToHex(theme_.fillColorRed, theme_.fillColorGreen, theme_.fillColorBlue) << R"(;
        color: #333;
        font-size: )" << fontSize_.label << R"(pt;
        font-weight: bold;
        text-align: left;
        padding: 2px 3px;
        border: 1px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    td {
        padding: 1px 3px;
        border: 1px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
        font-size: )" << fontSize_.data << R"(pt;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .text-right { text-align: right; }
    .text-left { text-align: left; }
    .footer-row td {
        font-weight: bold;
        font-size: )" << fontSize_.total << R"(pt;
        border-top: 2px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
        border-bottom: 2px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
    }
    .grand-total-row td {
        font-weight: bold;
        font-size: )" << fontSize_.total << R"(pt;
        border-top: 2px solid #333;
        border-bottom: 2px solid #333;
    }
    .page-footer {
        font-size: )" << fontSize_.footer << R"(pt;
        color: #666;
        margin-top: 4px;
        overflow: hidden;
    }
    .page-footer-left { float: left; }
    .page-footer-right { float: right; }
    .section-break { page-break-before: always; }
)";

    if (!customCss_.empty()) {
        html << customCss_ << "\n";
    }

    html << R"(</style>
</head>
<body>
)";

    // Compute column widths as percentages
    double totalWeightage = 0;
    for (const auto& col : columns_) {
        totalWeightage += col.weightage;
    }
    std::vector<double> colWidths;
    for (const auto& col : columns_) {
        colWidths.push_back(totalWeightage > 0 ? (col.weightage / totalWeightage * 100.0) : 0);
    }

    int startOfs = breakPageOn_ ? 1 : 0;

    if (noData_ && sections_.empty()) {
        html << R"(<div style="text-align:center; margin-top: 40mm;">
    <p style="font-size: 12pt;">No Data for:</p>
    <p style="font-size: 12pt; margin-top: 10mm;">)" << noDataText_ << R"(</p>
</div>
)";
    }

    for (size_t si = 0; si < sections_.size(); ++si) {
        const auto& sec = sections_[si];
        html << "<div" << (si > 0 ? " class=\"section-break\"" : "") << ">\n";

        // Header
        html << "  <div class=\"header-row\">\n";
        html << "    <span class=\"report-title\">" << sec.title << "</span>\n";
        html << "    <span class=\"report-date\">" << sec.subtitle << "</span>\n";
        html << "  </div>\n";

        if (!sec.pageTitle.empty()) {
            html << "  <div class=\"page-title\">" << sec.pageTitle << "</div>\n";
        }

        // Table
        html << "  <table>\n    <thead><tr>\n";
        for (size_t ci = startOfs; ci < columns_.size(); ++ci) {
            const auto& col = columns_[ci];
            html << "      <th style=\"width:" << fmt::format("{:.1f}", colWidths[ci])
                 << "%\"" << (col.isNumber ? " class=\"text-right\"" : "") << ">"
                 << col.name << "</th>\n";
        }
        html << "    </tr></thead>\n    <tbody>\n";

        // Data rows
        for (const auto& row : sec.rows) {
            html << "    <tr>\n";
            for (size_t ci = startOfs; ci < row.cells.size() && ci < columns_.size(); ++ci) {
                html << "      <td" << (columns_[ci].isNumber ? " class=\"text-right\"" : "")
                     << ">" << row.cells[ci] << "</td>\n";
            }
            html << "    </tr>\n";
        }

        // Page total
        if (sec.hasPageTotal) {
            html << "    <tr class=\"footer-row\">\n";
            for (size_t ci = startOfs; ci < sec.pageTotalCells.size() && ci < columns_.size(); ++ci) {
                html << "      <td" << (columns_[ci].isNumber ? " class=\"text-right\"" : "")
                     << ">" << sec.pageTotalCells[ci] << "</td>\n";
            }
            html << "    </tr>\n";
        }

        html << "    </tbody>\n  </table>\n";

        // Grand total (only on last section)
        if (hasGrandTotal_ && si == sections_.size() - 1) {
            html << "  <table><tr class=\"grand-total-row\">\n";
            for (size_t ci = startOfs; ci < grandTotalCells_.size() && ci < columns_.size(); ++ci) {
                html << "    <td style=\"width:" << fmt::format("{:.1f}", colWidths[ci])
                     << "%\"" << (columns_[ci].isNumber ? " class=\"text-right\"" : "")
                     << ">" << grandTotalCells_[ci] << "</td>\n";
            }
            html << "  </tr></table>\n";
        }

        // Footer
        html << "  <div class=\"page-footer\">\n";
        html << "    <span class=\"page-footer-left\">" << outletName_ << "</span>\n";
        if (showFooterPageNo_) {
            html << "    <span class=\"page-footer-right\">Page " << sec.pageNo << "</span>\n";
        }
        html << "  </div>\n";

        html << "</div>\n";
    }

    html << "</body>\n</html>\n";
    return html.str();
}

bool HtmlReportBuilder::generatePdf(const std::string& outputPath) const {
    std::string htmlContent = renderHtml();

    htmlToPDF::PdfGenerator::PdfSettings settings;
    settings.orientation = isLandscape() ? "Landscape" : "Portrait";
    settings.pageSize = "A4";
    settings.marginTop = 10;
    settings.marginBottom = 10;
    settings.marginLeft = 10;
    settings.marginRight = 10;

    htmlToPDF::PdfGeneratorProxy proxy;
    return proxy.generateFromHtml(htmlContent, outputPath, settings);
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
