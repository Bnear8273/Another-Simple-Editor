#include "editor_frame.h"

#include <wx/settings.h>
#include <wx/config.h>
#include <wx/checkbox.h>
#include <wx/fontenum.h>
#include <wx/fontpicker.h>
#include <wx/spinctrl.h>
#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#include <wx/fontutil.h>
#endif
#ifdef __WXGTK__
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include "pixel_font_rendering.h"
#endif

void EditorFrame::ApplySystemAppearance() {
    const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    constexpr double gutterTint = 0.16;
    const wxColour gutter(
        static_cast<unsigned char>(background.Red() * (1.0 - gutterTint) +
                                   foreground.Red() * gutterTint),
        static_cast<unsigned char>(background.Green() * (1.0 - gutterTint) +
                                   foreground.Green() * gutterTint),
        static_cast<unsigned char>(background.Blue() * (1.0 - gutterTint) +
                                   foreground.Blue() * gutterTint));
    editor_->StyleSetFont(wxSTC_STYLE_DEFAULT, editorFont_);
#if defined(__WXMSW__)
    editor_->SetFontQuality(pixelFontRendering_
        ? wxSTC_EFF_QUALITY_NON_ANTIALIASED
        : wxSTC_EFF_QUALITY_DEFAULT);
#endif
    editor_->StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
    editor_->StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
    editor_->StyleClearAll();
    editor_->StyleSetForeground(wxSTC_STYLE_LINENUMBER, foreground);
    editor_->StyleSetBackground(wxSTC_STYLE_LINENUMBER, gutter);
    editor_->SetBackgroundColour(background);
    editor_->SetCaretForeground(foreground);
    editor_->SetSelBackground(true,
        wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
    editor_->SetSelForeground(true,
        wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
    for (int margin = 0; margin < editor_->GetMarginCount(); ++margin) {
        editor_->SetMarginWidth(margin, 0);
    }
    editor_->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor_->SetMarginBackground(0, gutter);
    UpdateLineNumberWidth();
    editor_->SetMarginLeft(FromDIP(4));
    editor_->SetExtraDescent(FromDIP(extraLineSpacing_));
#if defined(__WXGTK__)
    ApplyPixelFontRendering();
#endif
    editor_->Refresh();
}

void EditorFrame::LoadEditorSettings() {
    wxConfig config(wxS("SingleCode"));
    wxString faceName;
    wxString fontInfo;
    long pointSize = editorFont_.IsOk() ? editorFont_.GetPointSize() : 10;
    long lineSpacing = 0;
    bool pixelFontRendering = false;
    config.Read(wxS("/Editor/Font"), &fontInfo);
    config.Read(wxS("/Editor/FontFace"), &faceName);
    config.Read(wxS("/Editor/FontSize"), &pointSize, pointSize);
    config.Read(wxS("/Editor/LineSpacing"), &lineSpacing, 0L);
    config.Read(wxS("/Editor/PixelFontRendering"), &pixelFontRendering,
                false);

    if (!fontInfo.empty()) {
        wxFont configuredFont;
        if (configuredFont.SetNativeFontInfo(fontInfo)) {
            editorFont_ = configuredFont;
        }
    }
    if (pointSize >= 6 && pointSize <= 72) {
        editorFont_.SetPointSize(static_cast<int>(pointSize));
    }
    if (!faceName.empty() && wxFontEnumerator::IsValidFacename(faceName)) {
        editorFont_.SetFaceName(faceName);
    }
    extraLineSpacing_ = static_cast<int>(wxClip(lineSpacing, 0L, 30L));
    pixelFontRendering_ = pixelFontRendering;
}

void EditorFrame::SaveEditorSettings() {
    wxConfig config(wxS("SingleCode"));
    config.Write(wxS("/Editor/Font"), editorFont_.GetNativeFontInfoDesc());
    config.Write(wxS("/Editor/FontFace"), editorFont_.GetFaceName());
    config.Write(wxS("/Editor/FontSize"), EffectiveFontSize());
    config.Write(wxS("/Editor/LineSpacing"), extraLineSpacing_);
    config.Write(wxS("/Editor/PixelFontRendering"), pixelFontRendering_);
    config.Flush();
}

int EditorFrame::EffectiveFontSize() const {
    return static_cast<int>(wxClip(
        editorFont_.GetPointSize() + editor_->GetZoom(), 6, 72));
}

void EditorFrame::SetEffectiveFontSize(int pointSize) {
    const int clampedSize = static_cast<int>(wxClip(pointSize, 6, 72));
    editor_->SetZoom(clampedSize - editorFont_.GetPointSize());
}

#ifdef __WXGTK__
void EditorFrame::ApplyPixelFontRendering() {
    auto* context = gtk_widget_get_pango_context(
        GTK_WIDGET(editor_->GetHandle()));
    auto* fontMap = PANGO_FC_FONT_MAP(
        pango_context_get_font_map(context));
    SetPixelFontRendering(fontMap, editorFont_.GetFaceName(), pixelFontRendering_);
}
#endif

void EditorFrame::OnEditorSettings(wxCommandEvent&) {
    wxDialog dialog(this, wxID_ANY, _("Editor Settings"), wxDefaultPosition,
                    wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, FromDIP(12), FromDIP(10));
    grid->AddGrowableCol(1, 1);
    wxFont settingsFont = editorFont_;
    settingsFont.SetPointSize(EffectiveFontSize());

    grid->Add(new wxStaticText(&dialog, wxID_ANY, _("Font")),
              0, wxALIGN_CENTER_VERTICAL);
    auto* fontPicker = new wxFontPickerCtrl(
        &dialog, wxID_ANY, settingsFont, wxDefaultPosition, wxDefaultSize,
        wxFNTP_FONTDESC_AS_LABEL);
    fontPicker->SetMinPointSize(6);
    fontPicker->SetMaxPointSize(72);
    grid->Add(fontPicker, 1, wxEXPAND);

    grid->Add(new wxStaticText(&dialog, wxID_ANY,
                               _("Line spacing (extra pixels)")),
              0, wxALIGN_CENTER_VERTICAL);
    auto* spacing = new wxSpinCtrl(
        &dialog, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 0, 30, extraLineSpacing_);
    grid->Add(spacing, 0, wxEXPAND);

    auto* pixelFontRendering = new wxCheckBox(
        &dialog, wxID_ANY, _("Pixel font rendering (disable anti-aliasing)"));
    pixelFontRendering->SetValue(pixelFontRendering_);
#if !defined(__WXGTK__) && !defined(__WXMSW__)
    pixelFontRendering->Disable();
#endif
    grid->Add(pixelFontRendering, 0, wxALIGN_CENTER_VERTICAL);
    grid->AddSpacer(0);

    auto* preview = new wxStaticText(&dialog, wxID_ANY, _("Aa Bb 0123"));
#ifdef __WXGTK__
    PangoFontMap* previewFontMap =
        pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
    if (previewFontMap != nullptr && PANGO_IS_FC_FONT_MAP(previewFontMap)) {
        gtk_widget_set_font_map(GTK_WIDGET(preview->GetHandle()), previewFontMap);
        g_object_unref(previewFontMap);
    } else if (previewFontMap != nullptr) {
        g_object_unref(previewFontMap);
        previewFontMap = nullptr;
    }
#endif
    const auto setPreviewFont = [preview, pixelFontRendering
#ifdef __WXGTK__
                                 , previewFontMap
#endif
    ](const wxFont& font) {
        wxFont previewFont = font;
        previewFont.SetPointSize(static_cast<int>(wxClip(
            font.GetPointSize(), 12, 24)));
#ifdef __WXGTK__
        if (previewFontMap != nullptr) {
            SetPixelFontRendering(PANGO_FC_FONT_MAP(previewFontMap),
                                  previewFont.GetFaceName(),
                                  pixelFontRendering->GetValue());
            GtkWidget* widget = GTK_WIDGET(preview->GetHandle());
            pango_context_changed(gtk_widget_get_pango_context(widget));
            gtk_widget_queue_resize(widget);
        }
#elif defined(__WXMSW__)
        if (pixelFontRendering->GetValue()) {
            wxNativeFontInfo fontInfo = *previewFont.GetNativeFontInfo();
            fontInfo.lf.lfQuality = NONANTIALIASED_QUALITY;
            previewFont.SetNativeFontInfo(fontInfo);
        }
#endif
        preview->SetFont(previewFont);
        preview->Refresh();
    };
    setPreviewFont(settingsFont);
    fontPicker->Bind(wxEVT_FONTPICKER_CHANGED,
                     [setPreviewFont](wxFontPickerEvent& event) {
                         setPreviewFont(event.GetFont());
                     });
    pixelFontRendering->Bind(wxEVT_CHECKBOX,
                             [fontPicker, setPreviewFont](wxCommandEvent&) {
                                 setPreviewFont(fontPicker->GetSelectedFont());
                             });

    outer->Add(grid, 0, wxALL | wxEXPAND, FromDIP(16));
    outer->Add(preview, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));
    outer->Add(dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL),
               0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));
    dialog.SetSizerAndFit(outer);
    dialog.SetMinSize(wxSize(FromDIP(390), -1));
    dialog.CentreOnParent();

    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxFont selectedFont = fontPicker->GetSelectedFont();
    if (selectedFont.IsOk()) {
        editorFont_ = selectedFont;
    }
    editor_->SetZoom(0);
    extraLineSpacing_ = spacing->GetValue();
    pixelFontRendering_ = pixelFontRendering->GetValue();
    ApplySystemAppearance();
    SaveEditorSettings();
    SetStatusText(_("Editor settings updated"), 0);
}
