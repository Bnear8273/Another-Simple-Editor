#include "editor_frame.h"

#include <wx/settings.h>
#include <wx/config.h>
#include <wx/checkbox.h>
#include <wx/fontenum.h>
#include <wx/fontdlg.h>
#include <wx/listbox.h>
#include <wx/spinctrl.h>
#include <functional>
#include <memory>
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
#ifdef __WXGTK__
    if (!fontFamilies_.empty()) {
        editor_->StyleSetFaceName(wxSTC_STYLE_DEFAULT, wxJoin(fontFamilies_, ','));
    }
#endif
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
    long familyCount = 0;
    config.Read(wxS("/Editor/FontFamilies/Count"), &familyCount, 0L);
    for (long i = 0; i < wxMin(familyCount, 100L); ++i) {
        wxString family;
        if (config.Read(wxString::Format(wxS("/Editor/FontFamilies/%ld"), i), &family) &&
            !family.empty() && fontFamilies_.Index(family, false) == wxNOT_FOUND) {
            fontFamilies_.Add(family);
        }
    }
    if (fontFamilies_.empty()) {
        fontFamilies_.Add(editorFont_.GetFaceName());
    }
    for (size_t i = 0; i < fontFamilies_.size(); ++i) {
        bool disabled = pixelFontRendering;
        config.Read(wxString::Format(wxS("/Editor/FontAntialias/%zu"), i), &disabled, pixelFontRendering);
        if (disabled) pixelFontFamilies_.Add(fontFamilies_[i]);
    }
    extraLineSpacing_ = static_cast<int>(wxClip(lineSpacing, 0L, 30L));
    pixelFontRendering_ = pixelFontFamilies_.Index(fontFamilies_[0], false) != wxNOT_FOUND;
}

void EditorFrame::SaveEditorSettings() {
    wxConfig config(wxS("SingleCode"));
    config.DeleteGroup(wxS("/Editor/FontFamilies"));
    config.Write(wxS("/Editor/FontFamilies/Count"), static_cast<long>(fontFamilies_.size()));
    for (size_t i = 0; i < fontFamilies_.size(); ++i) {
        config.Write(wxString::Format(wxS("/Editor/FontFamilies/%zu"), i), fontFamilies_[i]);
    }
    config.DeleteGroup(wxS("/Editor/FontAntialias"));
    for (size_t i = 0; i < fontFamilies_.size(); ++i) {
        config.Write(wxString::Format(wxS("/Editor/FontAntialias/%zu"), i),
                     pixelFontFamilies_.Index(fontFamilies_[i], false) != wxNOT_FOUND);
    }
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
    PangoFontMap* fontMap = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
    if (fontMap && PANGO_IS_FC_FONT_MAP(fontMap)) {
        SetPixelFontRendering(PANGO_FC_FONT_MAP(fontMap), pixelFontFamilies_, true);
        gtk_widget_set_font_map(GTK_WIDGET(editor_->GetHandle()), fontMap);
    }
    if (fontMap) g_object_unref(fontMap);
}
#endif

