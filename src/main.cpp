#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>

namespace {
constexpr int ID_OPEN = wxID_HIGHEST + 1;
constexpr int ID_SAVE = wxID_HIGHEST + 2;
constexpr int ID_SAVE_AS = wxID_HIGHEST + 3;
}

class EditorFrame final : public wxFrame {
public:
    EditorFrame()
        : wxFrame(nullptr, wxID_ANY, _("SingleCode Editor"), wxDefaultPosition,
                  wxSize(960, 680)) {
        auto* fileMenu = new wxMenu;
        fileMenu->Append(ID_OPEN, _("Open...\tCtrl+O"));
        fileMenu->Append(ID_SAVE, _("Save\tCtrl+S"));
        fileMenu->Append(ID_SAVE_AS, _("Save As...\tCtrl+Shift+S"));
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, _("Exit\tAlt+F4"));

        auto* menuBar = new wxMenuBar;
        menuBar->Append(fileMenu, _("File"));
        SetMenuBar(menuBar);

        editor_ = new wxTextCtrl(
            this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_RICH2 | wxTE_DONTWRAP | wxHSCROLL);
        editor_->SetFont(wxFontInfo(12));

        CreateStatusBar();
        SetStatusText(_("Ready"));

        Bind(wxEVT_MENU, &EditorFrame::OnOpen, this, ID_OPEN);
        Bind(wxEVT_MENU, &EditorFrame::OnSave, this, ID_SAVE);
        Bind(wxEVT_MENU, &EditorFrame::OnSaveAs, this, ID_SAVE_AS);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
        Bind(wxEVT_TEXT, &EditorFrame::OnTextChanged, this, editor_->GetId());
        Bind(wxEVT_CLOSE_WINDOW, &EditorFrame::OnClose, this);

        UpdateTitle();
    }

private:
    wxTextCtrl* editor_ = nullptr;
    wxString filePath_;
    bool modified_ = false;
    bool loading_ = false;

    void OnTextChanged(wxCommandEvent&) {
        if (!loading_) {
            modified_ = true;
            UpdateTitle();
        }
    }

    void UpdateTitle() {
        const wxString name = filePath_.empty()
                                  ? _("Untitled")
                                  : wxFileName(filePath_).GetFullName();
        SetTitle(wxString::Format(_("%s%s - SingleCode Editor"),
                                  modified_ ? "*" : "", name));
    }

    bool ConfirmSaveChanges() {
        if (!modified_) {
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
        modified_ = false;
        editor_->SetInsertionPoint(0);
        UpdateTitle();
        SetStatusText(wxString::Format(_("Loaded %s"), filePath_));
    }

    void OnSave(wxCommandEvent&) { SaveCurrentFile(); }

    void OnSaveAs(wxCommandEvent&) { SaveFileAs(); }

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
        modified_ = false;
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
