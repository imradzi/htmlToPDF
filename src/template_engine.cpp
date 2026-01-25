#include "template_engine.h"
#include <regex>
#include <fstream>
#include <sstream>

std::string TemplateEngine::render(const std::string& templateStr, const TemplateContext& context) {
    std::string result = templateStr;
    
    // First process {{#if}} blocks (conditionals)
    result = processIfBlocks(result, context.variables);
    
    // Then process {{#each}} blocks
    result = processEachBlocks(result, context.lists);
    
    // Then replace all {{key}} patterns with values from context
    for (const auto& [key, value] : context.variables) {
        result = replaceVariable(result, key, value);
    }
    
    // Remove any remaining unmatched variables
    std::regex pattern(R"(\{\{[^#/][^}]*\}\})");
    result = std::regex_replace(result, pattern, "");
    
    return result;
}

std::string TemplateEngine::processIfBlocks(const std::string& input,
                                             const std::map<std::string, std::string>& variables) {
    std::string result = input;
    
    // Pattern: {{#if varName}}...{{/if}}
    std::regex ifPattern(R"(\{\{#if\s+(\w+)\}\}([\s\S]*?)\{\{/if\}\})");
    std::smatch match;
    
    while (std::regex_search(result, match, ifPattern)) {
        std::string varName = match[1].str();
        std::string blockContent = match[2].str();
        std::string replacement;
        
        auto it = variables.find(varName);
        // Condition is true if variable exists and is not empty, "0", or "false"
        bool conditionTrue = (it != variables.end() && 
                              !it->second.empty() && 
                              it->second != "0" && 
                              it->second != "false");
        
        if (conditionTrue) {
            replacement = blockContent;
        }
        
        result = result.substr(0, match.position()) + replacement + 
                 result.substr(match.position() + match.length());
    }
    
    return result;
}