void EditorFrame::OnEditorSettings(wxCommandEvent&) {
    wxDialog dialog(this, wxID_ANY, _("Editor Settings"), wxDefaultPosition,
                    wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    auto* outer = new wxBoxSizer(wxVERTICAL);
#ifdef __WXGTK__
    const wxString fontLabel = _("Fonts (drag to reorder)");
#else
    const wxString fontLabel = _("Font");
#endif
    outer->Add(new wxStaticText(&dialog, wxID_ANY, fontLabel),
               0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    auto* list = new wxListBox(&dialog, wxID_ANY, wxDefaultPosition,
                               FromDIP(wxSize(320, 110)), fontFamilies_, wxLB_SINGLE);
    outer->Add(list, 1, wxALL | wxEXPAND, FromDIP(12));
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* add = new wxButton(&dialog, wxID_ADD, _("Add..."));
    auto* remove = new wxButton(&dialog, wxID_REMOVE, _("Remove"));
    actions->Add(add, 0, wxRIGHT, FromDIP(8));
    actions->Add(remove);
    outer->Add(actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

#ifndef __WXGTK__
    add->SetLabel(_("Change..."));
    outer->Add(new wxStaticText(&dialog, wxID_ANY,
        _("Ordered font fallback is currently available on GTK only.")),
        0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
#endif

    auto* grid = new wxFlexGridSizer(2, FromDIP(10), FromDIP(10));
    grid->AddGrowableCol(1, 1);
    grid->Add(new wxStaticText(&dialog, wxID_ANY, _("Font size")),
              0, wxALIGN_CENTER_VERTICAL);
    auto* size = new wxSpinCtrl(&dialog, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                wxDefaultSize, wxSP_ARROW_KEYS, 6, 72, EffectiveFontSize());
    grid->Add(size, 0, wxEXPAND);
    grid->Add(new wxStaticText(&dialog, wxID_ANY, _("Line spacing (extra pixels)")),
              0, wxALIGN_CENTER_VERTICAL);
    auto* spacing = new wxSpinCtrl(&dialog, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                   wxDefaultSize, wxSP_ARROW_KEYS, 0, 30, extraLineSpacing_);
    grid->Add(spacing, 0, wxEXPAND);
    outer->Add(grid, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));

    auto disabledFamilies = std::make_shared<wxArrayString>(pixelFontFamilies_);
    auto* pixelFontRendering = new wxCheckBox(
        &dialog, wxID_ANY, _("Disable anti-aliasing for selected font"));
    pixelFontRendering->SetValue(pixelFontRendering_);
#if !defined(__WXGTK__) && !defined(__WXMSW__)
    pixelFontRendering->Disable();
#endif
    outer->Add(pixelFontRendering, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    auto* preview = new wxStyledTextCtrl(&dialog, wxID_ANY, wxDefaultPosition,
                                         FromDIP(wxSize(320, 150)), wxBORDER_SIMPLE);
    preview->SetText(_("Aa Bb 0123 中文"));
    for (int i = 0; i < preview->GetMarginCount(); ++i) preview->SetMarginWidth(i, 0);
#ifdef __WXGTK__
    PangoFontMap* previewFontMap = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
    if (previewFontMap != nullptr && PANGO_IS_FC_FONT_MAP(previewFontMap)) {
        gtk_widget_set_font_map(GTK_WIDGET(preview->GetHandle()), previewFontMap);
        g_object_unref(previewFontMap);
    } else if (previewFontMap != nullptr) {
        g_object_unref(previewFontMap);
        previewFontMap = nullptr;
    }
#endif
    auto lastPreviewFamilies = std::make_shared<wxArrayString>();
    auto previewInitialized = std::make_shared<bool>(false);
    auto updatePreview = [=]() {
        wxFont font = editorFont_;
        if (list->GetCount() > 0) font.SetFaceName(list->GetString(0));
        font.SetPointSize(wxClip(size->GetValue(), 6, 72));
        preview->StyleSetFont(wxSTC_STYLE_DEFAULT, font);
        preview->StyleSetBackground(wxSTC_STYLE_DEFAULT,
            wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        preview->StyleSetForeground(wxSTC_STYLE_DEFAULT,
            wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#ifdef __WXGTK__
        wxArrayString families;
        for (unsigned int i = 0; i < list->GetCount(); ++i) families.Add(list->GetString(i));
        preview->StyleSetFaceName(wxSTC_STYLE_DEFAULT, wxJoin(families, ','));
        if (previewFontMap != nullptr && (!*previewInitialized || *lastPreviewFamilies != *disabledFamilies)) {
            SetPixelFontRendering(PANGO_FC_FONT_MAP(previewFontMap), *disabledFamilies, true);
            pango_context_changed(gtk_widget_get_pango_context(GTK_WIDGET(preview->GetHandle())));
            *lastPreviewFamilies = *disabledFamilies;
            *previewInitialized = true;
        }
#elif defined(__WXMSW__)
        preview->SetFontQuality(pixelFontRendering->GetValue()
            ? wxSTC_EFF_QUALITY_NON_ANTIALIASED : wxSTC_EFF_QUALITY_DEFAULT);
#endif
        preview->StyleClearAll();
        preview->SetExtraDescent(FromDIP(spacing->GetValue()));
        preview->Refresh();
        remove->Enable(list->GetSelection() != wxNOT_FOUND && list->GetCount() > 1);
    };
    if (list->GetCount()) list->SetSelection(0);
    auto updateSelection = [=]() {
        const int selected = list->GetSelection();
        pixelFontRendering->SetValue(selected != wxNOT_FOUND &&
            disabledFamilies->Index(list->GetString(selected), false) != wxNOT_FOUND);
        updatePreview();
    };
    list->Bind(wxEVT_LISTBOX, [updateSelection](wxCommandEvent&) { updateSelection(); });
    add->Bind(wxEVT_BUTTON, [&, updateSelection](wxCommandEvent&) {
        wxFontData data;
        data.SetInitialFont(editorFont_);
        wxFontDialog picker(&dialog, data);
        if (picker.ShowModal() != wxID_OK) return;
        const wxString family = picker.GetFontData().GetChosenFont().GetFaceName();
        if (family.empty()) return;
        const int existing = list->FindString(family, false);
#ifdef __WXGTK__
        if (existing == wxNOT_FOUND) {
            list->Append(family);
            list->SetSelection(list->GetCount() - 1);
        } else {
            list->SetSelection(existing);
        }
#else
        (void)existing;
        list->Clear();
        list->Append(family);
        list->SetSelection(0);
#endif
        updateSelection();
    });
    remove->Bind(wxEVT_BUTTON, [=](wxCommandEvent&) {
        const int selected = list->GetSelection();
        if (selected != wxNOT_FOUND && list->GetCount() > 1) {
            const int disabledIndex = disabledFamilies->Index(list->GetString(selected), false);
            if (disabledIndex != wxNOT_FOUND) disabledFamilies->RemoveAt(disabledIndex);
            list->Delete(selected);
            list->SetSelection(wxMin(selected, static_cast<int>(list->GetCount()) - 1));
            updateSelection();
        }
    });
#ifdef __WXGTK__
    gtk_tree_view_set_reorderable(list->m_treeview, TRUE);
    list->Bind(wxEVT_CHAR_HOOK, [=](wxKeyEvent& event) {
        const int selected = list->GetSelection();
        const int direction = event.GetKeyCode() == WXK_UP ? -1 :
                              event.GetKeyCode() == WXK_DOWN ? 1 : 0;
        const int target = selected + direction;
        if (!event.AltDown() || direction == 0 || selected == wxNOT_FOUND ||
            target < 0 || target >= static_cast<int>(list->GetCount())) {
            event.Skip();
            return;
        }
        const wxString family = list->GetString(selected);
        list->Delete(selected);
        list->Insert(family, target);
        list->SetSelection(target);
        updateSelection();
    });
    g_signal_connect_data(
        list->m_treeview, "drag-end",
        G_CALLBACK(+[](GtkWidget*, GdkDragContext*, gpointer data) {
            (*static_cast<std::function<void()>*>(data))();
        }),
        new std::function<void()>(updateSelection),
        +[](gpointer data, GClosure*) { delete static_cast<std::function<void()>*>(data); },
        GConnectFlags(0));
#endif
    size->Bind(wxEVT_SPINCTRL, [updatePreview](wxSpinEvent&) { updatePreview(); });
    size->Bind(wxEVT_TEXT, [updatePreview](wxCommandEvent&) { updatePreview(); });
    spacing->Bind(wxEVT_SPINCTRL, [updatePreview](wxSpinEvent&) { updatePreview(); });
    spacing->Bind(wxEVT_TEXT, [updatePreview](wxCommandEvent&) { updatePreview(); });
    pixelFontRendering->Bind(wxEVT_CHECKBOX, [=](wxCommandEvent&) {
        const int selected = list->GetSelection();
        if (selected == wxNOT_FOUND) return;
        const wxString family = list->GetString(selected);
        const int index = disabledFamilies->Index(family, false);
        if (pixelFontRendering->GetValue() && index == wxNOT_FOUND) disabledFamilies->Add(family);
        if (!pixelFontRendering->GetValue() && index != wxNOT_FOUND) disabledFamilies->RemoveAt(index);
        updatePreview();
    });
    updateSelection();
    outer->Add(preview, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));
    outer->Add(dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL),
               0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(12));
    dialog.SetSizerAndFit(outer);
    dialog.SetMinSize(wxSize(FromDIP(390), -1));
    dialog.CentreOnParent();
    if (dialog.ShowModal() != wxID_OK) return;
    fontFamilies_.Clear();
    for (unsigned int i = 0; i < list->GetCount(); ++i) fontFamilies_.Add(list->GetString(i));
    if (!fontFamilies_.empty()) editorFont_.SetFaceName(fontFamilies_[0]);
    editorFont_.SetPointSize(size->GetValue());
    editor_->SetZoom(0);
    extraLineSpacing_ = spacing->GetValue();
    pixelFontFamilies_ = *disabledFamilies;
    pixelFontRendering_ = pixelFontFamilies_.Index(fontFamilies_[0], false) != wxNOT_FOUND;
    ApplySystemAppearance();
    SaveEditorSettings();
    SetStatusText(_("Editor settings updated"), 0);
}
