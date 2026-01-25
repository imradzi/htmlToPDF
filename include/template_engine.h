#pragma once

#include <string>
#include <map>
#include <vector>

// Represents a single item (e.g., invoice line item)
struct Item {
    std::map<std::string, std::string> fields;
};

// Context for template rendering
struct TemplateContext {
    std::map<std::string, std::string> variables;      // Simple key-value pairs
    std::map<std::string, std::vector<Item>> lists;    // Arrays for {{#each}}
};

class TemplateEngine {
public:
    // Render a template string with the given context
    static std::string render(const std::string& templateStr, const TemplateContext& context);
    
    // Load template from file
    static std::string loadTemplate(const std::string& path);
    
    // Get built-in templates (HTML)
    static std::string getInvoiceTemplate();
    static std::string getReportTemplate();
    static std::string getLetterTemplate();
    static std::string getSalesSummaryTemplate();

private:
    static std::string replaceVariable(const std::string& input, 
                                        const std::string& key, 
                                        const std::string& value);
    static std::string processEachBlocks(const std::string& input,
                                          const std::map<std::string, std::vector<Item>>& lists);
    static std::string processIfBlocks(const std::string& input,
                                         const std::map<std::string, std::string>& variables);
};
