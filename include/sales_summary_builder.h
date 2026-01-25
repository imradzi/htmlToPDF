#pragma once

#include <string>
#include <map>
#include <vector>
#include "template_engine.h"

// Helper class for building Sales Summary PDF using htmlToPDF
class SalesSummaryPDFBuilder {
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

    struct ShiftInfo {
        bool isShift = false;
        std::string shiftId;
        std::string terminalName;
        double startingCash = 0;
        double closingCash = 0;
    };

    struct CategoryRow {
        std::string name;
        double amount = 0;
    };

    struct DateRow {
        std::string date;
        double gst = 0;
        double amount = 0;
        double total = 0;
    };

    struct PaymentRow {
        std::string name;
        double amount = 0;
    };

    struct CashOutRow {
        std::string name;
        double amount = 0;
    };

    struct CustomerRow {
        std::string name;
        double sales = 0;
        double cost = 0;
        double margin = 0;
    };

    struct SummaryData {
        std::string title;
        std::string dateComputed;
        std::string terminalName;
        OutletInfo outlet;
        std::string fromDate;
        std::string toDate;
        long numReceipts = 0;
        ShiftInfo shift;
        
        // Flags
        bool showCategory = true;
        bool showByDate = false;
        bool isCashSales = true;
        bool showMembership = false;
        bool showByCustomer = false;
        
        // Category section
        std::vector<CategoryRow> categories;
        double totalSales = 0;
        
        // Date section
        std::vector<DateRow> dates;
        double datesTotalGst = 0;
        double datesTotalAmount = 0;
        double datesTotal = 0;
        
        // Payment section
        std::vector<PaymentRow> paymentTypes;
        double totalDiscountRounding = 0;
        double totalGst = 0;
        
        // Cash out section
        std::vector<CashOutRow> cashOuts;
        double totalCashOut = 0;
        
        // Summary
        double returnCancelled = 0;
        double cashInDrawer = 0;
        
        // Membership
        double pointsGiven = 0;
        double pointsReimbursed = 0;
        
        // Customer section (for credit sales)
        std::vector<CustomerRow> customers;
        double customerTotalSales = 0;
        double customerTotalCost = 0;
        double customerTotalMargin = 0;
        
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
