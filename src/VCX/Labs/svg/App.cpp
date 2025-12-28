#include "Labs/svg/App.h"

namespace VCX::Labs::SVG {
    App::App():
        _ui(
            Labs::Common::UIOptions { .SideWindowWidth = 450.0f }) {
    }

    void App::OnFrame() {
        _ui.Setup(_cases, _caseId);
    }
}
