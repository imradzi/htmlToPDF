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
#include <cctype>
#include <cstdlib>
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
    sec.columns = columns_;  // snapshot the current column layout for this section
    sec.breakPage = breakPageOn_;  // and the break-page setting it was created with
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
    return renderHtmlImpl(0, sections_.size());
}

std::string HtmlReportBuilder::renderSectionHtml(size_t sectionIndex) const {
    if (sectionIndex >= sections_.size()) return {};
    return renderHtmlImpl(sectionIndex, sectionIndex + 1);
}

std::string HtmlReportBuilder::renderHtmlImpl(size_t firstSection, size_t lastSection) const {
    // Since TemplateEngine doesn't support nested {{#each}} blocks,
    // we build the HTML directly for maximum flexibility.

    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::ostringstream headerDate;
    headerDate << std::put_time(&localTime, "%d-%m-%Y %H:%M");

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    /* Page size and margins are supplied by PdfGenerator (PdfSettings). */
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
        white-space: normal;
        word-wrap: break-word;
        overflow-wrap: break-word;
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
    th.text-right { white-space: normal; }
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
    .section-break { page-break-before: always; break-before: page; }
    .po-header {
        font-family: Arial, sans-serif;
    }
    /* Sizes are em-relative: PdfGenerator injects the legacy wkhtmltopdf header
       font size on .po-header, so these keep the old 8/7/6/7pt ratios. */
    .po-header .row { overflow: hidden; }
    .po-header .left { float: left; }
    .po-header .right { float: right; white-space: nowrap; }
    .po-header .title { clear: both; font-size: 0.875em; margin-top: 2px; }
    .po-header .subtitle { clear: both; font-size: 0.75em; }
    .po-header .section { clear: both; font-size: 0.875em; margin-top: 3px; overflow-wrap: break-word; }
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

    auto escapeHtml = [](const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (char c : text) {
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

    // PlutoBook replaces wkhtmltopdf's [section]/[subsection] tokens with CSS
    // running elements. They are taken out of the normal flow and rendered in
    // the @top-center / @bottom-center page margin boxes (see PdfGenerator).
    auto appendRunningHeader = [&](const std::string& sectionTitle) {
        html << "<div class=\"po-header\" style=\"position: running(page-header);\">\n";
        html << "  <div class=\"row\"><div class=\"left\">" << escapeHtml(outletName_)
             << "</div><div class=\"right\">" << escapeHtml(headerDate.str()) << "</div></div>\n";
        if (!title_.empty())
            html << "  <div class=\"title\">" << escapeHtml(title_) << "</div>\n";
        if (!subtitle_.empty())
            html << "  <div class=\"subtitle\">" << escapeHtml(subtitle_) << "</div>\n";
        if (!sectionTitle.empty())
            html << "  <div class=\"section\">" << escapeHtml(sectionTitle) << "</div>\n";
        html << "</div>\n";
    };

    auto appendRunningFooter = [&](const std::string& total) {
        html << "<div class=\"po-footer\" style=\"position: running(page-footer);\">"
             << escapeHtml(total) << "</div>\n";
    };

    // Compute column widths as percentages. Columns may change between sections
    // (reports can have several result sets with different column layouts), so
    // widths are computed from each section's own column definitions below.
    auto computeColWidths = [](const std::vector<ColumnDef>& cols) {
        double totalWeightage = 0;
        for (const auto& col : cols) {
            totalWeightage += col.weightage;
        }
        std::vector<double> widths;
        widths.reserve(cols.size());
        for (const auto& col : cols) {
            widths.push_back(totalWeightage > 0 ? (col.weightage / totalWeightage * 100.0) : 0);
        }
        return widths;
    };

    if (noData_ && sections_.empty()) {
        appendRunningHeader(std::string());
        html << R"(<div style="text-align:center; margin-top: 40mm;">
    <p style="font-size: 12pt;">No Data for:</p>
    <p style="font-size: 12pt; margin-top: 10mm;">)" << noDataText_ << R"(</p>
</div>
)";
        appendRunningFooter(std::string());
    }

    for (size_t si = firstSection; si < lastSection; ++si) {
        const auto& sec = sections_[si];
        // Use the section's own column layout (falls back to the builder's
        // current columns for sections created before any columns were defined).
        const std::vector<ColumnDef>& cols = sec.columns.empty() ? columns_ : sec.columns;
        std::vector<double> colWidths = computeColWidths(cols);
        // Each section keeps the break-page setting it was created with (column 0
        // hidden as the page-title column); a later result set without @break-page
        // must not unhide it.
        int startOfs = sec.breakPage ? 1 : 0;
        // Emit this section's running header/footer BEFORE the page break so
        // the new values are already active on the page that starts the section.
        std::string sectionTitle = sec.pageTitle;
        {
            std::string decoded;
            decoded.reserve(sectionTitle.size());
            for (size_t i = 0; i < sectionTitle.size(); ++i) {
                if (sectionTitle[i] == '%' && i + 2 < sectionTitle.size()
                    && std::isxdigit(static_cast<unsigned char>(sectionTitle[i+1]))
                    && std::isxdigit(static_cast<unsigned char>(sectionTitle[i+2]))) {
                    char hex[3] = {sectionTitle[i+1], sectionTitle[i+2], '\0'};
                    decoded += static_cast<char>(std::strtol(hex, nullptr, 16));
                    i += 2;
                } else {
                    decoded += sectionTitle[i];
                }
            }
            sectionTitle = std::move(decoded);
        }
        appendRunningHeader(sectionTitle);

        std::string pageTotalStr;
        if (sec.hasPageTotal) {
            for (size_t ci = 0; ci < cols.size() && ci < sec.pageTotalCells.size(); ++ci) {
                if (!sec.pageTotalCells[ci].empty()) {
                    if (!pageTotalStr.empty()) pageTotalStr += " ";
                    pageTotalStr += sec.pageTotalCells[ci];
                }
            }
        }
        appendRunningFooter(pageTotalStr);

        html << "<div" << (si > firstSection ? " class=\"section-break\"" : "") << ">\n";

        // Table
        html << "  <table>\n    <thead><tr>\n";
        for (size_t ci = startOfs; ci < cols.size(); ++ci) {
            const auto& col = cols[ci];
            html << "      <th style=\"width:" << fmt::format("{:.1f}", colWidths[ci])
                 << "%\"" << (col.isNumber ? " class=\"text-right\"" : "") << ">"
                 << col.name << "</th>\n";
        }
        html << "    </tr></thead>\n    <tbody>\n";

        // Data rows
        for (const auto& row : sec.rows) {
            html << "    <tr>\n";
            bool hasSubTable = !row.subTableHtml.empty();
            for (size_t ci = startOfs; ci < row.cells.size() && ci < cols.size(); ++ci) {
                // Skip detail columns covered by sub-table colspan
                if (hasSubTable && ci > (size_t)row.subTableStartCol && ci < (size_t)(row.subTableStartCol + row.subTableColspan))
                    continue;

                bool isSubTableCell = hasSubTable && (int)ci == row.subTableStartCol;
                std::string tdClass;
                if (isSubTableCell) {
                    tdClass = " class=\"sub-table-cell\"";
                } else if (cols[ci].isNumber) {
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
            for (size_t dj = startOfs; dj < cols.size(); ++dj) {
                if (dj < keyColumns_.size() && !keyColumns_[dj]) {
                    if (totDetailStart < 0) totDetailStart = (int)dj;
                    totDetailEnd = (int)dj;
                }
            }
            totDetailColspan = (totDetailStart >= 0) ? (totDetailEnd - totDetailStart + 1) : 0;
        }

        if (sec.hasPageTotal) {
            html << "    <tr class=\"footer-row\">\n";
            for (size_t ci = startOfs; ci < sec.pageTotalCells.size() && ci < cols.size(); ++ci) {
                if (totDetailColspan > 0 && (int)ci > totDetailStart && (int)ci <= totDetailEnd) {
                    continue;  // covered by colspan cell
                }
                bool isDetailStart = (totDetailColspan > 0 && (int)ci == totDetailStart);
                html << "      <td"
                     << (cols[ci].isNumber ? " class=\"text-right\"" : "")
                     << (isDetailStart ? fmt::format(" colspan=\"{}\"", totDetailColspan) : "")
                     << ">" << sec.pageTotalCells[ci] << "</td>\n";
            }
            html << "    </tr>\n";
        }

        html << "    </tbody>\n  </table>\n";

        // Grand total (only on last section)
        if (hasGrandTotal_ && si == sections_.size() - 1) {
            html << "  <table><tr class=\"grand-total-row\">\n";
            for (size_t ci = startOfs; ci < grandTotalCells_.size() && ci < cols.size(); ++ci) {
                if (totDetailColspan > 0 && (int)ci > totDetailStart && (int)ci <= totDetailEnd) {
                    continue;
                }
                bool isDetailStart = (totDetailColspan > 0 && (int)ci == totDetailStart);
                html << "    <td style=\"width:" << fmt::format("{:.1f}", colWidths[ci])
                     << "%\""
                     << (cols[ci].isNumber ? " class=\"text-right\"" : "")
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

