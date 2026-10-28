# Translations

User-facing strings in `src/main.cpp` are English source messages wrapped in
wxWidgets' `_()` translation macro. Translation catalogs use GNU gettext PO
files named by locale, such as `zh_CN.po` or `fr_FR.po`.

To refresh the message template and a locale catalog (requires gettext tools):

```sh
xgettext --language=C++ --keyword=_ --from-code=UTF-8 \
  --output=locale/singlecode.pot src/main.cpp
msgmerge --update locale/zh_CN.po locale/singlecode.pot
```

Add another `<locale>.po` file beside `zh_CN.po`. CMake compiles each catalog
with `msgfmt` when it is installed. The resulting catalog name is
`singlecode.mo`; the application loads it using the system locale. If a catalog
is missing, the English source message is shown.
