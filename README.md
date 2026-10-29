# Another Simple Editor

A lightweight wxWidgets text editor with open, save, and save-as support.

## Build

Requires wxWidgets 3.2 or newer (including the STC component) and CMake.

```sh
cmake -S . -B build
cmake --build build
```

The executable is named `another-simple-editor`. On Linux, `wx-config` can provide
the wxWidgets build configuration.

## Localization

All user-facing strings use wxWidgets' gettext translation API. The app selects
the catalog for the system locale and falls back to the English source strings
when no translation is installed. Translation catalogs are optional; when
`msgfmt` is available, CMake compiles `locale/*.po` and installs them under
`${CMAKE_INSTALL_DATADIR}/locale`.

See [locale/README.md](locale/README.md) for updating and adding translations.

## Platform notes

- Drag-to-reorder font fallback has only been tested on Linux with GTK. Windows and macOS behavior has not been tested.

## Known issues

- On Wayland, after entering text with an input method, opening the Settings menu can emit `Gdk-WARNING: Tried to map a popup with a non-top most parent`. The warning occurs while GTK maps the menu; the Wayland trace shows another small popup still active at that time. Its exact relationship to the input method has not been confirmed.
