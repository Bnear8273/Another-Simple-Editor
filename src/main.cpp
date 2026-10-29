#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/fdrepdlg.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>
#include <wx/config.h>
#include <wx/checkbox.h>
#include <wx/fontenum.h>
#include <wx/fontpicker.h>
#include <wx/spinctrl.h>
#ifdef __WXGTK__
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include "pixel_font_rendering.h"
#endif

namespace {
constexpr int ID_OPEN = wxID_HIGHEST + 1;
constexpr int ID_SAVE = wxID_HIGHEST + 2;
constexpr int ID_SAVE_AS = wxID_HIGHEST + 3;
constexpr int ID_UNDO = wxID_HIGHEST + 4;
constexpr int ID_REDO = wxID_HIGHEST + 5;
constexpr int ID_FIND = wxID_HIGHEST + 6;
constexpr int ID_FIND_NEXT = wxID_HIGHEST + 7;
constexpr int ID_FIND_PREVIOUS = wxID_HIGHEST + 8;
constexpr int ID_EDITOR_SETTINGS = wxID_HIGHEST + 9;
}

class EditorFrame final : public wxFrame {
public:
    EditorFrame()
        : wxFrame(nullptr, wxID_ANY, _("SingleCode Editor"), wxDefaultPosition,
                  wxSize(960, 680)) {
        auto* fileMenu = new wxMenu;
        fileMenu->Append(ID_OPEN, _("Open...") + wxS("\tCtrl+O"));
        fileMenu->Append(ID_SAVE, _("Save") + wxS("\tCtrl+S"));
        fileMenu->Append(ID_SAVE_AS, _("Save As...") + wxS("\tCtrl+Shift+S"));
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, _("Exit"));

        auto* menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, _("File"));

        auto* editMenu = new wxMenu;
        editMenu->Append(ID_UNDO, _("Undo") + wxS("\tCtrl+Z"));
        editMenu->Append(ID_REDO, _("Redo") + wxS("\tCtrl+Y"));
        editMenu->AppendSeparator();
        editMenu->Append(ID_FIND, _("Find...") + wxS("\tCtrl+F"));
        editMenu->Append(ID_FIND_NEXT, _("Find Next") + wxS("\tF3"));
        editMenu->Append(ID_FIND_PREVIOUS, _("Find Previous") + wxS("\tShift+F3"));
        menuBar->Append(editMenu, _("Edit"));

        auto* settingsMenu = new wxMenu;
        settingsMenu->Append(ID_EDITOR_SETTINGS, _("Editor Settings..."));
        menuBar->Append(settingsMenu, _("Settings"));
        SetMenuBar(menuBar);

        editor_ = new wxStyledTextCtrl(this, wxID_ANY);
        editor_->SetTabWidth(4);
        editor_->SetIndent(4);
        editor_->SetUseTabs(false);
        editorFont_ = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        LoadEditorSettings();
        ApplySystemAppearance();

        CreateStatusBar(2);
        const int statusWidths[] = {-1, FromDIP(170)};
        SetStatusWidths(2, statusWidths);
        SetStatusText(_("Ready"), 0);
        UpdateCursorPosition();

        Bind(wxEVT_MENU, &EditorFrame::OnOpen, this, ID_OPEN);
        Bind(wxEVT_MENU, &EditorFrame::OnSave, this, ID_SAVE);
        Bind(wxEVT_MENU, &EditorFrame::OnSaveAs, this, ID_SAVE_AS);
        Bind(wxEVT_MENU, &EditorFrame::OnUndo, this, ID_UNDO);
        Bind(wxEVT_MENU, &EditorFrame::OnRedo, this, ID_REDO);
        Bind(wxEVT_MENU, &EditorFrame::OnFind, this, ID_FIND);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { FindText(true); }, ID_FIND_NEXT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { FindText(false); }, ID_FIND_PREVIOUS);
        Bind(wxEVT_MENU, &EditorFrame::OnEditorSettings, this, ID_EDITOR_SETTINGS);
        Bind(wxEVT_FIND, &EditorFrame::OnFindDialogSearch, this);
        Bind(wxEVT_FIND_NEXT, &EditorFrame::OnFindDialogSearch, this);
        Bind(wxEVT_FIND_CLOSE, &EditorFrame::OnFindDialogClose, this);
        Bind(wxEVT_UPDATE_UI, &EditorFrame::OnUpdateUndo, this, ID_UNDO);
        Bind(wxEVT_UPDATE_UI, &EditorFrame::OnUpdateRedo, this, ID_REDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorFrame::OnSavePointChanged, this,
             editor_->GetId());
        Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorFrame::OnSavePointChanged, this,
             editor_->GetId());
        Bind(wxEVT_STC_MODIFIED, &EditorFrame::OnEditorModified, this,
             editor_->GetId());
        Bind(wxEVT_STC_UPDATEUI, [this](wxStyledTextEvent& event) {
            UpdateCursorPosition();
            event.Skip();
        }, editor_->GetId());
        Bind(wxEVT_STC_ZOOM, [this](wxStyledTextEvent& event) {
            CallAfter(&EditorFrame::UpdateLineNumberWidth);
            event.Skip();
        }, editor_->GetId());
        editor_->Bind(wxEVT_KEY_DOWN, &EditorFrame::OnEditorKeyDown, this);
        editor_->Bind(wxEVT_CHAR, &EditorFrame::OnEditorChar, this);
        editor_->Bind(wxEVT_MOUSEWHEEL, &EditorFrame::OnEditorMouseWheel, this);
        Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
            ApplySystemAppearance();
            event.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, &EditorFrame::OnClose, this);

        UpdateTitle();
    }

