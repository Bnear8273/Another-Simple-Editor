#pragma once

#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>
#include <wx/string.h>

inline void SetPixelFontRendering(PangoFcFontMap* fontMap, const wxString& faceName,
                                  bool enabled) {
    const wxScopedCharBuffer familyBuffer = faceName.utf8_str();
    const char* family = familyBuffer.data();
    if (enabled && family != nullptr && *family != '\0') {
        pango_fc_font_map_set_default_substitute(
            fontMap,
            [](FcPattern* pattern, gpointer data) {
                FcChar8* name = nullptr;
                for (int i = 0; FcPatternGetString(pattern, FC_FAMILY, i, &name)
                                == FcResultMatch; ++i) {
                    if (FcStrCmpIgnoreCase(name,
                            reinterpret_cast<const FcChar8*>(data)) == 0) {
                        FcPatternDel(pattern, FC_ANTIALIAS);
                        FcPatternAddBool(pattern, FC_ANTIALIAS, FcFalse);
                        break;
                    }
                }
            },
            g_strdup(family), g_free);
    } else {
        pango_fc_font_map_set_default_substitute(fontMap, nullptr, nullptr, nullptr);
    }
    pango_fc_font_map_substitute_changed(fontMap);
}
