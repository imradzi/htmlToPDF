# htmlToPDF

PDF output is rendered by **PlutoBook** (vendored, see `third_party/README.md`).
Build the engine before configuring the project:

- Linux: `./scripts/build_plutobook_linux.sh`
- Windows x64 (MinGW cross): `./scripts/build_plutobook_mingw.sh`

Verification tools:
- `handlebars_pdf` — renders the bundled sample templates
- `report_probe` — exercises `HtmlReportBuilder` running headers/footers and buffer output
- `scripts/verify_pdf.sh` — builds both tools and checks the generated PDFs (needs `poppler-utils`)
