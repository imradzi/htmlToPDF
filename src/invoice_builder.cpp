#include "invoice_builder.h"
#include <sstream>
#include <iomanip>
#include <cstdio>

// InvoicePDFBuilder implementations
std::string InvoicePDFBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

std::string InvoicePDFBuilder::formatNumber(double value, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    return oss.str();
}

std::string InvoicePDFBuilder::formatQuantity(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string result = oss.str();
    // Remove trailing zeros
    size_t dotPos = result.find('.');
    if (dotPos != std::string::npos) {
        size_t lastNonZero = result.find_last_not_of('0');
        if (lastNonZero == dotPos) {
            result = result.substr(0, dotPos);
        } else if (lastNonZero != std::string::npos) {
            result = result.substr(0, lastNonZero + 1);
        }
    }
    return result;
}

int InvoicePDFBuilder::getItemsPerPage(bool isLandscape, const PaginationConfig& config) {
    return isLandscape ? config.itemsPerPageLandscape : config.itemsPerPagePortrait;
}

int InvoicePDFBuilder::calculateTotalPages(int itemCount, bool isLandscape, const PaginationConfig& config) {
    int itemsPerPage = getItemsPerPage(isLandscape, config);
    if (itemCount <= 0) return 1;
    return (itemCount + itemsPerPage - 1) / itemsPerPage;  // Ceiling division
}

std::vector<InvoicePDFBuilder::InvoiceData> InvoicePDFBuilder::paginateInvoice(
    const InvoiceData& data, const PaginationConfig& config) {
    
    std::vector<InvoiceData> pages;
    int itemsPerPage = getItemsPerPage(data.isLandscape, config);
    int totalItems = static_cast<int>(data.items.size());
    int totalPages = calculateTotalPages(totalItems, data.isLandscape, config);
    
    if (totalPages <= 1) {
        // Single page - just set the page numbers
        InvoiceData singlePage = data;
        singlePage.pageNo = 1;
        singlePage.totalPages = 1;
        pages.push_back(singlePage);
        return pages;
    }
    
    // Split items across pages
    for (int pageIdx = 0; pageIdx < totalPages; ++pageIdx) {
        InvoiceData pageData = data;  // Copy all common data
        pageData.pageNo = pageIdx + 1;
        pageData.totalPages = totalPages;
        pageData.items.clear();
        
        // Calculate item range for this page
        int startIdx = pageIdx * itemsPerPage;
        int endIdx = std::min(startIdx + itemsPerPage, totalItems);
        
        // Copy items for this page
        for (int i = startIdx; i < endIdx; ++i) {
            pageData.items.push_back(data.items[i]);
        }
        
        // Only show totals on the last page
        if (pageIdx < totalPages - 1) {
            // Clear totals for non-last pages (or you could set a flag)
            // Actually, keep totals but template should only show on last page
        }
        
        pages.push_back(pageData);
    }
    
    return pages;
}

