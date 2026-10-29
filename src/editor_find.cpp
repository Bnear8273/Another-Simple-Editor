#include "editor_frame.h"


void EditorFrame::OnFind(wxCommandEvent&) {
    if (findDialog_ == nullptr) {
        findDialog_ = new wxFindReplaceDialog(
            this, &findData_, _("Find"), wxFR_NOWHOLEWORD);
    }
    findDialog_->Show();
    findDialog_->Raise();
}

void EditorFrame::OnFindDialogSearch(wxFindDialogEvent& event) {
    findData_.SetFindString(event.GetFindString());
    FindText((event.GetFlags() & wxFR_DOWN) != 0, event.GetFlags());
}

void EditorFrame::OnFindDialogClose(wxFindDialogEvent& event) {
    if (findDialog_ != nullptr) {
        findDialog_->Destroy();
        findDialog_ = nullptr;
    }
    editor_->SetFocus();
    event.Skip();
}

void EditorFrame::FindText(bool forward, int flags) {
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
