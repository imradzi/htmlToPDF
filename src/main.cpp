#include "template_engine.h"
#include "pdf_generator.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Handlebars Template to PDF Generator\n";
    std::cout << "=====================================\n\n";
    
    PdfGenerator generator;
    
    // Example 1: Generate an invoice PDF with line items
    {
        TemplateContext ctx;
        ctx.variables = {
            {"letterhead_image", "templates/letterhead.png"},
            {"invoice_number", "INV-2024-001"},
            {"date", "January 24, 2024"},
            {"due_date", "February 23, 2024"},
            {"customer_name", "Acme Corporation"},
            {"customer_address", "123 Main Street, Kuala Lumpur, Malaysia"},
            {"currency", "MYR"},
            {"subtotal", "4,500.00"},
            {"tax_rate", "6%"},
            {"tax_amount", "270.00"},
            {"total", "4,770.00"},
            {"payment_terms", "Net 30"},
            {"bank_name", "Maybank"},
            {"bank_account", "1234-5678-9012"}
        };
        
        // Invoice line items
        ctx.lists["items"] = std::vector<Item> {
            {{{"description", " 1. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", " 2. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", " 3. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", " 4. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", " 5. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", " 6. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", " 7. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", " 8. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", " 9. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "10. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "11. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "12. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "13. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "14. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "15. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "16. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "17. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "18. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "19. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "20. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "21. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "22. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "23. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "24. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "25. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "26. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "27. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "28. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "29. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "30. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "31. Web Development Services"}, {"qty", "20"}, {"unit_price", "150.00"}, {"amount", "3,000.00"}}},
            {{{"description", "32. UI/UX Design"}, {"qty", "10"}, {"unit_price", "100.00"}, {"amount", "1,000.00"}}},
            {{{"description", "33. Server Setup & Configuration"}, {"qty", "5"}, {"unit_price", "100.00"}, {"amount", "500.00"}}}};

        std::string html = TemplateEngine::render(TemplateEngine::getInvoiceTemplate(), ctx);
        generator.generate(html, "invoice.pdf");
    }
    
    // Example 2: Generate a tabular report PDF
    {
        TemplateContext ctx;
        ctx.variables = {
            {"letterhead_image", "templates/letterhead.png"},
            {"report_title", "Monthly Sales Report"},
            {"date", "January 2024"},
            {"author", "Sales Team"},
            {"summary", "This report covers the sales performance for January 2024. "
                        "Overall sales increased by 15% compared to the previous month."},
            {"col1_header", "Region"},
            {"col2_header", "Sales (MYR)"},
            {"col3_header", "Growth"},
            {"conclusion", "The overall performance exceeded expectations. "
                          "Recommend continued investment in Region C expansion."}
        };
        
        // Report data rows
        ctx.lists["rows"] = std::vector<Item> {
            {{{"col1", " 1. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", " 2. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", " 3. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", " 4. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", " 5. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", " 6. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", " 7. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", " 8. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", " 9. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", "10. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", "11. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", "12. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", "13. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", "14. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", "15. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", "16. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", "17. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", "18. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", "19. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", "20. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", "21. Region A - Retail"}, {"col2", "125,000"}, {"col3", "+20%"}}},
            {{{"col1", "22. Region B - Wholesale"}, {"col2", "98,500"}, {"col3", "+3%"}}},
            {{{"col1", "23. Region C - New Market"}, {"col2", "45,200"}, {"col3", "+25%"}}},
            {{{"col1", "24. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", "25. Region D - Online"}, {"col2", "78,300"}, {"col3", "+18%"}}},
            {{{"col1", "TOTAL"}, {"col2", "347,000"}, {"col3", "+15%"}}}};

        std::string html = TemplateEngine::render(TemplateEngine::getReportTemplate(), ctx);
        generator.generate(html, "report.pdf");
    }
    
    // Example 3: Generate a letter PDF
    {
        TemplateContext ctx;
        ctx.variables = {
            {"letterhead_image", "templates/letterhead.png"},
            {"sender_name", "ABC Corporation"},
            {"sender_address", "456 Business Ave, Suite 100<br>Kuala Lumpur, Malaysia"},
            {"sender_title", "Managing Director"},
            {"date", "January 24, 2024"},
            {"recipient_name", "Jane Smith"},
            {"recipient_address", "789 Client Street, Office 200<br>Petaling Jaya, Malaysia"},
            {"body", "We are pleased to inform you that your application has been approved. "
                     "Please find the attached documents for your review.<br><br>"
                     "Should you have any questions, please do not hesitate to contact us."}
        };
        
        std::string html = TemplateEngine::render(TemplateEngine::getLetterTemplate(), ctx);
        generator.generate(html, "letter.pdf");
    }
    
    std::cout << "\nAll PDFs generated successfully.\n";
    
    // Cleanup library
    PdfGenerator::deinitLibrary();
    
    return 0;
}
