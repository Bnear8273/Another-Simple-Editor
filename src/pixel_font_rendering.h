#pragma once

#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>
#include <wx/arrstr.h>

inline void SetPixelFontRendering(PangoFcFontMap* fontMap, const wxArrayString& families,
                                  bool enabled) {
    FcConfig* config = FcInitLoadConfigAndFonts();
    if (!config) return;
    wxString xml = wxS("<?xml version=\"1.0\"?><!DOCTYPE fontconfig SYSTEM \"fonts.dtd\"><fontconfig>");
    if (enabled) {
      for (const auto& family : families) {
        wxString escaped = family;
        escaped.Replace("&", "&amp;");
        escaped.Replace("<", "&lt;");
        escaped.Replace(">", "&gt;");
        escaped.Replace("\"", "&quot;");
        xml += wxS("<match target=\"font\"><test name=\"family\" compare=\"eq\"><string>") + escaped +
               wxS("</string></test><edit name=\"antialias\" mode=\"assign\"><bool>false</bool></edit></match>");
      }
    }
    xml += wxS("</fontconfig>");
    auto utf8 = xml.utf8_str();
    if (FcConfigParseAndLoadFromMemory(config, reinterpret_cast<const FcChar8*>(utf8.data()), FcTrue))
        pango_fc_font_map_set_config(fontMap, config);
    FcConfigDestroy(config);
    pango_fc_font_map_config_changed(fontMap);
}
inline void SetPixelFontRendering(PangoFcFontMap* fontMap, const wxString& family,
                                  bool enabled) {
    wxArrayString families;
    families.Add(family);
    SetPixelFontRendering(fontMap, families, enabled);
}
