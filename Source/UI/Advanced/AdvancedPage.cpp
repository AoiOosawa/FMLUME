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
    for (auto* b : { &slotA, &slotB })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4202);
        addChildComponent (b);
    }
    slotA.setTooltip ("Edit morph sound A (the main sound)");
    slotB.setTooltip ("Edit morph sound B. Operators, routing, modulation and macros exist per sound; FX and rhythm are shared.");
    slotA.onClick = [this] { if (slotA.getToggleState()) ctx.proc.withModel ([] (PatchController& pc) { pc.setEditingB (false); return 0; }); };
    slotB.onClick = [this] { if (slotB.getToggleState()) ctx.proc.withModel ([] (PatchController& pc) { pc.setEditingB (true); return 0; }); };
    slotB.setColour (juce::TextButton::buttonOnColourId, col::accentB);
    slotB.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void AdvancedPage::resized()
{
    tabs.setBounds (getLocalBounds().withTrimmedLeft (6).withTrimmedRight (6));
    auto bar = getLocalBounds().removeFromTop (38).reduced (12, 6);
    slotB.setBounds (bar.removeFromRight (66));
    bar.removeFromRight (4);
    slotA.setBounds (bar.removeFromRight (66));
}

void AdvancedPage::refresh (const Preset& p)
{
    const bool b = ctx.sideB();
    // The A / B switch only appears once a second (morph) sound exists.
    slotA.setVisible (p.data.hasB);
    slotB.setVisible (p.data.hasB);
    slotA.setToggleState (! b, juce::dontSendNotification);
    slotB.setToggleState (b, juce::dontSendNotification);
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
