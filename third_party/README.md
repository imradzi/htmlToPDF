# Third-party: PlutoBook

Vendored **PlutoBook 0.19.0** (`plutobook/`), the paged HTML renderer used by
`ppServer` for PDF generation.

- Upstream: https://github.com/plutoprint/plutobook
- License: Mozilla Public License 2.0 (see `plutobook/LICENSE`)
- Pinned version: 0.19.0 — upgrade deliberately and re-run the PDF verification
  (`htmlToPDF/tools/report_probe` + the sample generator).

Build artifacts are not committed:

- `prefix/linux-x64/` — produced by `htmlToPDF/scripts/build_plutobook_linux.sh`
- `prefix/win64/` — produced by `htmlToPDF/scripts/build_plutobook_mingw.sh`

The Windows build is x64 only, matching the ppServer support matrix (Windows 10+).