TemplateContext InvoicePDFBuilder::buildContext(const InvoiceData& data) {
    TemplateContext ctx;
    auto& vars = ctx.variables;
    
    // Theme colors
    vars["theme_color"] = colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue);
    vars["box_color"] = colorToHex(data.theme.boxColorRed, data.theme.boxColorGreen, data.theme.boxColorBlue);
    vars["fill_color"] = data.theme.fillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    vars["letterhead_fill_color"] = data.theme.letterheadFillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    
    // Document info
    vars["document_type"] = data.documentType;
    vars["ref_title"] = data.refTitle;
    vars["is_draft"] = data.isDraft ? "1" : "";
    vars["is_landscape"] = data.isLandscape ? "1" : "";
    
    // Header info
    vars["id"] = data.id;
    vars["ref_no"] = data.refNo;
    vars["transaction_date"] = data.transactionDate;
    vars["term"] = data.term;
    vars["page_no"] = std::to_string(data.pageNo);
    vars["total_pages"] = std::to_string(data.totalPages);
    vars["is_last_page"] = (data.pageNo == data.totalPages) ? "1" : "";
    
    // Outlet info
    vars["outlet_name"] = data.outlet.name;
    vars["outlet_name2"] = data.outlet.name2;
    vars["outlet_address"] = data.outlet.address;
    vars["outlet_reg_no"] = data.outlet.regNo;
    vars["outlet_gst_reg_no"] = data.outlet.gstRegNo;
    vars["outlet_logo"] = data.outlet.logoPath;
    
    // Party info
    vars["invoice_to_name"] = data.invoiceTo.name;
    vars["invoice_to_address"] = data.invoiceTo.address;
    vars["invoice_to_id"] = data.invoiceTo.id;
    vars["deliver_to_name"] = data.deliverTo.name;
    vars["deliver_to_address"] = data.deliverTo.address;
    vars["deliver_to_id"] = data.deliverTo.id;
    vars["show_deliver_to"] = data.showDeliverTo ? "1" : "";
    vars["show_account_id"] = data.showAccountId ? "1" : "";
    
    // Display flags
    vars["show_code"] = data.showCode ? "1" : "";
    vars["show_mal"] = data.showMal ? "1" : "";
    vars["show_batch_expiry"] = data.showBatchExpiry ? "1" : "";
    vars["show_bonus"] = data.showBonus ? "1" : "";
    vars["show_srp"] = data.showSrp ? "1" : "";
    vars["show_discount"] = data.showDiscount ? "1" : "";
    vars["show_gst"] = data.showGst ? "1" : "";
    vars["show_minimal"] = data.showMinimal ? "1" : "";
    
    // Purchase order flags
    vars["is_purchase_order"] = data.isPurchaseOrder ? "1" : "";
    vars["is_goods_received"] = data.isGoodsReceived ? "1" : "";
    vars["is_goods_return"] = data.isGoodsReturn ? "1" : "";
    vars["is_invoice"] = (!data.isPurchaseOrder && !data.isGoodsReceived && !data.isGoodsReturn) ? "1" : "";
    
    // Party label - derive from document type
    if (data.isPurchaseOrder) {
        vars["party_label"] = "Order From:";
    } else if (data.isGoodsReceived) {
        vars["party_label"] = "Invoice From:";
    } else {
        vars["party_label"] = "Invoice To:";
    }
    
    // Items label - use custom if provided, otherwise derive from document type
    if (!data.itemsLabel.empty()) {
        vars["items_label"] = data.itemsLabel;
    } else if (data.isPurchaseOrder) {
        vars["items_label"] = "We would like to order:";
    } else if (data.isGoodsReceived) {
        vars["items_label"] = "Items purchased:";
    } else {
        vars["items_label"] = "Items sold:";
    }
    
    // Line items - copy display flags into each item for template loop conditionals
    for (const auto& item : data.items) {
        Item it;
        it.fields["line_no"] = std::to_string(item.lineNo);
        it.fields["code"] = item.code;
        it.fields["mal"] = item.mal;
        it.fields["name"] = item.name;
        it.fields["packing"] = item.packing;
        it.fields["batch_no"] = item.batchNo;
        it.fields["expiry_date"] = item.expiryDate;
        it.fields["quantity"] = formatQuantity(item.quantity);
        it.fields["bonus"] = formatQuantity(item.bonus);
        it.fields["price"] = formatNumber(item.price);
        it.fields["net_price"] = formatNumber(item.netPrice);
        it.fields["selling_price"] = formatNumber(item.sellingPrice);
        it.fields["margin"] = formatNumber(item.margin);
        it.fields["discount"] = formatNumber(item.discount);
        it.fields["gst"] = formatNumber(item.gst);
        it.fields["amount"] = formatNumber(item.amount);
        // Copy display flags so conditionals work inside the loop
        it.fields["show_code"] = data.showCode ? "1" : "";
        it.fields["show_mal"] = data.showMal ? "1" : "";
        it.fields["show_batch_expiry"] = data.showBatchExpiry ? "1" : "";
        it.fields["show_bonus"] = data.showBonus ? "1" : "";
        it.fields["show_srp"] = data.showSrp ? "1" : "";
        it.fields["show_discount"] = data.showDiscount ? "1" : "";
        it.fields["show_gst"] = data.showGst ? "1" : "";
        it.fields["show_minimal"] = data.showMinimal ? "1" : "";
        ctx.lists["items"].push_back(it);
    }
    
    // Totals
    vars["total_amount"] = formatNumber(data.totalAmount);
    vars["total_gst"] = formatNumber(data.totalGst);
    vars["total_discount"] = formatNumber(data.totalDiscount);
    
    // Notes
    for (const auto& note : data.notes) {
        Item it;
        it.fields["text"] = note;
        ctx.lists["notes"].push_back(it);
    }
    for (const auto& remark : data.remarks) {
        Item it;
        it.fields["text"] = remark;
        ctx.lists["remarks"].push_back(it);
    }
    vars["has_remarks"] = !data.remarks.empty() ? "1" : "";
    
    // e-Invoice
    vars["e_invoice_qr"] = data.eInvoiceQR;
    vars["has_e_invoice"] = !data.eInvoiceQR.empty() ? "1" : "";
    
    return ctx;
}

// BillingStatementPDFBuilder implementations
std::string BillingStatementPDFBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    return InvoicePDFBuilder::colorToHex(r, g, b);
}

std::string BillingStatementPDFBuilder::formatNumber(double value, int decimals) {
    return InvoicePDFBuilder::formatNumber(value, decimals);
}

