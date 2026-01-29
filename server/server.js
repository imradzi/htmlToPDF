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
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  theme_color: '#2c3e50',
  fill_color: '#e8f4f8',
  
  // Outlet info
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: 'Pharmacy & Healthcare',
  outlet_address: '123 Jalan Sultan, Bukit Bintang\n50000 Kuala Lumpur, Malaysia',
  outlet_reg_no: '201901012345',
  outlet_gst_reg_no: 'GST-001234567',
  
  // e-Invoice (optional)
  has_e_invoice: true,
  e_invoice_qr: 'https://api.qrserver.com/v1/create-qr-code/?size=80x80&data=INV-2026-0042-LHDN-EINVOICE',
  
  // Document type flags
  is_invoice: true,
  is_purchase_order: false,
  is_goods_received: false,
  is_goods_return: false,
  is_draft: false,
  
  // Party info
  party_label: 'Invoice To:',
  invoice_to_name: 'ABC Company Sdn Bhd',
  invoice_to_address: '456 Jalan Ampang\n50450 Kuala Lumpur\nMalaysia',
  invoice_to_id: 'CUST-001',
  show_account_id: true,
  
  // Delivery address (optional)
  show_deliver_to: true,
  deliver_to_name: 'TESTING DELIVERY',
  deliver_to_address: '123 Delivery Lane\n50000 Kuala Lumpur\nMalaysia',
  
  // Document info
  document_type: 'INVOICE',
  transaction_date: '28/01/2026',
  ref_title: 'INV No:',
  ref_no: 'INV-2026-0042',
  id: '',
  page_no: '1',
  total_pages: '1',
  term: 'Net 30',
  
  // Items label
  items_label: 'Items sold:',
  
  // Display flags
  show_code: true,
  show_mal: true,
  show_batch_expiry: true,
  show_bonus: false,
  show_gst: true,
  show_srp: false,
  show_discount: true,
  
  // Items
  items: [
    { line_no: '1', code: 'MED-001', mal: 'MAL19991234A', name: 'Paracetamol 500mg', packing: '100s', batch_no: 'BN2025A', expiry_date: '12/2027', quantity: '5', price: '12.00', discount: '0.00', gst: '3.60', amount: '63.60', show_code: true, show_mal: true, show_batch_expiry: true, show_gst: true, show_discount: true },
    { line_no: '2', code: 'MED-002', mal: 'MAL19995678A', name: 'Ibuprofen 400mg', packing: '100s', batch_no: 'BN2025B', expiry_date: '06/2027', quantity: '3', price: '15.00', discount: '2.25', gst: '2.56', amount: '45.31', show_code: true, show_mal: true, show_batch_expiry: true, show_gst: true, show_discount: true },
    { line_no: '3', code: 'MED-003', mal: 'MAL19999012A', name: 'Vitamin B Complex', packing: '60s', batch_no: 'BN2025C', expiry_date: '09/2027', quantity: '10', price: '8.00', discount: '4.00', gst: '4.56', amount: '80.56', show_code: true, show_mal: true, show_batch_expiry: true, show_gst: true, show_discount: true }
  ],
  
  // Totals
  total_discount: '6.25',
  total_gst: '10.72',
  total_amount: '189.47',
  
  // Footer
  notes: [
    { text: 'Thank you for your business!' },
    { text: 'Payment due within 30 days.' }
  ],
  remarks: [
    { text: 'Goods sold are not returnable.' }
  ]
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

// Mock data for billing statement template
const mockBillingStatementData = {
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  theme_color: '#2c3e50',
  fill_color: '#e8f4f8',
  
  // Outlet info
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: 'Pharmacy & Healthcare',
  outlet_address: '123 Jalan Sultan, Bukit Bintang\n50000 Kuala Lumpur, Malaysia',
  outlet_reg_no: '201901012345',
  
  // Document info
  title: 'BILLING STATEMENT',
  period: 'January 2026',
  total_amount: 'RM 15,680.50',
  term: 'Net 30',
  
  // Debtor info
  debtor_id: 'CUST-001',
  debtor_name: 'ABC Clinic Sdn Bhd',
  debtor_address: '456 Jalan Ampang\n50450 Kuala Lumpur\nMalaysia',
  
  // Customer summary
  customers: [
    { name: 'Ahmad bin Ali', ic: '850101-14-5521', total: 'RM 3,250.00' },
    { name: 'Siti binti Hassan', ic: '900215-10-6632', total: 'RM 4,180.50' },
    { name: 'Tan Wei Ming', ic: '881030-08-4455', total: 'RM 8,250.00' }
  ],
  
  // Detailed items (optional)
  show_details: true,
  all_items: [
    { customer_name: 'Ahmad bin Ali', item: 'Paracetamol 500mg x 100', sales_ids: 'INV-2026-001', quantity: '5', amount: 'RM 125.00' },
    { customer_name: 'Ahmad bin Ali', item: 'Vitamin C 1000mg x 60', sales_ids: 'INV-2026-001', quantity: '3', amount: 'RM 89.00' },
    { customer_name: 'Siti binti Hassan', item: 'Blood Pressure Monitor', sales_ids: 'INV-2026-015', quantity: '1', amount: 'RM 280.00' },
    { customer_name: 'Tan Wei Ming', item: 'Insulin Pen x 5', sales_ids: 'INV-2026-022', quantity: '10', amount: 'RM 1,500.00' }
  ]
};

