#include "template_engine.h"
#include "template_strings.h"  // Auto-generated from HTML templates
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
    return TemplateStrings::getInvoiceTemplate();
}

std::string TemplateEngine::getReportTemplate() {
    return TemplateStrings::getReportTemplate();
}

std::string TemplateEngine::getLetterTemplate() {
    return TemplateStrings::getLetterTemplate();
}

std::string TemplateEngine::getSalesSummaryTemplate() {
    return TemplateStrings::getSalesSummaryTemplate();
}

std::string TemplateEngine::getPurchaseSummaryTemplate() {
    return TemplateStrings::getPurchaseSummaryTemplate();
}

std::string TemplateEngine::getPoisonOrderTemplate() {
    return TemplateStrings::getPoisonOrderTemplate();
}

std::string TemplateEngine::getBillingStatementTemplate() {
    return TemplateStrings::getBillingStatementTemplate();
}

std::string TemplateEngine::getPurchaseOrderTemplate() {
    return TemplateStrings::getPurchaseOrderTemplate();
}