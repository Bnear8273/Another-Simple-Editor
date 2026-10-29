#include "editor_frame.h"

#include <wx/filename.h>
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
constexpr int ID_EDITOR_SETTINGS = wxID_HIGHEST + 9;
}


EditorFrame::EditorFrame()
    : wxFrame(nullptr, wxID_ANY, _("Another Simple Editor"), wxDefaultPosition,
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

void EditorFrame::OnSavePointChanged(wxStyledTextEvent&) {
    if (!loading_) {
        // Scintilla sends save-point notifications during the transition.
        // Read the final state after the notification has completed.
        CallAfter(&EditorFrame::UpdateTitle);
    }
}

void EditorFrame::UpdateLineNumberWidth() {
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

void EditorFrame::UpdateCursorPosition() {
    SetStatusText(wxString::Format(
        _("Line %d, Column %d"), editor_->GetCurrentLine() + 1,
        editor_->GetColumn(editor_->GetCurrentPos()) + 1), 1);
}

void EditorFrame::UpdateTitle() {
    const wxString name = filePath_.empty()
                              ? _("Untitled")
                              : wxFileName(filePath_).GetFullName();
    SetTitle((editor_->GetModify() ? wxS("*") : wxString()) + name);
}

void EditorFrame::OnClose(wxCloseEvent& event) {
    if (ConfirmSaveChanges()) {
        SaveEditorSettings();
        event.Skip();
    } else {
        event.Veto();
    }
}