TemplateContext BillingStatementPDFBuilder::buildContext(const BillingData& data, size_t debtorIndex) {
    TemplateContext ctx;
    auto& vars = ctx.variables;
    
    if (debtorIndex >= data.debtors.size()) {
        return ctx;
    }
    
    const auto& debtor = data.debtors[debtorIndex];
    
    // Theme colors
    vars["theme_color"] = colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue);
    vars["box_color"] = colorToHex(data.theme.boxColorRed, data.theme.boxColorGreen, data.theme.boxColorBlue);
    vars["fill_color"] = data.theme.fillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    vars["letterhead_fill_color"] = data.theme.letterheadFillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    
    // Title and dates
    vars["title"] = data.title;
    vars["from_date"] = data.fromDate;
    vars["to_date"] = data.toDate;
    vars["period"] = data.fromDate + " - " + data.toDate;
    
    // Outlet info
    vars["outlet_name"] = data.outlet.name;
    vars["outlet_name2"] = data.outlet.name2;
    vars["outlet_address"] = data.outlet.address;
    vars["outlet_reg_no"] = data.outlet.regNo;
    
    // Debtor info
    vars["debtor_name"] = debtor.name;
    vars["debtor_address"] = debtor.address;
    vars["debtor_id"] = debtor.debtorId;
    vars["total_amount"] = formatNumber(debtor.totalAmount);
    vars["term"] = formatNumber(debtor.term);
    
    // Customer records with nested items
    for (const auto& customer : debtor.customers) {
        Item custItem;
        custItem.fields["name"] = customer.name;
        custItem.fields["ic"] = customer.ic;
        custItem.fields["total"] = formatNumber(customer.total);
        
        // Build items as a single string for now (simplified)
        // In a more complex template, we'd need nested loops support
        ctx.lists["customers"].push_back(custItem);
        
        // Add items to a flat list with customer reference
        for (const auto& item : customer.items) {
            Item itemData;
            itemData.fields["customer_name"] = customer.name;
            itemData.fields["item"] = item.item;
            itemData.fields["sales_ids"] = item.salesIds;
            itemData.fields["quantity"] = formatNumber(item.quantity);
            itemData.fields["amount"] = formatNumber(item.amount);
            ctx.lists["all_items"].push_back(itemData);
        }
    }
    
    return ctx;
}

// PoisonOrderPDFBuilder implementations
std::string PoisonOrderPDFBuilder::colorToHex(unsigned char r, unsigned char g, unsigned char b) {
    return InvoicePDFBuilder::colorToHex(r, g, b);
}

std::string PoisonOrderPDFBuilder::formatNumber(double value, int decimals) {
    return InvoicePDFBuilder::formatNumber(value, decimals);
}

std::string PoisonOrderPDFBuilder::formatQuantity(double value) {
    return InvoicePDFBuilder::formatQuantity(value);
}

TemplateContext PoisonOrderPDFBuilder::buildContext(const PoisonOrderData& data) {
    TemplateContext ctx;
    auto& vars = ctx.variables;
    
    // Theme colors
    vars["theme_color"] = colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue);
    vars["box_color"] = colorToHex(data.theme.boxColorRed, data.theme.boxColorGreen, data.theme.boxColorBlue);
    vars["fill_color"] = data.theme.fillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    vars["letterhead_fill_color"] = data.theme.letterheadFillRect ? colorToHex(data.theme.fillColorRed, data.theme.fillColorGreen, data.theme.fillColorBlue) : "#ffffff";
    
    // Header info
    vars["title"] = data.title;
    vars["id"] = data.id;
    vars["ref_no"] = data.refNo;
    vars["transaction_date"] = data.transactionDate;
    vars["term"] = data.term;
    vars["page_no"] = std::to_string(data.pageNo);
    vars["total_pages"] = std::to_string(data.totalPages);
    
    // Outlet info
    vars["outlet_name"] = data.outlet.name;
    vars["outlet_name2"] = data.outlet.name2;
    vars["outlet_address"] = data.outlet.address;
    vars["outlet_reg_no"] = data.outlet.regNo;
    
    // Delivery info
    vars["deliver_to_name"] = data.deliverTo.name;
    vars["deliver_to_address"] = data.deliverTo.address;
    vars["account_id"] = data.accountId;
    vars["show_account_id"] = data.showAccountId ? "1" : "";
    vars["purpose_of_sale"] = data.purposeOfSale;
    
    // Items
    for (const auto& item : data.items) {
        Item it;
        it.fields["line_no"] = std::to_string(item.lineNo);
        it.fields["code"] = item.code;
        it.fields["mal"] = item.mal;
        it.fields["name"] = item.name;
        it.fields["batch_no"] = item.batchNo;
        it.fields["expiry_date"] = item.expiryDate;
        it.fields["quantity"] = formatQuantity(item.quantity);
        it.fields["uom"] = item.uom;
        ctx.lists["items"].push_back(it);
    }
    
    // Footer notes
    for (const auto& note : data.receiverNotes) {
        Item it;
        it.fields["text"] = note;
        ctx.lists["receiver_notes"].push_back(it);
    }
    for (const auto& note : data.supplierNotes) {
        Item it;
        it.fields["text"] = note;
        ctx.lists["supplier_notes"].push_back(it);
    }
    
    return ctx;
}
