#include "purchase_summary_builder.h"
#include <sstream>
#include <iomanip>
#include <cstdio>

std::string PurchaseSummaryPDFBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

std::string PurchaseSummaryPDFBuilder::formatNumber(double value, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    std::string numStr = oss.str();
    
    // Add thousand separators
    size_t dotPos = numStr.find('.');
    std::string intPart = (dotPos != std::string::npos) ? numStr.substr(0, dotPos) : numStr;
    std::string decPart = (dotPos != std::string::npos) ? numStr.substr(dotPos) : "";
    
    // Handle negative numbers
    bool isNegative = (!intPart.empty() && intPart[0] == '-');
    if (isNegative) intPart = intPart.substr(1);
    
    // Insert commas from right to left
    std::string result;
    int count = 0;
    for (int i = static_cast<int>(intPart.length()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) result = ',' + result;
        result = intPart[i] + result;
        ++count;
    }
    
    if (isNegative) result = '-' + result;
    return result + decPart;
}

TemplateContext PurchaseSummaryPDFBuilder::buildContext(const SummaryData& data) {
    TemplateContext ctx;
    auto& vars = ctx.variables;
    
    // Theme colors
    vars["theme_color"] = colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue);
    vars["box_color"] = colorToHex(data.theme.boxColorRed, data.theme.boxColorGreen, data.theme.boxColorBlue);
    vars["fill_color"] = data.theme.fillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    vars["letterhead_fill_color"] = data.theme.letterheadFillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    
    // Header info
    vars["title"] = data.title;
    vars["date_computed"] = data.dateComputed;
    
    // Outlet info
    vars["outlet_code"] = data.outlet.code;
    vars["outlet_name"] = data.outlet.name;
    vars["outlet_name2"] = data.outlet.name2;
    vars["outlet_address_1"] = data.outlet.address1;
    vars["outlet_address_2"] = data.outlet.address2;
    vars["outlet_address_3"] = data.outlet.address3;
    vars["outlet_address_4"] = data.outlet.address4;
    
    // Date range
    vars["from_date"] = data.fromDate;
    vars["to_date"] = data.toDate;
    
    // Category data
    for (const auto& cat : data.categories) {
        Item item;
        item.fields["name"] = cat.name;
        item.fields["gst"] = formatNumber(cat.gst);
        item.fields["amount"] = formatNumber(cat.amount);
        ctx.lists["categories"].push_back(item);
    }
    vars["total_category_gst"] = formatNumber(data.totalCategoryGst);
    vars["total_category_amount"] = formatNumber(data.totalCategoryAmount);
    
    // Payment types
    for (const auto& p : data.paymentTypes) {
        Item item;
        item.fields["name"] = p.name;
        item.fields["amount"] = formatNumber(p.amount);
        ctx.lists["payment_types"].push_back(item);
    }
    vars["total_payment"] = formatNumber(data.totalPayment);
    
    // Return/cancelled
    vars["return_cancelled"] = formatNumber(data.returnCancelled);
    
    // Supplier data
    for (const auto& sup : data.suppliers) {
        Item item;
        item.fields["name"] = sup.name;
        item.fields["gst"] = formatNumber(sup.gst);
        item.fields["amount"] = formatNumber(sup.amount);
        ctx.lists["suppliers"].push_back(item);
    }
    vars["total_supplier_gst"] = formatNumber(data.totalSupplierGst);
    vars["total_supplier_amount"] = formatNumber(data.totalSupplierAmount);
    
    return ctx;
}
