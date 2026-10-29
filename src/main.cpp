#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>

namespace {
constexpr int ID_OPEN = wxID_HIGHEST + 1;
constexpr int ID_SAVE = wxID_HIGHEST + 2;
constexpr int ID_SAVE_AS = wxID_HIGHEST + 3;
constexpr int ID_UNDO = wxID_HIGHEST + 4;
constexpr int ID_REDO = wxID_HIGHEST + 5;
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
        Bind(wxEVT_UPDATE_UI, &EditorFrame::OnUpdateUndo, this, ID_UNDO);
        Bind(wxEVT_UPDATE_UI, &EditorFrame::OnUpdateRedo, this, ID_REDO);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorFrame::OnSavePointChanged, this,
             editor_->GetId());
        Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorFrame::OnSavePointChanged, this,
             editor_->GetId());
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

    void ApplySystemAppearance() {
        const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        editor_->StyleSetFont(wxSTC_STYLE_DEFAULT,
                             wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        editor_->StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
        editor_->StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
        editor_->StyleClearAll();
        editor_->SetBackgroundColour(background);
        editor_->SetCaretForeground(foreground);
        editor_->SetSelBackground(true,
            wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
        editor_->SetSelForeground(true,
            wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        for (int margin = 0; margin < editor_->GetMarginCount(); ++margin) {
            editor_->SetMarginWidth(margin, 0);
        }
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
        return WriteFile(dialog.GetPath());
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
