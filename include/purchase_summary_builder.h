#pragma once

#include <string>
#include <map>
#include <vector>
#include "template_engine.h"

// Helper class for building Purchase Summary PDF using htmlToPDF
class PurchaseSummaryPDFBuilder {
public:
    struct ThemeColors {
        unsigned char fillColorRed = 0xCC;
        unsigned char fillColorGreen = 0xFF;
        unsigned char fillColorBlue = 0xFF;
        unsigned char boxColorRed = 0x80;
        unsigned char boxColorGreen = 0x80;
        unsigned char boxColorBlue = 0x80;
        bool fillRect = true;
        bool letterheadFillRect = true;
    };

    struct OutletInfo {
        std::string code;
        std::string name;
        std::string name2;
        std::string address1;
        std::string address2;
        std::string address3;
        std::string address4;
    };

    struct CategoryRow {
        std::string name;
        double gst = 0;
        double amount = 0;
    };

    struct PaymentRow {
        std::string name;
        double amount = 0;
    };

    struct SupplierRow {
        std::string name;
        double gst = 0;
        double amount = 0;
    };

    struct SummaryData {
        std::string title;
        std::string dateComputed;
        OutletInfo outlet;
        std::string fromDate;
        std::string toDate;
        
        // Category section
        std::vector<CategoryRow> categories;
        double totalCategoryGst = 0;
        double totalCategoryAmount = 0;
        
        // Payment section
        std::vector<PaymentRow> paymentTypes;
        double totalPayment = 0;
        
        // Return/cancelled
        double returnCancelled = 0;
        
        // Supplier section
        std::vector<SupplierRow> suppliers;
        double totalSupplierGst = 0;
        double totalSupplierAmount = 0;
        
        // Theme
        ThemeColors theme;
    };

    // Build template context from summary data
    static TemplateContext buildContext(const SummaryData& data);
    
    // Helper to format color as CSS hex
    static std::string colorToHex(unsigned char r, unsigned char g, unsigned char b);
    
    // Helper to format number
    static std::string formatNumber(double value, int decimals = 2);
};
