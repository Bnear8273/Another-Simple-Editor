# SingleCode Editor

A lightweight wxWidgets text editor with open, save, and save-as support.

## 构建

Requires wxWidgets 3.2 or newer (including the STC component) and CMake.

```sh
cmake -S . -B build
cmake --build build
```

The executable is named `singlecode-editor`. On Linux, `wx-config` can provide
the wxWidgets build configuration.

## Localization

All user-facing strings use wxWidgets' gettext translation API. The app selects
the catalog for the system locale and falls back to the English source strings
when no translation is installed. Translation catalogs are optional; when
`msgfmt` is available, CMake compiles `locale/*.po` and installs them under
`${CMAKE_INSTALL_DATADIR}/locale`.

See [locale/README.md](locale/README.md) for updating and adding translations.
