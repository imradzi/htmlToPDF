const express = require('express');
const hbs = require('hbs');
const path = require('path');
const browserSync = require('browser-sync').create();

const app = express();
const PORT = 3000;

// Set view engine to Handlebars
app.set('view engine', 'hbs');
app.set('views', path.join(__dirname, '../templates'));

// Serve static files from templates folder (for letterhead images)
app.use('/templates', express.static(path.join(__dirname, '../templates')));

// Register custom Handlebars helpers
hbs.registerHelper('eq', function(a, b) {
  return a === b;
});

hbs.registerHelper('formatAmount', function(amount, options) {
  const locale = (options.hash.locale || 'en_US').replace(/_/g, '-');
  if (!amount) return '0.00';
  return parseFloat(amount).toLocaleString(locale, {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  });
});

hbs.registerHelper('formatIBAN', function(iban) {
  if (!iban) return '';
  return iban.replace(/(.{4})/g, '$1 ').trim();
});

hbs.registerHelper('stringFormat', function(str) {
  return str ? String(str) : '';
});

// Register .html files as handlebars templates
app.engine('html', hbs.__express);

// Mock data for invoice template
const mockInvoiceData = {
  letterhead_image: '/templates/letterhead.png',
  invoice_number: 'INV-2024-001',
  date: 'January 28, 2026',
  due_date: 'February 28, 2026',
  customer_name: 'ABC Company Sdn Bhd',
  customer_address: '123 Jalan Sultan, 50000 Kuala Lumpur, Malaysia',
  items: [
    { description: 'Product A - Premium Widget', qty: 10, unit_price: 'RM 50.00', amount: 'RM 500.00' },
    { description: 'Product B - Standard Gadget', qty: 5, unit_price: 'RM 75.00', amount: 'RM 375.00' },
    { description: 'Service Charge - Installation', qty: 1, unit_price: 'RM 150.00', amount: 'RM 150.00' }
  ],
  subtotal: 'RM 1,025.00',
  tax_rate: '6%',
  tax_amount: 'RM 61.50',
  currency: 'RM',
  total: '1,086.50',
  payment_terms: 'Net 30 days',
  bank_name: 'Maybank',
  bank_account: '1234-5678-9012'
};

// Mock data for letter template
const mockLetterData = {
  letterhead_image: '/templates/letterhead.png',
  sender_name: 'John Smith',
  sender_address: '456 Business Park, 47800 Petaling Jaya, Selangor',
  date: 'January 28, 2026',
  recipient_name: 'Ms. Sarah Lee',
  recipient_address: 'HR Department, XYZ Corporation, 789 Corporate Tower, 50450 Kuala Lumpur',
  body: 'I am writing to express my interest in the open position at your esteemed organization. With over 10 years of experience in the industry, I believe I would be a valuable addition to your team. I have attached my resume for your review and would welcome the opportunity to discuss how my skills and experience align with your requirements. Please feel free to contact me at your earliest convenience to schedule an interview.',
  sender_title: 'Senior Manager'
};

// Mock data for report template
const mockReportData = {
  letterhead_image: '/templates/letterhead.png',
  report_title: 'Monthly Sales Performance Report',
  date: 'January 28, 2026',
  author: 'Finance Department',
  summary: 'This report summarizes the sales performance for the month of January 2026. Overall, the company has exceeded its quarterly targets by 15%, with strong performance across all product categories.',
  col1_header: 'Product Category',
  col2_header: 'Units Sold',
  col3_header: 'Revenue (RM)',
  rows: [
    { col1: 'Electronics', col2: '1,250', col3: '125,000.00' },
    { col1: 'Apparel', col2: '3,400', col3: '68,000.00' },
    { col1: 'Home & Living', col2: '890', col3: '44,500.00' },
    { col1: 'Food & Beverages', col2: '5,200', col3: '26,000.00' }
  ],
  conclusion: 'The sales performance this month demonstrates strong market demand across all categories. We recommend increasing inventory for Electronics and Apparel to meet anticipated demand in the coming quarter.'
};

