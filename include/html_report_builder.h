#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "template_engine.h"

// Replacement for ReportPDF that generates HTML for wkhtmltox-based PDF output.
// Collects data into structured data and renders HTML, then converts to PDF via wkhtmltox.
class HtmlReportBuilder {
public:
    // Font size settings (matching ReportPDF's fontSize struct)
    struct FontSizes {
        int label = 8;
        int title = 20;
        int data = 8;
        int total = 7;
        int note = 7;
        int footer = 6;
    };

    // Column definition for tabular reports
    struct ColumnDef {
        std::string name;
        double weightage = 1.0;   // proportional width
        bool isNumber = false;    // right-align numbers
        std::string sumFunction;  // "sum", "average", etc.
    };

    // A single row of cell values
    struct RowData {
        std::vector<std::string> cells;
    };

    // A single page/section of the report
    struct Section {
        std::string title;
        std::string subtitle;
        std::string pageTitle;
        int pageNo = 1;
        std::vector<RowData> rows;
        std::vector<std::string> pageTotalCells;
        bool hasPageTotal = false;
    };

    // Theme colors
    struct ThemeColors {
        unsigned char fillColorRed = 0xCC;
        unsigned char fillColorGreen = 0xFF;
        unsigned char fillColorBlue = 0xFF;
        unsigned char boxColorRed = 0x80;
        unsigned char boxColorGreen = 0x80;
        unsigned char boxColorBlue = 0x80;
    };

private:
    std::string title_;
    std::string subtitle_;
    std::string outletName_;
    std::string orientation_;  // "Portrait" or "Landscape"
    FontSizes fontSize_;
    ThemeColors theme_;
    bool showFooterPageNo_ = true;

    // Column definitions
    std::vector<ColumnDef> columns_;

    // Sections (each becomes a page)
    std::vector<Section> sections_;
    Section* currentSection_ = nullptr;

    // Grand total cells
    std::vector<std::string> grandTotalCells_;
    bool hasGrandTotal_ = false;

    // No-data message
    std::string noDataText_;
    bool noData_ = false;

    // Custom CSS
    std::string customCss_;

    // Line height
    int lineHeight_ = 5;

    // Page break on column 0 value change
    bool breakPageOn_ = false;

    // Track page count
    int pageCount_ = 0;

    // Owning pointer to XLSColumnFormatter (used by AppendToPDF)
    void* formatterPtr_ = nullptr;
    std::function<void(void*)> formatterDeleter_;

public:
    HtmlReportBuilder(const std::string& title, const std::string& outletName, const std::string& orientation = "Portrait");
    ~HtmlReportBuilder() {
        if (formatterPtr_ && formatterDeleter_) formatterDeleter_(formatterPtr_);
    }

    // --- Configuration ---
    void setSubtitle(const std::string& subtitle) { subtitle_ = subtitle; }
    void setOrientation(const std::string& orientation) { orientation_ = orientation; }
    void setFontSizes(const FontSizes& sizes) { fontSize_ = sizes; }
    void setThemeColors(const ThemeColors& colors) { theme_ = colors; }
    void setShowFooterPageNo(bool show) { showFooterPageNo_ = show; }
    void setLineHeight(int h) { lineHeight_ = h; }
    void setBreakPageOn(bool b) { breakPageOn_ = b; }
    void setCustomCss(const std::string& css) { customCss_ = css; }

    // --- Column management ---
    void clearColumns() { columns_.clear(); }
    void addColumn(const std::string& name, double weightage = 1.0, bool isNumber = false, const std::string& sumFunction = "");
    void setColumns(std::vector<ColumnDef> cols) { columns_ = std::move(cols); }
    int columnCount() const { return static_cast<int>(columns_.size()); }
    std::vector<ColumnDef>& columns() { return columns_; }
    const std::vector<ColumnDef>& columns() const { return columns_; }

    // --- Section/page management ---
    Section& newSection(const std::string& pageTitle = "");
    Section* currentSection() { return currentSection_; }
    int sectionCount() const { return static_cast<int>(sections_.size()); }
    bool hasSections() const { return !sections_.empty(); }

    // --- Data insertion ---
    void addRow(const std::vector<std::string>& cellValues);
    void addRow(const RowData& row);

    // --- Totals ---
    void setPageTotal(const std::vector<std::string>& totalCells);
    void setGrandTotal(const std::vector<std::string>& totalCells);

    // --- No data ---
    void setNoData(const std::string& text);

    // --- Output ---
    // Build TemplateContext for rendering
    TemplateContext buildContext() const;

    // Render HTML string from template
    std::string renderHtml() const;

    // Generate PDF file, returns the output path
    bool generatePdf(const std::string& outputPath) const;

    // SaveAsFile equivalent — generate PDF to the given path
    bool saveAsFile(const std::wstring& filePath) const;

    // --- Accessors for compatibility ---
    const std::string& title() const { return title_; }
    const std::string& outletName() const { return outletName_; }
    const std::string& orientation() const { return orientation_; }
    bool isLandscape() const;
    int pageCount() const { return pageCount_; }
    bool breakPageOn() const { return breakPageOn_; }

    // --- Utility ---
    static std::string colorToHex(unsigned char r, unsigned char g, unsigned char b);
    static std::string formatNumber(double value, int decimals = 2);

    // --- Formatter storage (for AppendToPDF pipeline) ---
    // The formatter is stored as void* to avoid including PDFWriter.h here.
    // AppendToPDF creates and stores it; it is deleted in the destructor.
    template<typename T>
    void setFormatterPtr(T* ptr) {
        formatterPtr_ = ptr;
        formatterDeleter_ = [](void* p) { delete static_cast<T*>(p); };
    }
    template<typename T>
    T* formatterPtr() const { return static_cast<T*>(formatterPtr_); }
};
