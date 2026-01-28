const express = require('express');
const hbs = require('hbs');
const path = require('path');
const browserSync = require('browser-sync').create();

const app = express();
const PORT = 3000;

// Set view engine to Handlebars
app.set('view engine', 'hbs');
app.set('views', path.join(__dirname, '../../atlas-bootstrap-task/src/main/resources/content-service-repositories'));

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

// Mock data for payment template
const mockPaymentData = {
  paymentOrder: {
    paymentType: 'INTRABANK_TRANSFER',
    additions: {
      referenceId: 'REF20240127001',
      fromAccountDisplayName: 'John Doe - Savings Account',
      transferAmount: '1000.00',
      feeAmount: '0.00',
      displayName: 'Jane Smith',
      creditorName: 'Jane Smith',
      proxyValue: null,
      recipientReference: 'REC-PRIMARY-0001',
      transactionDate: 'Saturday, January 27, 2024 02:35 PM',
      transactionTime: 'Saturday, January 27, 2024 02:35 PM'
    },
    bankReferenceId: 'BANK20240127001',
    originatorAccount: {
      identification: {
        schemeName: 'IBAN',
        identification: 'MY8912345678901234567890'
      }
    },
    transferTransactionInformation: {
      instructedAmount: {
        amount: '1000.00'
      },
      counterparty: {
        name: 'Jane Smith'
      },
      counterpartyAccount: {
        identification: {
          schemeName: 'IBAN',
          identification: 'MY9198765432109876543210'
        }
      }
    }
  }
};

// Routes
app.get('/', (req, res) => {
  res.render('payments/templates/paymentTemplate', mockPaymentData);
});

app.get('/duitnow-acc', (req, res) => {
  const duitnowData = JSON.parse(JSON.stringify(mockPaymentData));
  duitnowData.paymentOrder.paymentType = 'DUITNOW_TRANSFER_ACC';
  duitnowData.paymentOrder.bankReferenceId = 'BANK20240127002';
  duitnowData.paymentOrder.additions.transferAmount = '500.00';
  duitnowData.paymentOrder.additions.displayName = 'Duitnow Recipient';
  duitnowData.paymentOrder.additions.recipientReference = 'REC-DUITNOW-ACC-0001';
  duitnowData.paymentOrder.additions.transactionDate = 'Wednesday, February 14, 2024 10:15 AM';
  duitnowData.paymentOrder.additions.transactionTime = 'Wednesday, February 14, 2024 10:15 AM';
  res.render('payments/templates/paymentTemplate', duitnowData);
});

app.get('/duitnow-qr', (req, res) => {
  const qrData = JSON.parse(JSON.stringify(mockPaymentData));
  qrData.paymentOrder.paymentType = 'DUITNOW_QR_PAYMENT';
  qrData.paymentOrder.bankReferenceId = 'BANK20240127003';
  qrData.paymentOrder.additions.transferAmount = '250.50';
  qrData.paymentOrder.additions.creditorName = 'QR Merchant';
  qrData.paymentOrder.additions.recipientReference = 'REC-DUITNOW-QR-0001';
  qrData.paymentOrder.additions.transactionDate = 'Friday, March 1, 2024 06:22 PM';
  qrData.paymentOrder.additions.transactionTime = 'Friday, March 1, 2024 06:22 PM';
  res.render('payments/templates/paymentTemplate', qrData);
});

// Start server
const server = app.listen(PORT, () => {
  console.log(`\n✓ Server running at http://localhost:${PORT}`);
  console.log(`\nAvailable routes:`);
  console.log(`  - http://localhost:${PORT}                    (Intrabank Transfer)`);
  console.log(`  - http://localhost:${PORT}/duitnow-acc        (DuitNow Account Transfer)`);
  console.log(`  - http://localhost:${PORT}/duitnow-qr         (DuitNow QR Payment)`);
  console.log(`\n✓ BrowserSync enabled - browser will auto-refresh on changes`);
});

// BrowserSync setup for auto-refresh
browserSync.init({
  proxy: `localhost:${PORT}`,
  port: 3001,
  files: [
    '../../atlas-bootstrap-task/src/main/resources/content-service-repositories/**/*.hbs'
  ],
  reloadDelay: 1000,
  notify: false
});
