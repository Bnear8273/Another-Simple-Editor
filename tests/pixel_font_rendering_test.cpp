#include "../src/pixel_font_rendering.h"

#include <pango/pangocairo.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

static int IntermediatePixels(const std::vector<unsigned char>& pixels) {
    int count = 0;
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if ((pixels[i] != 0 && pixels[i] != 255) ||
            (pixels[i + 1] != 0 && pixels[i + 1] != 255) ||
            (pixels[i + 2] != 0 && pixels[i + 2] != 255)) {
            ++count;
        }
    }
    return count;
}

static std::vector<unsigned char> Render(PangoFontMap* fontMap, const char* description = "Unifont 27", const char* sample = "Aa Bb 0123") {
    constexpr int width = 400;
    constexpr int height = 80;
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    cairo_t* cairo = cairo_create(surface);
    cairo_set_source_rgb(cairo, 1, 1, 1);
    cairo_paint(cairo);
    cairo_set_source_rgb(cairo, 0, 0, 0);
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options(cairo, options);

    PangoContext* context = pango_font_map_create_context(fontMap);
    PangoLayout* layout = pango_layout_new(context);
    PangoFontDescription* font = pango_font_description_from_string(description);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, sample, -1);
    pango_cairo_update_layout(cairo, layout);
    cairo_move_to(cairo, 8, 8);
    pango_cairo_show_layout(cairo, layout);
    cairo_surface_flush(surface);

    const int stride = cairo_image_surface_get_stride(surface);
    const auto* data = cairo_image_surface_get_data(surface);
    std::vector<unsigned char> pixels(data, data + stride * height);
    pango_font_description_free(font);
    g_object_unref(layout);
    g_object_unref(context);
    cairo_font_options_destroy(options);
    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
    return pixels;
}

int main() {
    FcPattern* query = FcNameParse(reinterpret_cast<const FcChar8*>("Unifont"));
    FcConfigSubstitute(nullptr, query, FcMatchPattern);
    FcDefaultSubstitute(query);
    FcResult result;
    FcPattern* match = FcFontMatch(nullptr, query, &result);
    FcChar8* family = nullptr;
    const bool hasUnifont = match != nullptr &&
        FcPatternGetString(match, FC_FAMILY, 0, &family) == FcResultMatch &&
        FcStrCmpIgnoreCase(family, reinterpret_cast<const FcChar8*>("Unifont")) == 0;
    if (match != nullptr) FcPatternDestroy(match);
    FcPatternDestroy(query);
    if (!hasUnifont) {
        std::puts("Unifont unavailable; skipping rendering test");
        return 77;
    }

    PangoFontMap* fontMap = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
    if (fontMap == nullptr || !PANGO_IS_FC_FONT_MAP(fontMap)) {
        std::fputs("Pango Fontconfig backend unavailable\n", stderr);
        return EXIT_FAILURE;
    }
    auto* fcMap = PANGO_FC_FONT_MAP(fontMap);
    SetPixelFontRendering(fcMap, wxString::FromUTF8("Unifont"), false);
    const auto original = Render(fontMap);
    SetPixelFontRendering(fcMap, wxString::FromUTF8("Unifont"), true);
    const auto pixelated = Render(fontMap);
    SetPixelFontRendering(fcMap, wxString::FromUTF8("Unifont"), false);
    const auto restored = Render(fontMap);
    wxArrayString names; names.Add("Unifont");
    SetPixelFontRendering(fcMap, names, false);
    const auto mixedBefore = Render(fontMap, "DejaVu Sans Mono,Unifont 27", "Aa 中文");
    SetPixelFontRendering(fcMap, names, true);
    const auto mixedAfter = Render(fontMap, "DejaVu Sans Mono,Unifont 27", "Aa 中文");
    auto leftGray = [](const std::vector<unsigned char>& image) {
        std::vector<unsigned char> left;
        for (int y = 0; y < 80; ++y) for (int x = 0; x < 55; ++x)
            for (int c = 0; c < 4; ++c) left.push_back(image[(y * 400 + x) * 4 + c]);
        return IntermediatePixels(left);
    };
    if (leftGray(mixedBefore) != leftGray(mixedAfter) || mixedBefore == mixedAfter) {
        std::fprintf(stderr, "Mixed fallback rendering incorrect: Latin %d -> %d, changed=%d\n", leftGray(mixedBefore), leftGray(mixedAfter), mixedBefore != mixedAfter);
        return EXIT_FAILURE;
    }
    g_object_unref(fontMap);
    const int originalGray = IntermediatePixels(original);
    const int pixelatedGray = IntermediatePixels(pixelated);
    if (originalGray == 0 || pixelatedGray != 0 || original != restored) {
        std::fprintf(stderr, "Intermediate pixels: original=%d, disabled=%d\n",
                     originalGray, pixelatedGray);
        std::fputs("Font rendering did not change and restore as expected\n", stderr);
        return EXIT_FAILURE;
    }
    std::puts("Font rendering changes and restores");
    return EXIT_SUCCESS;
}
