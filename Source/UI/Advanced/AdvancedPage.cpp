#include "UI/Advanced/AdvancedPage.h"

namespace ui
{
using namespace fmlume;

// ============================================================================== AdvancedPage
AdvancedPage::AdvancedPage (Ctx& c) : ctx (c), operators (c), algorithm (c), modulation (c), fx (c), rhythm (c), performance (c)
{
    tabs.setTabBarDepth (38);
    tabs.setOutline (0);
    tabs.addTab ("Operators", col::bg, &operators, false);
    tabs.addTab ("Algorithm", col::bg, &algorithm, false);
    tabs.addTab ("Filter & Mod", col::bg, &modulation, false);
    tabs.addTab ("FX", col::bg, &fx, false);
    tabs.addTab ("Rhythm", col::bg, &rhythm, false);
    tabs.addTab ("Performance", col::bg, &performance, false);
    tabs.getTabbedButtonBar().setMinimumTabScaleFactor (0.6);
    addAndMakeVisible (tabs);
}

void AdvancedPage::resized()
{
    tabs.setBounds (getLocalBounds().withTrimmedLeft (6).withTrimmedRight (6));
}

void AdvancedPage::refresh (const Preset& p)
{
    // Only the visible tab is refreshed; switching tabs refreshes through the editor timer.
    switch (tabs.getCurrentTabIndex())
    {
        case 0: operators.refresh (p); break;
        case 1: algorithm.refresh (p); break;
        case 2: modulation.refresh (p); break;
        case 3: fx.refresh (p); break;
        case 4: rhythm.refresh (p); break;
        default: performance.refresh (p); break;
    }
}
} // namespace ui
