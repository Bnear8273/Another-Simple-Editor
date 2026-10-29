#include "editor_frame.h"

#include <wx/filedlg.h>
#include <wx/filename.h>

bool EditorFrame::ConfirmSaveChanges() {
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

void EditorFrame::OnOpen(wxCommandEvent&) {
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

void EditorFrame::OnSave(wxCommandEvent&) { SaveCurrentFile(); }

void EditorFrame::OnSaveAs(wxCommandEvent&) { SaveFileAs(); }

bool EditorFrame::SaveCurrentFile() {
    if (filePath_.empty()) {
        return SaveFileAs();
    }
    return WriteFile(filePath_);
}

bool EditorFrame::SaveFileAs() {
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

bool EditorFrame::WriteFile(const wxString& path) {
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
