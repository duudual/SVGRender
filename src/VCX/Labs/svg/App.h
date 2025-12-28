#pragma once

#include <vector>

#include "Engine/app.h"
#include "Labs/svg/CaseSVGRender.h"
#include "Labs/Common/UI.h"

namespace VCX::Labs::SVG {
    class App : public VCX::Engine::IApp {
    private:
        Common::UI _ui;

        CaseSVGRender  _caseSVGRender;

        std::size_t _caseId = 0;

        std::vector<std::reference_wrapper<Common::ICase>> _cases = {
            _caseSVGRender,
        };

    public:
        App();

        void OnFrame() override;
    };
}