// Mock data for poison order template
const mockPoisonOrderData = {
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  theme_color: '#c0392b',
  fill_color: '#fce4e4',
  
  // Outlet info
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: 'Pharmacy & Healthcare',
  outlet_address: '123 Jalan Sultan, Bukit Bintang, 50000 Kuala Lumpur',
  outlet_reg_no: '201901012345',
  
  // Delivery info
  deliver_to_name: 'XYZ Medical Centre',
  deliver_to_address: '789 Jalan Hospital\n40000 Shah Alam\nSelangor',
  show_account_id: true,
  account_id: 'ACC-MED-001',
  
  // Document info
  title: 'POISON ORDER',
  transaction_date: '28/01/2026',
  ref_no: 'INV-2026-0145',
  id: 'DO-2026-0089',
  page_no: '1',
  total_pages: '1',
  term: 'Net 14',
  
  // Items
  items: [
    { line_no: '1', code: 'PSN-001', mal: 'MAL19985432A', name: 'Codeine Phosphate 30mg', batch_no: 'BN2025A', expiry_date: '12/2027', quantity: '100', uom: 'TAB' },
    { line_no: '2', code: 'PSN-002', mal: 'MAL19987654A', name: 'Morphine Sulphate 10mg', batch_no: 'BN2025B', expiry_date: '06/2027', quantity: '50', uom: 'AMP' },
    { line_no: '3', code: 'PSN-003', mal: 'MAL19989876A', name: 'Pethidine HCl 50mg/ml', batch_no: 'BN2025C', expiry_date: '09/2027', quantity: '20', uom: 'AMP' }
  ],
  
  // Footer
  purpose_of_sale: 'Medical Treatment',
  receiver_notes: [
    { text: 'Name: Dr. Ahmad bin Hassan' },
    { text: 'MMC No: 12345' },
    { text: 'Clinic: XYZ Medical Centre' }
  ],
  supplier_notes: [
    { text: 'Pharmacist: Lee Mei Ling' },
    { text: 'License No: PH12345' }
  ]
};

// Mock data for purchase order template
const mockPurchaseOrderData = {
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  theme_color: '#2c3e50',
  fill_color: '#e8f4f8',
  
  // Outlet info
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: 'Pharmacy & Healthcare',
  outlet_address: '123 Jalan Sultan, Bukit Bintang, 50000 Kuala Lumpur',
  outlet_reg_no: '201901012345',
  outlet_gst_reg_no: 'GST-001234567',
  
  // Document type flags
  is_purchase_order: true,
  is_goods_received: false,
  is_goods_return: false,
  
  // Supplier info
  invoice_to_name: 'ABC Pharma Supplies Sdn Bhd',
  invoice_to_address: '456 Industrial Park\n47500 Subang Jaya\nSelangor',
  
  // Document info
  document_type: 'PURCHASE ORDER',
  transaction_date: '28/01/2026',
  id: 'PO-2026-0045',
  ref_title: 'Quote Ref:',
  ref_no: 'QT-2026-0012',
  page_no: '1',
  total_pages: '1',
  term: 'Net 30',
  
  // Display flags
  show_code: true,
  show_mal: true,
  show_batch_expiry: false,
  show_gst: true,
  show_srp: true,
  
  // Items
  items: [
    { line_no: '1', code: 'MED-001', mal: 'MAL19991234A', name: 'Paracetamol 500mg x 100', quantity: '50', bonus: '5', price: '12.00', net_price: '11.50', selling_price: '18.00', margin: '36%', gst: '28.75', amount: '603.75', show_code: true, show_mal: true, show_gst: true, show_srp: true },
    { line_no: '2', code: 'MED-002', mal: 'MAL19995678A', name: 'Ibuprofen 400mg x 100', quantity: '30', bonus: '3', price: '15.00', net_price: '14.25', selling_price: '22.00', margin: '35%', gst: '21.38', amount: '448.88', show_code: true, show_mal: true, show_gst: true, show_srp: true },
    { line_no: '3', code: 'MED-003', mal: 'MAL19999012A', name: 'Vitamin B Complex x 60', quantity: '100', bonus: '10', price: '8.00', net_price: '7.60', selling_price: '12.00', margin: '37%', gst: '38.00', amount: '798.00', show_code: true, show_mal: true, show_gst: true, show_srp: true }
  ],
  
  // Totals
  total_gst: '88.13',
  total_amount: '1,850.63',
  
  // Remarks
  has_remarks: true,
  remarks: [
    { text: 'Please deliver before 15/02/2026' },
    { text: 'Contact person: Ahmad (012-3456789)' }
  ]
};