private:
    wxStyledTextCtrl* editor_ = nullptr;
    wxFont editorFont_;
    int extraLineSpacing_ = 0;
    bool pixelFontRendering_ = false;
    wxString filePath_;
    bool loading_ = false;
    bool previousEditWasWhitespace_ = false;
    int wheelZoomRemainder_ = 0;
    wxFindReplaceData findData_;
    wxFindReplaceDialog* findDialog_ = nullptr;

    void ApplySystemAppearance() {
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

    void LoadEditorSettings() {
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

    void SaveEditorSettings() {
        wxConfig config(wxS("SingleCode"));
        config.Write(wxS("/Editor/Font"), editorFont_.GetNativeFontInfoDesc());
        config.Write(wxS("/Editor/FontFace"), editorFont_.GetFaceName());
        config.Write(wxS("/Editor/FontSize"), EffectiveFontSize());
        config.Write(wxS("/Editor/LineSpacing"), extraLineSpacing_);
        config.Write(wxS("/Editor/PixelFontRendering"), pixelFontRendering_);
        config.Flush();
    }

    int EffectiveFontSize() const {
        return static_cast<int>(wxClip(
            editorFont_.GetPointSize() + editor_->GetZoom(), 6, 72));
    }

    void SetEffectiveFontSize(int pointSize) {
        const int clampedSize = static_cast<int>(wxClip(pointSize, 6, 72));
        editor_->SetZoom(clampedSize - editorFont_.GetPointSize());
    }

    static wxChar ClosingCharacter(wxChar opening) {
        switch (opening) {
        case wxS('('): return wxS(')');
        case wxS('['): return wxS(']');
        case wxS('{'): return wxS('}');
        case wxS('<'): return wxS('>');
        case wxS('\"'): return wxS('\"');
        case wxS('\''): return wxS('\'');
        default: return 0;
        }
    }

    static bool IsMatchingPair(wxChar opening, wxChar closing) {
        return ClosingCharacter(opening) == closing && closing != 0;
    }

    static bool IsClosingCharacter(wxChar character) {
        return character == wxS(')') || character == wxS(']') ||
               character == wxS('}') || character == wxS('>') ||
               character == wxS('\"') ||
               character == wxS('\'');
    }

    bool InsertBraceNewline() {
        if (editor_->GetSelectionStart() != editor_->GetSelectionEnd()) {
            return false;
        }
        const int position = editor_->GetCurrentPos();
        if (position <= 0 || position >= editor_->GetTextLength() ||
            editor_->GetCharAt(position - 1) != wxS('{') ||
            editor_->GetCharAt(position) != wxS('}')) {
            return false;
        }

        const int line = editor_->LineFromPosition(position);
        const int lineStart = editor_->PositionFromLine(line);
        const wxString beforeCaret = editor_->GetTextRange(lineStart, position);
        wxString baseIndent;
        while (baseIndent.length() < beforeCaret.length()) {
            const wxChar character = beforeCaret[baseIndent.length()];
            if (character != wxS(' ') && character != wxS('\t')) {
                break;
            }
            baseIndent += character;
        }

        wxString innerIndent = baseIndent;
        if (editor_->GetUseTabs()) {
            innerIndent += wxS('\t');
        } else {
            const int indentWidth = editor_->GetIndent() > 0
                ? editor_->GetIndent()
                : 4;
            innerIndent.append(static_cast<size_t>(indentWidth), wxS(' '));
        }

        wxString newline = wxS("\n");
        if (editor_->GetEOLMode() == wxSTC_EOL_CRLF) {
            newline = wxS("\r\n");
        } else if (editor_->GetEOLMode() == wxSTC_EOL_CR) {
            newline = wxS("\r");
        }

        const wxString insertion = newline + innerIndent + newline + baseIndent;
        previousEditWasWhitespace_ = false;
        editor_->BeginUndoAction();
        editor_->ReplaceSelection(insertion);
        const int caret = position + static_cast<int>(newline.length() +
                                                       innerIndent.length());
        editor_->SetSelection(caret, caret);
        editor_->EndUndoAction();
        return true;
    }

    void OnEditorKeyDown(wxKeyEvent& event) {
        if (event.ControlDown() && !event.AltDown() && !event.MetaDown()) {
            const int key = event.GetKeyCode();
            if (key == WXK_ADD || key == WXK_NUMPAD_ADD || key == '+' ||
                (key == '=' && event.ShiftDown())) {
                SetEffectiveFontSize(EffectiveFontSize() + 1);
                return;
            }
            if (key == WXK_SUBTRACT || key == WXK_NUMPAD_SUBTRACT ||
                key == '-') {
                SetEffectiveFontSize(EffectiveFontSize() - 1);
                return;
            }
        }

        if (!event.ControlDown() && !event.AltDown() && !event.MetaDown() &&
            (event.GetKeyCode() == WXK_RETURN ||
             event.GetKeyCode() == WXK_NUMPAD_ENTER) && InsertBraceNewline()) {
            return;
        }

        if (!event.ControlDown() && !event.AltDown() && !event.MetaDown() &&
            event.GetKeyCode() == WXK_BACK) {
            const int selectionStart = editor_->GetSelectionStart();
            const int selectionEnd = editor_->GetSelectionEnd();

            if (selectionStart == selectionEnd && selectionStart > 0 &&
                selectionStart < editor_->GetTextLength() &&
                IsMatchingPair(
                    static_cast<wxChar>(editor_->GetCharAt(selectionStart - 1)),
                    static_cast<wxChar>(editor_->GetCharAt(selectionStart)))) {
                previousEditWasWhitespace_ = false;
                editor_->BeginUndoAction();
                editor_->SetSelection(selectionStart - 1, selectionStart + 1);
                editor_->ReplaceSelection(wxEmptyString);
                editor_->EndUndoAction();
                return;
            }
        }
        event.Skip();
    }

    void OnEditorChar(wxKeyEvent& event) {
        if (event.ControlDown() || event.AltDown() || event.MetaDown()) {
            event.Skip();
            return;
        }

        const int unicodeKey = event.GetUnicodeKey();
        const wxChar typed = static_cast<wxChar>(
            unicodeKey == WXK_NONE ? event.GetKeyCode() : unicodeKey);
        const int selectionStart = editor_->GetSelectionStart();
        const int selectionEnd = editor_->GetSelectionEnd();

        if (selectionStart == selectionEnd && IsClosingCharacter(typed) &&
            editor_->GetCharAt(selectionStart) == typed) {
            editor_->SetSelection(selectionStart + 1, selectionStart + 1);
            return;
        }

        const wxChar closing = ClosingCharacter(typed);
        if (closing == 0) {
            event.Skip();
            return;
        }

        previousEditWasWhitespace_ = false;
        editor_->BeginUndoAction();
        if (selectionStart != selectionEnd) {
            editor_->InsertText(selectionEnd, wxString(closing));
            editor_->InsertText(selectionStart, wxString(typed));
            editor_->SetSelection(selectionStart + 1, selectionEnd + 1);
        } else {
            editor_->InsertText(selectionStart,
                                wxString(typed) + wxString(closing));
            editor_->SetSelection(selectionStart + 1, selectionStart + 1);
        }
        editor_->EndUndoAction();
    }

    void OnEditorMouseWheel(wxMouseEvent& event) {
        if (event.ControlDown() && !event.AltDown() && !event.MetaDown()) {
            const int delta = event.GetWheelDelta();
            if (delta > 0) {
                wheelZoomRemainder_ += event.GetWheelRotation();
                const int steps = wheelZoomRemainder_ / delta;
                wheelZoomRemainder_ %= delta;
                if (steps != 0) {
                    SetEffectiveFontSize(EffectiveFontSize() + steps);
                }
            }
            return;
        }
        event.Skip();
    }

#ifdef __WXGTK__
    void ApplyPixelFontRendering() {
        auto* context = gtk_widget_get_pango_context(
            GTK_WIDGET(editor_->GetHandle()));
        auto* fontMap = PANGO_FC_FONT_MAP(
            pango_context_get_font_map(context));
        SetPixelFontRendering(fontMap, editorFont_.GetFaceName(), pixelFontRendering_);
    }
#endif

    void OnSavePointChanged(wxStyledTextEvent&) {
        if (!loading_) {
            // Scintilla sends save-point notifications during the transition.
            // Read the final state after the notification has completed.
            CallAfter(&EditorFrame::UpdateTitle);
        }
    }

    void OnEditorModified(wxStyledTextEvent& event) {
        const int type = event.GetModificationType();
        if (type & (wxSTC_PERFORMED_USER | wxSTC_PERFORMED_UNDO |
                    wxSTC_PERFORMED_REDO)) {
            UpdateLineNumberWidth();
        }
        if (type & (wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO)) {
            previousEditWasWhitespace_ = false;
        } else if ((type & wxSTC_PERFORMED_USER) &&
                   (type & (wxSTC_MOD_BEFOREINSERT | wxSTC_MOD_BEFOREDELETE))) {
            const wxString text = (type & wxSTC_MOD_BEFOREDELETE)
                ? editor_->GetTextRange(event.GetPosition(),
                                        event.GetPosition() + event.GetLength())
                : event.GetText();
            const bool whitespace = text.length() == 1 && wxIsspace(text[0]);
            if (whitespace || previousEditWasWhitespace_) {
                // An empty undo action ends Scintilla's automatic coalescing.
                editor_->BeginUndoAction();
                editor_->EndUndoAction();
            }
            previousEditWasWhitespace_ = whitespace;
        }
        event.Skip();
    }

    void UpdateLineNumberWidth() {
        int digits = 1;
        for (int lines = editor_->GetLineCount(); lines >= 10; lines /= 10) {
            ++digits;
        }
        wxString sample;
        for (int i = 0; i < digits; ++i) {
            sample += wxS('9');
        }
        editor_->SetMarginWidth(
            0, editor_->TextWidth(wxSTC_STYLE_LINENUMBER, sample) + FromDIP(8));
    }

    void UpdateCursorPosition() {
        SetStatusText(wxString::Format(
            _("Line %d, Column %d"), editor_->GetCurrentLine() + 1,
            editor_->GetColumn(editor_->GetCurrentPos()) + 1), 1);
    }

    void UpdateTitle() {
        const wxString name = filePath_.empty()
                                  ? _("Untitled")
                                  : wxFileName(filePath_).GetFullName();
        SetTitle(wxString::Format(_("%s%s - SingleCode Editor"),
                                  editor_->GetModify() ? "*" : "", name));
    }

    bool ConfirmSaveChanges() {
        if (!editor_->GetModify()) {
            return true;
        }

        const int answer = wxMessageBox(
            _("This document has unsaved changes. Save them first?"),
            _("Unsaved Changes"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
        if (answer == wxCANCEL || answer == wxID_CANCEL) {
            return false;
        }
        if (answer == wxYES || answer == wxID_YES) {
            return SaveCurrentFile();
        }
        return true;
    }

    void OnOpen(wxCommandEvent&) {
        if (!ConfirmSaveChanges()) {
            return;
        }

        wxFileDialog dialog(this, _("Open Text File"), wxEmptyString,
                            wxEmptyString, _("All files (*.*)|*.*"),
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }

        const wxString selectedPath = dialog.GetPath();
        loading_ = true;
        const bool loaded = editor_->LoadFile(selectedPath);
        loading_ = false;
        if (!loaded) {
            wxMessageBox(_("Could not read the selected file."), _("Open Failed"),
                         wxOK | wxICON_ERROR, this);
            return;
        }

        filePath_ = selectedPath;
        editor_->EmptyUndoBuffer();
        editor_->SetSavePoint();
        editor_->SetInsertionPoint(0);
        UpdateLineNumberWidth();
        UpdateTitle();
        SetStatusText(wxString::Format(_("Loaded %s"), filePath_), 0);
    }

    void OnSave(wxCommandEvent&) { SaveCurrentFile(); }

    void OnSaveAs(wxCommandEvent&) { SaveFileAs(); }

    void OnUndo(wxCommandEvent&) {
        if (editor_->CanUndo()) {
            editor_->Undo();
        }
    }

    void OnRedo(wxCommandEvent&) {
        if (editor_->CanRedo()) {
            editor_->Redo();
        }
    }

    void OnUpdateUndo(wxUpdateUIEvent& event) {
        event.Enable(editor_->CanUndo());
    }

    void OnUpdateRedo(wxUpdateUIEvent& event) {
        event.Enable(editor_->CanRedo());
    }

    void OnFind(wxCommandEvent&) {
        if (findDialog_ == nullptr) {
            findDialog_ = new wxFindReplaceDialog(
                this, &findData_, _("Find"), wxFR_NOWHOLEWORD);
        }
        findDialog_->Show();
        findDialog_->Raise();
    }

    void OnEditorSettings(wxCommandEvent&) {
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
        const auto setPreviewFont = [preview](const wxFont& font) {
            wxFont previewFont = font;
            previewFont.SetPointSize(static_cast<int>(wxClip(
                font.GetPointSize(), 12, 24)));
            preview->SetFont(previewFont);
            preview->Refresh();
        };
        setPreviewFont(settingsFont);
        fontPicker->Bind(wxEVT_FONTPICKER_CHANGED,
                         [setPreviewFont](wxFontPickerEvent& event) {
                             setPreviewFont(event.GetFont());
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

    void OnFindDialogSearch(wxFindDialogEvent& event) {
        findData_.SetFindString(event.GetFindString());
        FindText((event.GetFlags() & wxFR_DOWN) != 0, event.GetFlags());
    }

    void OnFindDialogClose(wxFindDialogEvent& event) {
        if (findDialog_ != nullptr) {
            findDialog_->Destroy();
            findDialog_ = nullptr;
        }
        editor_->SetFocus();
        event.Skip();
    }

    void FindText(bool forward, int flags = -1) {
        const wxString needle = findData_.GetFindString();
        if (needle.empty()) {
            return;
        }
        if (flags < 0) {
            flags = findData_.GetFlags();
        }
        editor_->SetSearchFlags(
            ((flags & wxFR_MATCHCASE) ? wxSTC_FIND_MATCHCASE : 0) |
            ((flags & wxFR_WHOLEWORD) ? wxSTC_FIND_WHOLEWORD : 0));

        const int length = editor_->GetLength();
        const int selectionStart = editor_->GetSelectionStart();
        const int selectionEnd = editor_->GetSelectionEnd();
        const int start = forward
            ? (selectionStart == selectionEnd ? editor_->GetCurrentPos() : selectionEnd)
            : (selectionStart == selectionEnd ? editor_->GetCurrentPos() : selectionStart);

        const bool mustWrap = forward ? start >= length : start <= 0;
        editor_->SetTargetRange(mustWrap ? (forward ? 0 : length) : start,
                                forward ? length : 0);
        int match = editor_->SearchInTarget(needle);
        if (match == wxSTC_INVALID_POSITION && !mustWrap) {
            editor_->SetTargetRange(forward ? 0 : length,
                                    forward ? length : 0);
            match = editor_->SearchInTarget(needle);
        }

        if (match == wxSTC_INVALID_POSITION) {
            SetStatusText(_("Text not found"), 0);
            return;
        }
        editor_->SetSelection(editor_->GetTargetStart(), editor_->GetTargetEnd());
        editor_->SetFocus();
        SetStatusText(wxString::Format(_("Found: %s"), needle), 0);
    }

    bool SaveCurrentFile() {
        if (filePath_.empty()) {
            return SaveFileAs();
        }
        return WriteFile(filePath_);
    }

    bool SaveFileAs() {
        wxFileDialog dialog(this, _("Save As"), wxEmptyString, wxEmptyString,
                            _("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) {
            return false;
        }
        wxString path = dialog.GetPath();
        if (dialog.GetFilterIndex() == 0 && !wxFileName(path).HasExt()) {
            path += wxS(".txt");
            if (wxFileName::FileExists(path) &&
                wxMessageBox(wxString::Format(_("The file %s already exists. Replace it?"),
                                              wxFileName(path).GetFullName()),
                             _("Confirm Overwrite"),
                             wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) {
                return false;
            }
        }
        return WriteFile(path);
    }

    bool WriteFile(const wxString& path) {
        if (!editor_->SaveFile(path)) {
            wxMessageBox(_("Could not save to the selected location."),
                         _("Save Failed"), wxOK | wxICON_ERROR, this);
            return false;
        }

        filePath_ = path;
        editor_->SetSavePoint();
        UpdateTitle();
        SetStatusText(wxString::Format(_("Saved %s"), filePath_), 0);
        return true;
    }

    void OnClose(wxCloseEvent& event) {
        if (ConfirmSaveChanges()) {
            SaveEditorSettings();
            event.Skip();
        } else {
            event.Veto();
        }
    }
};

class EditorApp final : public wxApp {
public:
    bool OnInit() override {
        const wxString executableDir =
            wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
        wxLocale::AddCatalogLookupPathPrefix(executableDir + wxS("/locale"));
        wxLocale::AddCatalogLookupPathPrefix(executableDir + wxS("/../share/locale"));

        locale_.Init(wxLANGUAGE_DEFAULT);
        locale_.AddCatalog(wxS("singlecode"));

        auto* frame = new EditorFrame;
        frame->Show();
        return true;
    }

private:
    wxLocale locale_;
};

wxIMPLEMENT_APP(EditorApp);
