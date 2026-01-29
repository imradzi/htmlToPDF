#pragma once

#include <string>
#include <vector>
#include "template_engine.h"

// Helper class for building Invoice/Order PDFs using htmlToPDF
class InvoicePDFBuilder {
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
        std::string address;
        std::string regNo;
        std::string gstRegNo;
        std::string logoPath;
    };

    struct PartyInfo {
        std::string name;
        std::string address;
        std::string id;
    };

    struct LineItem {
        int lineNo = 0;
        std::string code;
        std::string mal;
        std::string name;
        std::string packing;
        std::string batchNo;
        std::string expiryDate;
        double quantity = 0;
        double bonus = 0;
        double price = 0;
        double netPrice = 0;
        double sellingPrice = 0;
        double margin = 0;
        double discount = 0;
        double gst = 0;
        double amount = 0;
    };

    struct InvoiceData {
        // Document type/format
        std::string documentType;  // INVOICE, CREDIT NOTE, DELIVERY ORDER, PURCHASE ORDER, etc.
        std::string refTitle;      // INV:, CRN:, DO:, PO:, etc.
        bool isDraft = false;
        bool isLandscape = false;
        
        // Header info
        std::string id;
        std::string refNo;
        std::string transactionDate;
        std::string term;
        int pageNo = 1;
        int totalPages = 1;
        
        // Outlet (seller) info
        OutletInfo outlet;
        
        // Customer/Supplier info
        PartyInfo invoiceTo;
        PartyInfo deliverTo;
        bool showDeliverTo = true;
        bool showAccountId = false;
        
        // Display flags
        bool showCode = true;
        bool showMal = true;
        bool showBatchExpiry = false;
        bool showBonus = false;
        bool showSrp = false;
        bool showDiscount = false;
        bool showGst = true;
        bool showMinimal = false;  // For purchase orders - just qty and bonus
        
        // For purchase orders
        bool isPurchaseOrder = false;
        bool isGoodsReceived = false;
        bool isGoodsReturn = false;
        
        // Custom items label (e.g., "Items sold:", "We would like to order:", etc.)
        std::string itemsLabel;
        
        // Line items
        std::vector<LineItem> items;
        
        // Totals
        double totalAmount = 0;
        double totalGst = 0;
        double totalDiscount = 0;
        
        // Footer notes
        std::vector<std::string> notes;
        std::vector<std::string> remarks;
        
        // e-Invoice QR
        std::string eInvoiceQR;
        
        // Theme
        ThemeColors theme;
    };

    // Build template context from invoice data
    static TemplateContext buildContext(const InvoiceData& data);
    
    // Helper to format color as CSS hex
    static std::string colorToHex(unsigned char r, unsigned char g, unsigned char b);
    
    // Helper to format number
    static std::string formatNumber(double value, int decimals = 2);
    
    // Helper to format quantity (removes trailing zeros)
    static std::string formatQuantity(double value);
};

// Helper class for building Billing Statement PDFs
class BillingStatementPDFBuilder {
public:
    struct CustomerItem {
        std::string item;
        std::string salesIds;
        double quantity = 0;
        double amount = 0;
    };

    struct CustomerRecord {
        std::string name;
        std::string ic;
        std::string customerId;
        double total = 0;
        std::vector<CustomerItem> items;
    };

    struct DebtorRecord {
        std::string name;
        std::string address;
        std::string debtorId;
        double totalAmount = 0;
        double term = 0;
        std::vector<CustomerRecord> customers;
    };

    struct BillingData {
        std::string title;
        std::string fromDate;
        std::string toDate;
        
        // Outlet info
        InvoicePDFBuilder::OutletInfo outlet;
        
        // Debtors with their customers and items
        std::vector<DebtorRecord> debtors;
        
        // Theme
        InvoicePDFBuilder::ThemeColors theme;
    };

    static TemplateContext buildContext(const BillingData& data, size_t debtorIndex);
    static std::string colorToHex(unsigned char r, unsigned char g, unsigned char b);
    static std::string formatNumber(double value, int decimals = 2);
};

// Helper class for building Poison Order PDFs
class PoisonOrderPDFBuilder {
public:
    struct PoisonItem {
        int lineNo = 0;
        std::string code;
        std::string mal;
        std::string name;
        std::string batchNo;
        std::string expiryDate;
        double quantity = 0;
        std::string uom;
    };

    struct PoisonOrderData {
        std::string title;
        std::string id;
        std::string refNo;
        std::string transactionDate;
        std::string term;
        int pageNo = 1;
        int totalPages = 1;
        
        // Outlet info
        InvoicePDFBuilder::OutletInfo outlet;
        
        // Customer info
        InvoicePDFBuilder::PartyInfo deliverTo;
        std::string accountId;
        bool showAccountId = false;
        std::string purposeOfSale;
        
        // Items
        std::vector<PoisonItem> items;
        
        // Footer
        std::vector<std::string> receiverNotes;
        std::vector<std::string> supplierNotes;
        
        // Theme
        InvoicePDFBuilder::ThemeColors theme;
    };

    static TemplateContext buildContext(const PoisonOrderData& data);
    static std::string colorToHex(unsigned char r, unsigned char g, unsigned char b);
    static std::string formatNumber(double value, int decimals = 2);
    static std::string formatQuantity(double value);
};