// Mock data for purchase summary template
const mockPurchaseSummaryData = {
  // Style colors
  letterhead_fill_color: '#f5f5f5',
  box_color: '#ddd',
  
  // Header
  title: 'PURCHASE SUMMARY REPORT',
  date_computed: '28/01/2026 14:30:00',
  
  // Outlet info
  outlet_code: 'KL001',
  outlet_name: 'PharmaPOS Main Branch',
  outlet_name2: 'Pharmacy & Healthcare',
  outlet_address_1: '123 Jalan Sultan',
  outlet_address_2: 'Bukit Bintang',
  outlet_address_3: '50000 Kuala Lumpur',
  outlet_address_4: 'Malaysia',
  
  // Date range
  from_date: '01/01/2026',
  to_date: '28/01/2026',
  
  // Categories
  categories: [
    { name: 'Medicines', gst: '1,250.00', amount: '25,000.00' },
    { name: 'Supplements', gst: '680.00', amount: '13,600.00' },
    { name: 'Medical Devices', gst: '450.00', amount: '9,000.00' },
    { name: 'Personal Care', gst: '320.00', amount: '6,400.00' }
  ],
  total_category_gst: '2,700.00',
  total_category_amount: '54,000.00',
  
  // Payment types
  payment_types: [
    { name: 'Bank Transfer', amount: '35,000.00' },
    { name: 'Credit Term', amount: '15,000.00' },
    { name: 'Cash', amount: '4,000.00' }
  ],
  return_cancelled: '-500.00',
  
  // Suppliers
  suppliers: [
    { name: 'ABC Pharma Supplies', gst: '1,200.00', amount: '24,000.00' },
    { name: 'XYZ Medical Distributors', gst: '850.00', amount: '17,000.00' },
    { name: 'Healthcare Wholesale', gst: '650.00', amount: '13,000.00' }
  ],
  total_supplier_gst: '2,700.00',
  total_supplier_amount: '54,000.00'
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
        <li>
          <a href="/billing-statement">Billing Statement Template</a>
          <span class="description">- Customer billing with itemized details</span>
        </li>
        <li>
          <a href="/poison-order">Poison Order Template</a>
          <span class="description">- Controlled substance delivery order</span>
        </li>
        <li>
          <a href="/purchase-order">Purchase Order Template</a>
          <span class="description">- Supplier purchase order with GST</span>
        </li>
        <li>
          <a href="/purchase-summary">Purchase Summary Template</a>
          <span class="description">- Purchase summary by category/supplier</span>
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

app.get('/billing-statement', (req, res) => {
  res.render('billing_statement.html', mockBillingStatementData);
});

app.get('/poison-order', (req, res) => {
  res.render('poison_order.html', mockPoisonOrderData);
});

app.get('/purchase-order', (req, res) => {
  res.render('purchase_order.html', mockPurchaseOrderData);
});

app.get('/purchase-summary', (req, res) => {
  res.render('purchase_summary.html', mockPurchaseSummaryData);
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
  console.log(`  - http://localhost:${PORT}/billing-statement  (Billing Statement Template)`);
  console.log(`  - http://localhost:${PORT}/poison-order       (Poison Order Template)`);
  console.log(`  - http://localhost:${PORT}/purchase-order     (Purchase Order Template)`);
  console.log(`  - http://localhost:${PORT}/purchase-summary   (Purchase Summary Template)`);
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
