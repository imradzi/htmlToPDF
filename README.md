# htmlToPDF

PDF output is rendered by **PlutoBook** (vendored, see `third_party/README.md`).
Build the engine before configuring the project:

- Linux: `./scripts/build_plutobook_linux.sh`
- Windows x64 (MinGW cross): `./scripts/build_plutobook_mingw.sh`
  The Windows script also replaces ICU's stub `libsicudt.a` with the real data
  archive from `bin/sicudt.a`; without the data PlutoBook aborts in
  `BreakIterator::createCharacterInstance` as soon as it lays out text.

Verification tools:
- `handlebars_pdf` — renders the bundled sample templates
- `report_probe` — exercises `HtmlReportBuilder` running headers/footers and buffer output
- `html2pdf <in.html> <out.pdf> [--margin-top N] [--scale X] ...` — converts an arbitrary
  HTML file, for A/B diffs against `wkhtmltopdf` on identical input
- `scripts/verify_pdf.sh` — builds both tools and checks the generated PDFs (needs `poppler-utils`)

## Output parity with wkhtmltopdf

The old engine is still the reference for layout, so two quirks are reproduced:

- **Content scale 0.8** (`PdfSettings::contentScale`). wkhtmltopdf's default "smart
  shrinking" renders every CSS length (pt/mm/px) at 80 % of nominal while the paper and
  margins keep their real size. `PdfGenerator` lays the document out on a 1/0.8 larger
  page, inflates margins, header/footer font sizes and the `--ppos-*` variables, then
  paints through a `PDFCanvas` scaled by 0.8. Set `contentScale = 1.0` for standard CSS
  sizing.
- **Whole-pixel lengths.** wkhtmltopdf truncated CSS lengths to whole device pixels
  (96 dpi), e.g. `padding: 1.5mm` became 5 px and `line-height: 1.3` on 9 pt text became
  15 px. Templates therefore use `padding: 7px 5px` / `5px 5px` and `line-height: 1.25`
  in table cells; keep new repetitive (grid) metrics in px for the same reason.

Header and footer margin boxes are bottom/top aligned inside the page margin
(`vertical-align`) and the header block leaves wkhtmltopdf's 5 mm `header.spacing`, so the
top margin looks the same as before.