std::string TemplateEngine::loadTemplate(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string TemplateEngine::replaceVariable(const std::string& input,
                                             const std::string& key,
                                             const std::string& value) {
    std::string result = input;
    std::string placeholder = "{{" + key + "}}";
    
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
    
    return result;
}

std::string TemplateEngine::processEachBlocks(const std::string& input,
                                               const std::map<std::string, std::vector<Item>>& lists) {
    std::string result = input;
    
    // Pattern: {{#each listName}}...{{/each}}
    std::regex eachPattern(R"(\{\{#each\s+(\w+)\}\}([\s\S]*?)\{\{/each\}\})");
    std::smatch match;
    
    while (std::regex_search(result, match, eachPattern)) {
        std::string listName = match[1].str();
        std::string blockContent = match[2].str();
        std::string replacement;
        
        auto it = lists.find(listName);
        if (it != lists.end()) {
            for (const auto& item : it->second) {
                std::string itemContent = blockContent;
                for (const auto& [key, value] : item.fields) {
                    itemContent = replaceVariable(itemContent, key, value);
                }
                replacement += itemContent;
            }
        }
        
        result = result.substr(0, match.position()) + replacement + 
                 result.substr(match.position() + match.length());
    }
    
    return result;
}

std::string TemplateEngine::getInvoiceTemplate() {
    return R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: Arial, sans-serif; font-size: 11pt; color: #333; padding: 20px; }
    .letterhead { width: 100%; max-height: 100px; margin-bottom: 20px; }
    .header { margin-bottom: 30px; }
    .invoice-title { font-size: 24pt; font-weight: bold; color: #2c3e50; margin-bottom: 10px; }
    .invoice-info { margin-bottom: 20px; }
    .invoice-info p { margin: 3px 0; }
    .customer-info { margin-bottom: 30px; }
    .customer-info h3 { font-size: 12pt; color: #666; margin-bottom: 5px; }
    table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
    th { background: #2c3e50; color: white; font-size: 11pt; text-align: left; padding: 10px; }
    td { padding: 10px; border-bottom: 1px solid #ddd; }
    .amount { text-align: right; }
    .subtotal-row td { border-top: 2px solid #2c3e50; font-weight: bold; }
    .total-row { background: #f8f9fa; }
    .total-row td { font-size: 14pt; font-weight: bold; border-top: 2px solid #2c3e50; }
    .footer { margin-top: 40px; padding-top: 20px; border-top: 1px solid #ddd; font-size: 10pt; color: #666; }
</style>
</head>
<body>
    <img src="{{letterhead_image}}" class="letterhead" onerror="this.style.display='none'">   
    <div class="header">
        <div class="invoice-title">INVOICE</div>
        <div class="invoice-info">
            <p><strong>Invoice #:</strong> {{invoice_number}}</p>
            <p><strong>Date:</strong> {{date}}</p>
            <p><strong>Due Date:</strong> {{due_date}}</p>
        </div>
    </div>
    
    <div class="customer-info">
        <h3>Bill To:</h3>
        <p><strong>{{customer_name}}</strong></p>
        <p>{{customer_address}}</p>
    </div>
    
    <table>
        <thead>
            <tr>
                <th>Description</th>
                <th>Qty</th>
                <th class="amount">Unit Price</th>
                <th class="amount">Amount</th>
            </tr>
        </thead>
        <tbody>
            {{#each items}}
            <tr>
                <td>{{description}}</td>
                <td>{{qty}}</td>
                <td class="amount">{{unit_price}}</td>
                <td class="amount">{{amount}}</td>
            </tr>
            {{/each}}
            <tr class="subtotal-row">
                <td colspan="3">Subtotal</td>
                <td class="amount">{{subtotal}}</td>
            </tr>
            <tr>
                <td colspan="3">Tax ({{tax_rate}})</td>
                <td class="amount">{{tax_amount}}</td>
            </tr>
            <tr class="total-row">
                <td colspan="3">Total</td>
                <td class="amount">{{currency}} {{total}}</td>
            </tr>
        </tbody>
    </table>
    
    <div class="footer">
        <p><strong>Payment Terms:</strong> {{payment_terms}}</p>
        <p><strong>Bank:</strong> {{bank_name}} | <strong>Account:</strong> {{bank_account}}</p>
        <p style="margin-top: 20px;">Thank you for your business.</p>
    </div>
</body>
</html>)";
}

std::string TemplateEngine::getReportTemplate() {
    return R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    body { font-family: Arial, sans-serif; font-size: 11pt; color: #333; padding: 20px; }
    .letterhead { width: 100%; max-height: 80px; margin-bottom: 20px; }
    h1 { font-size: 20pt; color: #2c3e50; border-bottom: 2px solid #2c3e50; padding-bottom: 10px; margin-bottom: 20px; }
    h2 { font-size: 14pt; color: #34495e; margin-top: 25px; margin-bottom: 10px; }
    .meta { color: #666; margin-bottom: 20px; text-align: right; display: flex; justify-content: flex-end; gap: 20px; }
    .meta p { margin: 0; }
    table { width: 100%; border-collapse: collapse; margin: 15px 0; }
    th { background: #34495e; color: white; font-size: 11pt; text-align: left; padding: 8px; }
    td { padding: 8px; border-bottom: 1px solid #ddd; }
    .text-right { text-align: right; }
    tr:nth-child(even) { background: #f8f9fa; }
    .summary-row { font-weight: bold; background: #ecf0f1; }
    .footer { margin-top: 30px; font-size: 10pt; color: #666; border-top: 1px solid #ddd; padding-top: 10px; }
</style>
</head>
<body>
    <img src="{{letterhead_image}}" class="letterhead" onerror="this.style.display='none'">
    
    <h1>{{report_title}}</h1>
    
    <div class="meta">
        <p><strong>Date:</strong> {{date}}</p>
        <p><strong>Prepared by:</strong> {{author}}</p>
    </div>
    
    <h2>Summary</h2>
    <p>{{summary}}</p>
    
    <h2>Data</h2>
    <table>
        <thead>
            <tr>
                <th>{{col1_header}}</th>
                <th>{{col2_header}}</th>
                <th>{{col3_header}}</th>
            </tr>
        </thead>
        <tbody>
            {{#each rows}}
            <tr>
                <td>{{col1}}</td>
                <td class="text-right">{{col2}}</td>
                <td class="text-right">{{col3}}</td>
            </tr>
            {{/each}}
        </tbody>
    </table>
    
    <h2>Conclusion</h2>
    <p>{{conclusion}}</p>
    
    <div class="footer">
        <p>Report generated on {{date}}</p>
    </div>
</body>
</html>)";
}

std::string TemplateEngine::getLetterTemplate() {
    return R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    body { font-family: 'Times New Roman', serif; font-size: 12pt; color: #333; padding: 40px; line-height: 1.6; }
    .letterhead { width: 100%; max-height: 100px; margin-bottom: 30px; }
    .sender { margin-bottom: 30px; }
    .date { margin-bottom: 30px; }
    .recipient { margin-bottom: 30px; }
    .salutation { margin-bottom: 20px; }
    .body { margin-bottom: 30px; text-align: justify; }
    .closing { margin-top: 30px; }
    .signature { margin-top: 50px; }
</style>
</head>
<body>
    <img src="{{letterhead_image}}" class="letterhead" onerror="this.style.display='none'">
    
    <div class="sender">
        <strong>{{sender_name}}</strong><br>
        {{sender_address}}
    </div>
    
    <div class="date">{{date}}</div>
    
    <div class="recipient">
        <strong>{{recipient_name}}</strong><br>
        {{recipient_address}}
    </div>
    
    <div class="salutation">Dear {{recipient_name}},</div>
    
    <div class="body">{{body}}</div>
    
    <div class="closing">Sincerely,</div>
    
    <div class="signature">
        <strong>{{sender_name}}</strong><br>
        {{sender_title}}
    </div>
</body>
</html>)";
}
std::string TemplateEngine::getSalesSummaryTemplate() {
    return R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
        font-family: Arial, sans-serif; 
        font-size: 10pt; 
        color: #333; 
        padding: 20px;
        line-height: 1.4;
    }
    
    .header {
        display: flex;
        justify-content: space-between;
        margin-bottom: 10px;
    }
    .title { font-size: 12pt; font-weight: bold; }
    .meta { text-align: right; font-size: 9pt; }
    
    .outlet-info {
        margin: 15px 0;
        padding: 10px;
        background: {{letterhead_fill_color}};
        border: 1px solid {{box_color}};
    }
    .outlet-name { font-weight: bold; }
    
    .date-range {
        display: flex;
        justify-content: space-between;
        margin: 10px 0;
    }
    
    .shift-info {
        margin: 5px 0;
        text-align: right;
    }
    
    .section {
        margin: 15px 0;
    }
    .section-title {
        font-weight: bold;
        font-size: 10pt;
        margin-bottom: 5px;
        padding: 3px 0;
        border-bottom: 1px solid #333;
    }
    
    table {
        width: 100%;
        border-collapse: collapse;
        margin: 5px 0;
        font-size: 10pt;
    }
    
    th {
        background: {{theme_color}};
        color: white;
        font-size: 9pt;
        text-align: left;
        padding: 5px 8px;
    }
    th.amount { text-align: right; }
    
    td {
        padding: 4px 8px;
        border-bottom: 1px solid #eee;
    }
    td.amount { text-align: right; }
    
    .bordered td {
        border: 1px solid #ddd;
    }
    
    tr:nth-child(even) { background: #f9f9f9; }
    
    .total-row {
        font-weight: bold;
        background: #f0f0f0 !important;
    }
    .total-row td {
        border-top: 2px solid #333;
    }
    
    .summary-table {
        width: auto;
        min-width: 300px;
    }
    .summary-table td:first-child {
        padding-right: 30px;
    }
    
    .narrow-section {
        max-width: 400px;
    }
    
    .footer {
        margin-top: 30px;
        padding-top: 10px;
        border-top: 1px solid #ddd;
        font-size: 9pt;
        color: #666;
    }
</style>
</head>
<body>
    <div class="header">
        <div class="title">{{title}}</div>
        <div class="meta">
            <div>Date computed: {{date_computed}}</div>
            <div>Print at: {{terminal_name}}</div>
        </div>
    </div>
    
    <div class="outlet-info">
        <div class="outlet-name">{{outlet_code}} {{outlet_name}}</div>
        <div>{{outlet_name2}}</div>
        <div>{{outlet_address_1}}</div>
        <div>{{outlet_address_2}}</div>
        <div>{{outlet_address_3}}</div>
        <div>{{outlet_address_4}}</div>
    </div>
    
    <div class="date-range">
        <div>FROM: {{from_date}}</div>
        <div>TO: {{to_date}}</div>
    </div>
    
    <div>Number of receipts: {{num_receipts}}</div>
    
    {{#if is_shift}}
    <div class="shift-info">
        <div>Shift ID: {{shift_id}}</div>
        <div>Terminal: {{shift_terminal}}</div>
    </div>
    {{/if}}
    
    {{#if show_category}}
    <div class="section">
        <div class="section-title">TOTAL BY CATEGORY</div>
        <table class="summary-table">
            <tbody>
                {{#each categories}}
                <tr>
                    <td>{{name}}</td>
                    <td class="amount">{{amount}}</td>
                </tr>
                {{/each}}
                <tr class="total-row">
                    <td>**Total</td>
                    <td class="amount">{{total_sales}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    {{/if}}
    
    {{#if show_by_date}}
    <div class="section">
        <div class="section-title">TOTAL BY DATE</div>
        <table>
            <thead>
                <tr>
                    <th>DATE</th>
                    <th class="amount">GST</th>
                    <th class="amount">AMOUNT</th>
                    <th class="amount">TOTAL</th>
                </tr>
            </thead>
            <tbody>
                {{#each dates}}
                <tr>
                    <td>{{date}}</td>
                    <td class="amount">{{gst}}</td>
                    <td class="amount">{{amount}}</td>
                    <td class="amount">{{total}}</td>
                </tr>
                {{/each}}
                <tr class="total-row">
                    <td>**Total</td>
                    <td class="amount">{{dates_total_gst}}</td>
                    <td class="amount">{{dates_total_amount}}</td>
                    <td class="amount">{{dates_total}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    {{/if}}
    
    <div class="section narrow-section">
        <div class="section-title">TOTAL BY PAYMENT TYPE</div>
        <table class="summary-table">
            <thead>
                <tr>
                    <th>PAYMENT TYPE</th>
                    <th class="amount">AMOUNT</th>
                </tr>
            </thead>
            <tbody>
                {{#each payment_types}}
                <tr>
                    <td>{{name}}</td>
                    <td class="amount">{{amount}}</td>
                </tr>
                {{/each}}
                <tr class="total-row">
                    <td>**Total Discount &amp; Rounding</td>
                    <td class="amount">{{total_discount_rounding}}</td>
                </tr>
                <tr class="total-row">
                    <td>**Total GST Collected</td>
                    <td class="amount">{{total_gst}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    
    <div class="section narrow-section">
        <div class="section-title">CASH TAKEN OUT FROM DRAWER</div>
        <table class="summary-table">
            <thead>
                <tr>
                    <th>TYPE</th>
                    <th class="amount">AMOUNT</th>
                </tr>
            </thead>
            <tbody>
                {{#each cash_outs}}
                <tr>
                    <td>{{name}}</td>
                    <td class="amount">{{amount}}</td>
                </tr>
                {{/each}}
                <tr class="total-row">
                    <td>**Total Cash Taken Out</td>
                    <td class="amount">{{total_cash_out}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    
    <div class="section narrow-section">
        <table class="summary-table">
            <tbody>
                <tr>
                    <td>Return/Cancelled</td>
                    <td class="amount">{{return_cancelled}}</td>
                </tr>
                {{#if is_cash_sales}}
                {{#if is_shift}}
                <tr>
                    <td>STARTING CASH</td>
                    <td class="amount">{{starting_cash}}</td>
                </tr>
                {{/if}}
                <tr>
                    <td>CASH IN DRAWER</td>
                    <td class="amount">{{cash_in_drawer}}</td>
                </tr>
                {{#if is_shift}}
                <tr>
                    <td>REPORTED CLOSING CASH</td>
                    <td class="amount">{{closing_cash}}</td>
                </tr>
                {{/if}}
                {{/if}}
            </tbody>
        </table>
    </div>
    
    {{#if show_membership}}
    <div class="section narrow-section">
        <div class="section-title">MEMBERSHIP REWARDS</div>
        <table class="summary-table">
            <tbody>
                <tr>
                    <td>Point Given</td>
                    <td class="amount">{{points_given}}</td>
                </tr>
                <tr>
                    <td>Point Reimbursed</td>
                    <td class="amount">{{points_reimbursed}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    {{/if}}
    
    {{#if show_by_customer}}
    <div class="section">
        <div class="section-title">TOTAL BY CUSTOMER</div>
        <table>
            <thead>
                <tr>
                    <th>CUSTOMER</th>
                    <th class="amount">SALES</th>
                    <th class="amount">COST</th>
                    <th class="amount">MGN</th>
                </tr>
            </thead>
            <tbody>
                {{#each customers}}
                <tr>
                    <td>{{name}}</td>
                    <td class="amount">{{sales}}</td>
                    <td class="amount">{{cost}}</td>
                    <td class="amount">{{margin}}</td>
                </tr>
                {{/each}}
                <tr class="total-row">
                    <td>Total</td>
                    <td class="amount">{{customer_total_sales}}</td>
                    <td class="amount">{{customer_total_cost}}</td>
                    <td class="amount">{{customer_total_margin}}</td>
                </tr>
            </tbody>
        </table>
    </div>
    {{/if}}
    
    <div class="footer">
        Generated by PharmaPOS
    </div>
</body>
</html>)";
}