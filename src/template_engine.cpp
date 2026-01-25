#include "template_engine.h"
#include <regex>
#include <fstream>
#include <sstream>

std::string TemplateEngine::render(const std::string& templateStr, const TemplateContext& context) {
    std::string result = templateStr;
    
    // First process {{#each}} blocks
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
