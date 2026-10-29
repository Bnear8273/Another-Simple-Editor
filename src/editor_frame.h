#pragma once

#include <wx/wx.h>
#include <wx/fdrepdlg.h>
#include <wx/stc/stc.h>

class EditorFrame final : public wxFrame {
public:
    EditorFrame();

private:
    wxStyledTextCtrl* editor_ = nullptr;
    wxFont editorFont_;
    wxArrayString fontFamilies_;
    wxArrayString pixelFontFamilies_;
    int extraLineSpacing_ = 0;
    bool pixelFontRendering_ = false;
    wxString filePath_;
    bool loading_ = false;
    bool previousEditWasWhitespace_ = false;
    int wheelZoomRemainder_ = 0;
    wxFindReplaceData findData_;
    wxFindReplaceDialog* findDialog_ = nullptr;

    void ApplySystemAppearance();
    void LoadEditorSettings();
    void SaveEditorSettings();
    int EffectiveFontSize() const;
    void SetEffectiveFontSize(int pointSize);
    static wxChar ClosingCharacter(wxChar opening);
    static bool IsMatchingPair(wxChar opening, wxChar closing);
    static bool IsClosingCharacter(wxChar character);
    wxString IndentationBefore(int position) const;
    wxString LineEnding() const;
    bool InsertIndentedNewline();
    bool InsertBraceNewline();
    void OnEditorKeyDown(wxKeyEvent& event);
    void OnEditorChar(wxKeyEvent& event);
    void OnEditorMouseWheel(wxMouseEvent& event);
#ifdef __WXGTK__
    void ApplyPixelFontRendering();
#endif
    void OnSavePointChanged(wxStyledTextEvent& event);
    void OnEditorModified(wxStyledTextEvent& event);
    void UpdateLineNumberWidth();
    void UpdateCursorPosition();
    void UpdateTitle();
    bool ConfirmSaveChanges();
    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnSaveAs(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);
    void OnFind(wxCommandEvent& event);
    void OnEditorSettings(wxCommandEvent& event);
    void OnFindDialogSearch(wxFindDialogEvent& event);
    void OnFindDialogClose(wxFindDialogEvent& event);
    void FindText(bool forward, int flags = -1);
    bool SaveCurrentFile();
    bool SaveFileAs();
    bool WriteFile(const wxString& path);
    void OnClose(wxCloseEvent& event);
};
