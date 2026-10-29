#include "editor_frame.h"


wxChar EditorFrame::ClosingCharacter(wxChar opening) {
    switch (opening) {
    case wxS('('): return wxS(')');
    case wxS('['): return wxS(']');
    case wxS('{'): return wxS('}');
    case wxS('\"'): return wxS('\"');
    case wxS('\''): return wxS('\'');
    default: return 0;
    }
}

bool EditorFrame::IsMatchingPair(wxChar opening, wxChar closing) {
    return ClosingCharacter(opening) == closing && closing != 0;
}

bool EditorFrame::IsClosingCharacter(wxChar character) {
    return character == wxS(')') || character == wxS(']') ||
           character == wxS('}') ||
           character == wxS('\"') ||
           character == wxS('\'');
}

wxString EditorFrame::IndentationBefore(int position) const {
    const int line = editor_->LineFromPosition(position);
    const int lineStart = editor_->PositionFromLine(line);
    const wxString beforeCaret = editor_->GetTextRange(lineStart, position);
    wxString indent;
    while (indent.length() < beforeCaret.length()) {
        const wxChar character = beforeCaret[indent.length()];
        if (character != wxS(' ') && character != wxS('\t')) {
            break;
        }
        indent += character;
    }
    return indent;
}

wxString EditorFrame::LineEnding() const {
    if (editor_->GetEOLMode() == wxSTC_EOL_CRLF) {
        return wxS("\r\n");
    }
    if (editor_->GetEOLMode() == wxSTC_EOL_CR) {
        return wxS("\r");
    }
    return wxS("\n");
}

bool EditorFrame::InsertIndentedNewline() {
    const wxString indent = IndentationBefore(editor_->GetSelectionStart());
    if (indent.empty()) {
        return false;
    }
    previousEditWasWhitespace_ = false;
    editor_->BeginUndoAction();
    editor_->ReplaceSelection(LineEnding() + indent);
    editor_->EndUndoAction();
    return true;
}

bool EditorFrame::InsertBraceNewline() {
    if (editor_->GetSelectionStart() != editor_->GetSelectionEnd()) {
        return false;
    }
    const int position = editor_->GetCurrentPos();
    if (position <= 0 || position >= editor_->GetTextLength() ||
        editor_->GetCharAt(position - 1) != wxS('{') ||
        editor_->GetCharAt(position) != wxS('}')) {
        return false;
    }

    const wxString baseIndent = IndentationBefore(position);

    wxString innerIndent = baseIndent;
    if (editor_->GetUseTabs()) {
        innerIndent += wxS('\t');
    } else {
        const int indentWidth = editor_->GetIndent() > 0
            ? editor_->GetIndent()
            : 4;
        innerIndent.append(static_cast<size_t>(indentWidth), wxS(' '));
    }

    const wxString newline = LineEnding();

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

void EditorFrame::OnEditorKeyDown(wxKeyEvent& event) {
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
         event.GetKeyCode() == WXK_NUMPAD_ENTER) &&
        (InsertBraceNewline() || InsertIndentedNewline())) {
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

void EditorFrame::OnEditorChar(wxKeyEvent& event) {
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

void EditorFrame::OnEditorMouseWheel(wxMouseEvent& event) {
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

void EditorFrame::OnEditorModified(wxStyledTextEvent& event) {
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

void EditorFrame::OnUndo(wxCommandEvent&) {
    if (editor_->CanUndo()) {
        editor_->Undo();
    }
}

void EditorFrame::OnRedo(wxCommandEvent&) {
    if (editor_->CanRedo()) {
        editor_->Redo();
    }
}

void EditorFrame::OnUpdateUndo(wxUpdateUIEvent& event) {
    event.Enable(editor_->CanUndo());
}

void EditorFrame::OnUpdateRedo(wxUpdateUIEvent& event) {
    event.Enable(editor_->CanRedo());
}
