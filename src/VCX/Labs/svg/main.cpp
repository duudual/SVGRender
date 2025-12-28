#include "Assets/bundled.h"
#include "Labs/svg/App.h"

int main() {
    using namespace VCX;
    return Engine::RunApp<Labs::SVG::App>(Engine::AppContextOptions {
        .Title      = "SVG Render",
        .WindowSize = { 1024, 768 },
        .FontSize   = 16,

        .IconFileNames = Assets::DefaultIcons,
        .FontFileNames = Assets::DefaultFonts,
    });
}
