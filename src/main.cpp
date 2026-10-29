#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "editor_frame.h"

class EditorApp final : public wxApp {
public:
    bool OnInit() override {
        const wxString executableDir =
            wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
        wxLocale::AddCatalogLookupPathPrefix(executableDir + wxS("/locale"));
        wxLocale::AddCatalogLookupPathPrefix(executableDir + wxS("/../share/locale"));

        locale_.Init(wxLANGUAGE_DEFAULT);
        locale_.AddCatalog(wxS("another-simple-editor"));

        auto* frame = new EditorFrame;
        frame->Show();
        return true;
    }

private:
    wxLocale locale_;
};

wxIMPLEMENT_APP(EditorApp);
