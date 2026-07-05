#include "html_report_builder.h"
#include "pdf_generator.h"
#include "template_engine.h"
#include "logging.hpp"
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
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
        margin-bottom: 10px;
        margin-top: 0px;
    }
    .report-subtitle {
        font-size: 8pt;
        margin-bottom: 4px;
        padding-bottom: 2px;
        margin-top: 0px;
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
        line-height: 1.4;
        vertical-align: top;
        word-wrap: break-word;
        overflow-wrap: break-word;
    }
    td:empty::after {
        content: '\00a0';
    }
    .text-right { text-align: right; white-space: nowrap; }
    .text-left { text-align: left; }
    .footer-row td {
        font-weight: bold;
        font-size: )" << fontSize_.data << R"(pt;
        border-top: 2px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
        border-bottom: 2px solid )" << colorToHex(theme_.boxColorRed, theme_.boxColorGreen, theme_.boxColorBlue) << R"(;
    }
    .grand-total-row td {
        font-weight: bold;
        font-size: )" << fontSize_.data << R"(pt;
        border-top: 2px solid #333;
        border-bottom: 2px solid #333;
    }
    td.sub-table-cell {
        padding: 0;
    }
    .section-break { page-break-before: always; }
    .page-title {
        font-size: )" << fontSize_.label << R"(pt;
        font-weight: bold;
        padding: 4px 0;
        margin: 0 0 2px 0;
    }
    .sub-table {
        width: 100%;
        border-collapse: collapse;
        margin: 0;
        table-layout: fixed;
    }
    .sub-table td {
        border: none;
        padding: 0px 2px;
        font-size: )" << fontSize_.data << R"(pt;
        word-wrap: break-word;
        overflow-wrap: break-word;
    }
    .sub-table .text-right {
        text-align: right;
        white-space: nowrap;
    }
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

        // Section title as h1 for wkhtmltopdf [section] token support
        if (!sec.pageTitle.empty()) {
            html << "  <h1 class=\"page-title\">" << sec.pageTitle << "</h1>\n";
        }

        // Hidden h2 with page total for wkhtmltopdf [subsection] footer token
        if (sec.hasPageTotal) {
            std::string pageTotalStr;
            for (size_t ci = 0; ci < columns_.size() && ci < sec.pageTotalCells.size(); ++ci) {
                if (!sec.pageTotalCells[ci].empty()) {
                    if (!pageTotalStr.empty()) pageTotalStr += " ";
                    pageTotalStr += sec.pageTotalCells[ci];
                }
            }
            if (!pageTotalStr.empty()) {
                html << "  <h2 style=\"visibility:hidden;height:0;overflow:hidden;margin:0;padding:0;font-size:0;line-height:0\">" << pageTotalStr << "</h2>\n";
            }
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
            bool hasSubTable = !row.subTableHtml.empty();
            for (size_t ci = startOfs; ci < row.cells.size() && ci < columns_.size(); ++ci) {
                // Skip detail columns covered by sub-table colspan
                if (hasSubTable && ci > (size_t)row.subTableStartCol && ci < (size_t)(row.subTableStartCol + row.subTableColspan))
                    continue;

                bool isSubTableCell = hasSubTable && (int)ci == row.subTableStartCol;
                std::string tdClass;
                if (isSubTableCell) {
                    tdClass = " class=\"sub-table-cell\"";
                } else if (columns_[ci].isNumber) {
                    tdClass = " class=\"text-right\"";
                }
                html << "      <td"
                     << tdClass
                     << (isSubTableCell ? fmt::format(" colspan=\"{}\"", row.subTableColspan) : "")
                     << ">";
                if (isSubTableCell) {
                    html << row.subTableHtml;
                } else {
                    html << row.cells[ci];
                }
                html << "</td>\n";
            }
            html << "    </tr>\n";
        }

        // Page total
        // Pre-compute detail column range for colspan in total rows (used by both page total and grand total)
        int totDetailStart = -1, totDetailEnd = -1, totDetailColspan = 0;
        if (hasKeyColumns()) {
            for (size_t dj = startOfs; dj < columns_.size(); ++dj) {
                if (dj < keyColumns_.size() && !keyColumns_[dj]) {
                    if (totDetailStart < 0) totDetailStart = (int)dj;
                    totDetailEnd = (int)dj;
                }
            }
            totDetailColspan = (totDetailStart >= 0) ? (totDetailEnd - totDetailStart + 1) : 0;
        }

        if (sec.hasPageTotal) {
            html << "    <tr class=\"footer-row\">\n";
            for (size_t ci = startOfs; ci < sec.pageTotalCells.size() && ci < columns_.size(); ++ci) {
                if (totDetailColspan > 0 && (int)ci > totDetailStart && (int)ci <= totDetailEnd) {
                    continue;  // covered by colspan cell
                }
                bool isDetailStart = (totDetailColspan > 0 && (int)ci == totDetailStart);
                html << "      <td"
                     << (columns_[ci].isNumber ? " class=\"text-right\"" : "")
                     << (isDetailStart ? fmt::format(" colspan=\"{}\"", totDetailColspan) : "")
                     << ">" << sec.pageTotalCells[ci] << "</td>\n";
            }
            html << "    </tr>\n";
        }

        html << "    </tbody>\n  </table>\n";

        // Grand total (only on last section)
        if (hasGrandTotal_ && si == sections_.size() - 1) {
            html << "  <table><tr class=\"grand-total-row\">\n";
            for (size_t ci = startOfs; ci < grandTotalCells_.size() && ci < columns_.size(); ++ci) {
                if (totDetailColspan > 0 && (int)ci > totDetailStart && (int)ci <= totDetailEnd) {
                    continue;
                }
                bool isDetailStart = (totDetailColspan > 0 && (int)ci == totDetailStart);
                html << "    <td style=\"width:" << fmt::format("{:.1f}", colWidths[ci])
                     << "%\""
                     << (columns_[ci].isNumber ? " class=\"text-right\"" : "")
                     << (isDetailStart ? fmt::format(" colspan=\"{}\"", totDetailColspan) : "")
                     << ">" << grandTotalCells_[ci] << "</td>\n";
            }
            html << "  </tr></table>\n";
        }

        html << "</div>\n";
    }

    html << "</body>\n</html>\n";
    return html.str();
}

bool HtmlReportBuilder::generatePdf(const std::string& outputPath) const {
    std::string htmlContent = renderHtml();

    // Format current date & time for header right
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream dateTimeStr;
    dateTimeStr << std::put_time(std::localtime(&t), "%d-%m-%Y %H:%M");

    htmlToPDF::PdfGenerator::PdfSettings settings;
    settings.orientation = isLandscape() ? "Landscape" : "Portrait";
    settings.pageSize = "A4";
    settings.marginTop = 15;
    settings.marginBottom = 10;
    settings.marginLeft = 10;
    settings.marginRight = 10;
    settings.headerLeft = outletName_;
    settings.headerCenter = "[section]";
    settings.headerRight = dateTimeStr.str();
    settings.headerTitle = title_;
    settings.headerSubtitle = subtitle_;
    settings.headerFontSize = "10";
    settings.footerCenter = "[subsection]";

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