// Mock data for sales summary template
const mockSalesSummaryData = {
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  theme_color: '#2c3e50',
  fill_color: '#e8f4f8',
  
  // Header
  title: 'SALES SUMMARY REPORT',
  date_computed: '28/01/2026 12:00:00',
  terminal_name: 'POS-01',
  
  // Outlet info
  outlet_code: 'KL001',
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: '',
  outlet_address_1: '123 Jalan Sultan',
  outlet_address_2: 'Bukit Bintang',
  outlet_address_3: '50000 Kuala Lumpur',
  outlet_address_4: 'Malaysia',
  
  // Date range
  from_date: '01/01/2026',
  to_date: '28/01/2026',
  num_receipts: 460,
  
  // Shift info (optional)
  is_shift: true,
  shift_id: 'SHIFT-2026-001',
  shift_terminal: 'POS-01',
  
  // Category section
  show_category: true,
  categories: [
    { name: 'Medicines', amount: '4,500.00' },
    { name: 'Supplements', amount: '2,550.00' },
    { name: 'Personal Care', amount: '1,600.00' },
    { name: 'Medical Devices', amount: '1,250.00' }
  ],
  total_sales: '9,900.00',
  
  // By date section
  show_by_date: true,
  dates: [
    { date: '01/01/2026', gst: '50.00', amount: '800.00', total: '850.00' },
    { date: '02/01/2026', gst: '45.00', amount: '720.00', total: '765.00' },
    { date: '03/01/2026', gst: '60.00', amount: '950.00', total: '1,010.00' }
  ],
  dates_total_gst: '155.00',
  dates_total_amount: '2,470.00',
  dates_total: '2,625.00',
  
  // Payment types
  payment_types: [
    { name: 'Cash', amount: '5,200.00' },
    { name: 'Credit Card', amount: '3,100.00' },
    { name: 'E-Wallet (Touch n Go)', amount: '1,200.00' },
    { name: 'E-Wallet (GrabPay)', amount: '400.00' }
  ],
  total_discount_rounding: '-50.00',
  total_gst: '594.00',
  
  // Cash out
  cash_outs: [
    { name: 'Bank Deposit', amount: '3,000.00' },
    { name: 'Petty Cash', amount: '200.00' }
  ],
  total_cash_out: '3,200.00',
  
  // Summary
  return_cancelled: '150.00',
  is_cash_sales: true,
  starting_cash: '500.00',
  cash_in_drawer: '2,500.00',
  closing_cash: '2,450.00',
  
  // Membership
  show_membership: true,
  points_given: '990',
  points_reimbursed: '150',
  
  // By customer section
  show_by_customer: true,
  customers: [
    { name: 'Walk-in Customer', sales: '6,500.00', cost: '4,200.00', margin: '35.4%' },
    { name: 'ABC Clinic', sales: '2,100.00', cost: '1,400.00', margin: '33.3%' },
    { name: 'XYZ Pharmacy', sales: '1,300.00', cost: '850.00', margin: '34.6%' }
  ],
  customer_total_sales: '9,900.00',
  customer_total_cost: '6,450.00',
  customer_total_margin: '34.8%'
};

// Routes
app.get('/', (req, res) => {
  res.send(`
    <html>
    <head>
      <title>Template Server</title>
      <style>
        body { font-family: Arial, sans-serif; padding: 40px; max-width: 800px; margin: 0 auto; }
        h1 { color: #2c3e50; }
        ul { list-style: none; padding: 0; }
        li { margin: 15px 0; }
        a { 
          display: inline-block;
          padding: 12px 24px;
          background: #3498db;
          color: white;
          text-decoration: none;
          border-radius: 5px;
          transition: background 0.3s;
        }
        a:hover { background: #2980b9; }
        .description { color: #666; margin-left: 10px; font-size: 14px; }
      </style>
    </head>
    <body>
      <h1>📄 Template Preview Server</h1>
      <p>Select a template to preview:</p>
      <ul>
        <li>
          <a href="/invoice">Invoice Template</a>
          <span class="description">- Standard invoice with items, tax, and totals</span>
        </li>
        <li>
          <a href="/letter">Letter Template</a>
          <span class="description">- Formal business letter format</span>
        </li>
        <li>
          <a href="/report">Report Template</a>
          <span class="description">- Data report with tables and summary</span>
        </li>
        <li>
          <a href="/sales-summary">Sales Summary Template</a>
          <span class="description">- Detailed sales summary report</span>
        </li>
      </ul>
      <hr style="margin-top: 40px;">
      <p style="color: #666; font-size: 12px;">
        Templates are loaded from: <code>htmlToPDF/templates/</code>
      </p>
    </body>
    </html>
  `);
});

app.get('/invoice', (req, res) => {
  res.render('invoice.html', mockInvoiceData);
});

app.get('/letter', (req, res) => {
  res.render('letter.html', mockLetterData);
});

app.get('/report', (req, res) => {
  res.render('report.html', mockReportData);
});

app.get('/sales-summary', (req, res) => {
  res.render('sales_summary.html', mockSalesSummaryData);
});

// Start server
const server = app.listen(PORT, () => {
  console.log(`\n✓ Server running at http://localhost:${PORT}`);
  console.log(`\nAvailable routes:`);
  console.log(`  - http://localhost:${PORT}                    (Template Index)`);
  console.log(`  - http://localhost:${PORT}/invoice            (Invoice Template)`);
  console.log(`  - http://localhost:${PORT}/letter             (Letter Template)`);
  console.log(`  - http://localhost:${PORT}/report             (Report Template)`);
  console.log(`  - http://localhost:${PORT}/sales-summary      (Sales Summary Template)`);
  console.log(`\n✓ BrowserSync enabled - browser will auto-refresh on changes`);
});

// BrowserSync setup for auto-refresh
browserSync.init({
  proxy: `localhost:${PORT}`,
  port: 3001,
  files: [
    '../templates/**/*.html',
    '../templates/**/*.hbs'
  ],
  reloadDelay: 1000,
  notify: false
});
