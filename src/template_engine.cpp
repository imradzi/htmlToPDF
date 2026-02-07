#include "template_engine.h"
#include "template_strings.h"  // Auto-generated from HTML templates
#include <fstream>
#include <sstream>

// Helper: find innermost {{#if varName}} block (no nested {{#if}} inside)
static bool findInnermostIfBlock(const std::string& input, size_t& startPos, size_t& endPos,
                                  std::string& varName, std::string& blockContent) {
    const std::string ifStart = "{{#if ";
    const std::string ifEnd = "{{/if}}";
    
    size_t searchFrom = 0;
    while (true) {
        size_t ifPos = input.find(ifStart, searchFrom);
        if (ifPos == std::string::npos) return false;
        
        // Find the closing }} of {{#if varName}}
        size_t varStart = ifPos + ifStart.length();
        size_t closeTag = input.find("}}", varStart);
        if (closeTag == std::string::npos) return false;
        
        // Extract variable name (trim whitespace)
        std::string var = input.substr(varStart, closeTag - varStart);
        size_t ws = var.find_first_of(" \t\n\r}");
        if (ws != std::string::npos) var = var.substr(0, ws);
        
        size_t contentStart = closeTag + 2;
        
        // Find matching {{/if}} - but check for nested {{#if}} first
        size_t endIfPos = input.find(ifEnd, contentStart);
        if (endIfPos == std::string::npos) return false;
        
        // Check if there's a nested {{#if}} before this {{/if}}
        size_t nestedIf = input.find(ifStart, contentStart);
        if (nestedIf != std::string::npos && nestedIf < endIfPos) {
            // There's a nested if, skip to process it first
            searchFrom = nestedIf;
            continue;
        }
        
        // This is an innermost block
        startPos = ifPos;
        endPos = endIfPos + ifEnd.length();
        varName = var;
        blockContent = input.substr(contentStart, endIfPos - contentStart);
        return true;
    }
}

// Helper: remove unmatched {{variable}} placeholders
static std::string removeUnmatchedVariables(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    size_t pos = 0;
    while (pos < input.size()) {
        size_t start = input.find("{{", pos);
        if (start == std::string::npos) {
            result.append(input, pos, std::string::npos);
            break;
        }
        result.append(input, pos, start - pos);
        
        // Check if this is a control block (skip it)
        if (start + 2 < input.size() && (input[start + 2] == '#' || input[start + 2] == '/')) {
            result.append("{{");
            pos = start + 2;
            continue;
        }
        
        size_t end = input.find("}}", start + 2);
        if (end == std::string::npos) {
            result.append(input, start, std::string::npos);
            break;
        }
        // Skip the unmatched variable (don't append it)
        pos = end + 2;
    }
    return result;
}

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
    
    // Remove any remaining unmatched variables (without regex)
    result = removeUnmatchedVariables(result);
    
    return result;
}

std::string TemplateEngine::processIfBlocks(const std::string& input,
                                             const std::map<std::string, std::string>& variables) {
    std::string result = input;
    
    // Process innermost {{#if}} blocks iteratively until none remain
    size_t startPos, endPos;
    std::string varName, blockContent;
    
    while (findInnermostIfBlock(result, startPos, endPos, varName, blockContent)) {
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
        
        result = result.substr(0, startPos) + replacement + result.substr(endPos);
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
    
    const std::string eachStart = "{{#each ";
    const std::string eachEnd = "{{/each}}";
    
    size_t searchPos = 0;
    while (true) {
        size_t pos = result.find(eachStart, searchPos);
        if (pos == std::string::npos) break;
        
        // Find the closing }} of {{#each listName}}
        size_t varStart = pos + eachStart.length();
        size_t closeTag = result.find("}}", varStart);
        if (closeTag == std::string::npos) break;
        
        // Extract list name
        std::string listName = result.substr(varStart, closeTag - varStart);
        size_t ws = listName.find_first_of(" \t\n\r}");
        if (ws != std::string::npos) listName = listName.substr(0, ws);
        
        size_t contentStart = closeTag + 2;
        size_t endPos = result.find(eachEnd, contentStart);
        if (endPos == std::string::npos) break;
        
        std::string blockContent = result.substr(contentStart, endPos - contentStart);
        std::string replacement;
        
        auto it = lists.find(listName);
        if (it != lists.end()) {
            for (const auto& item : it->second) {
                std::string itemContent = blockContent;
                // Process {{#if}} blocks within the item context first
                itemContent = processIfBlocks(itemContent, item.fields);
                // Then replace variables
                for (const auto& [key, value] : item.fields) {
                    itemContent = replaceVariable(itemContent, key, value);
                }
                replacement += itemContent;
            }
        }
        
        result = result.substr(0, pos) + replacement + result.substr(endPos + eachEnd.length());
        // Don't advance searchPos - replacement might contain more {{#each}} blocks
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