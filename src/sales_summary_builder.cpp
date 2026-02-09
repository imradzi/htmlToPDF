#include "sales_summary_builder.h"
#include <sstream>
#include <iomanip>
#include <cstdio>

std::string SalesSummaryPDFBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

std::string SalesSummaryPDFBuilder::formatNumber(double value, int decimals) {
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

TemplateContext SalesSummaryPDFBuilder::buildContext(const SummaryData& data) {
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
    vars["terminal_name"] = data.terminalName;
    
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
    vars["num_receipts"] = std::to_string(data.numReceipts);
    
    // Shift info
    vars["is_shift"] = data.shift.isShift ? "1" : "";
    vars["shift_id"] = data.shift.shiftId;
    vars["shift_terminal"] = data.shift.terminalName;
    vars["starting_cash"] = formatNumber(data.shift.startingCash);
    vars["closing_cash"] = formatNumber(data.shift.closingCash);
    
    // Flags
    vars["show_category"] = data.showCategory ? "1" : "";
    vars["show_by_date"] = data.showByDate ? "1" : "";
    vars["is_cash_sales"] = data.isCashSales ? "1" : "";
    vars["show_membership"] = data.showMembership ? "1" : "";
    vars["show_by_customer"] = data.showByCustomer ? "1" : "";
    
    // Category data
    for (const auto& cat : data.categories) {
        Item item;
        item.fields["name"] = cat.name;
        item.fields["amount"] = formatNumber(cat.amount);
        ctx.lists["categories"].push_back(item);
    }
    vars["total_sales"] = formatNumber(data.totalSales);
    
    // Date data
    for (const auto& d : data.dates) {
        Item item;
        item.fields["date"] = d.date;
        item.fields["gst"] = formatNumber(d.gst);
        item.fields["amount"] = formatNumber(d.amount);
        item.fields["total"] = formatNumber(d.total);
        ctx.lists["dates"].push_back(item);
    }
    vars["dates_total_gst"] = formatNumber(data.datesTotalGst);
    vars["dates_total_amount"] = formatNumber(data.datesTotalAmount);
    vars["dates_total"] = formatNumber(data.datesTotal);
    
    // Payment types
    for (const auto& p : data.paymentTypes) {
        Item item;
        item.fields["name"] = p.name;
        item.fields["amount"] = formatNumber(p.amount);
        ctx.lists["payment_types"].push_back(item);
    }
    vars["total_discount_rounding"] = formatNumber(data.totalDiscountRounding);
    vars["total_gst"] = formatNumber(data.totalGst);
    
    // Cash outs
    for (const auto& c : data.cashOuts) {
        Item item;
        item.fields["name"] = c.name;
        item.fields["amount"] = formatNumber(c.amount);
        ctx.lists["cash_outs"].push_back(item);
    }
    vars["total_cash_out"] = formatNumber(data.totalCashOut);
    
    // Summary
    vars["return_cancelled"] = formatNumber(data.returnCancelled);
    vars["cash_in_drawer"] = formatNumber(data.cashInDrawer);
    
    // Membership
    vars["points_given"] = formatNumber(data.pointsGiven);
    vars["points_reimbursed"] = formatNumber(data.pointsReimbursed);
    
    // Customer data
    for (const auto& cust : data.customers) {
        Item item;
        item.fields["name"] = cust.name;
        item.fields["sales"] = formatNumber(cust.sales);
        item.fields["cost"] = formatNumber(cust.cost);
        item.fields["margin"] = formatNumber(cust.margin);
        ctx.lists["customers"].push_back(item);
    }
    vars["customer_total_sales"] = formatNumber(data.customerTotalSales);
    vars["customer_total_cost"] = formatNumber(data.customerTotalCost);
    vars["customer_total_margin"] = formatNumber(data.customerTotalMargin);
    
    return ctx;
}
