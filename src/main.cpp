#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/fdrepdlg.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>

namespace {
constexpr int ID_OPEN = wxID_HIGHEST + 1;
constexpr int ID_SAVE = wxID_HIGHEST + 2;
constexpr int ID_SAVE_AS = wxID_HIGHEST + 3;
constexpr int ID_UNDO = wxID_HIGHEST + 4;
constexpr int ID_REDO = wxID_HIGHEST + 5;
constexpr int ID_FIND = wxID_HIGHEST + 6;
constexpr int ID_FIND_NEXT = wxID_HIGHEST + 7;
constexpr int ID_FIND_PREVIOUS = wxID_HIGHEST + 8;
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
        SetMenuBar(menuBar);

        editor_ = new wxStyledTextCtrl(this, wxID_ANY);
        ApplySystemAppearance();

        CreateStatusBar();
        SetStatusText(_("Ready"));

        Bind(wxEVT_MENU, &EditorFrame::OnOpen, this, ID_OPEN);
        Bind(wxEVT_MENU, &EditorFrame::OnSave, this, ID_SAVE);
        Bind(wxEVT_MENU, &EditorFrame::OnSaveAs, this, ID_SAVE_AS);
        Bind(wxEVT_MENU, &EditorFrame::OnUndo, this, ID_UNDO);
        Bind(wxEVT_MENU, &EditorFrame::OnRedo, this, ID_REDO);
        Bind(wxEVT_MENU, &EditorFrame::OnFind, this, ID_FIND);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { FindText(true); }, ID_FIND_NEXT);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { FindText(false); }, ID_FIND_PREVIOUS);
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
        Bind(wxEVT_STC_ZOOM, [this](wxStyledTextEvent& event) {
            CallAfter(&EditorFrame::UpdateLineNumberWidth);
            event.Skip();
        }, editor_->GetId());
        Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
            ApplySystemAppearance();
            event.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, &EditorFrame::OnClose, this);

        UpdateTitle();
    }

private:
    wxStyledTextCtrl* editor_ = nullptr;
    wxString filePath_;
    bool loading_ = false;
    bool previousEditWasWhitespace_ = false;
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
        editor_->StyleSetFont(wxSTC_STYLE_DEFAULT,
                             wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
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
        editor_->Refresh();
    }

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
        SetStatusText(wxString::Format(_("Loaded %s"), filePath_));
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
            SetStatusText(_("Text not found"));
            return;
        }
        editor_->SetSelection(editor_->GetTargetStart(), editor_->GetTargetEnd());
        editor_->SetFocus();
        SetStatusText(wxString::Format(_("Found: %s"), needle));
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
        SetStatusText(wxString::Format(_("Saved %s"), filePath_));
        return true;
    }

    void OnClose(wxCloseEvent& event) {
        if (ConfirmSaveChanges()) {
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
