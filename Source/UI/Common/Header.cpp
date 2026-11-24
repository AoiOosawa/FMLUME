#include "UI/Common/Header.h"

namespace ui
{
using namespace fmlume;

// ============================================================================== Header
Header::Header (Ctx& c) : ctx (c)
{
    for (auto* b : { &prev, &next, &save, &simpleBtn, &advancedBtn }) addAndMakeVisible (b);
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save the current sound as a user preset");
    simpleBtn.setTooltip ("Simple view: macros, effects and variations");
    advancedBtn.setTooltip ("Advanced view: operators and routing, filter and modulation, effects, rhythm, performance");
    simpleBtn.setClickingTogglesState (true);
    advancedBtn.setClickingTogglesState (true);
    simpleBtn.setRadioGroupId (4203);
    advancedBtn.setRadioGroupId (4203);
    simpleBtn.setConnectedEdges (juce::Button::ConnectedOnRight);
    advancedBtn.setConnectedEdges (juce::Button::ConnectedOnLeft);
    simpleBtn.setToggleState (true, juce::dontSendNotification);
    prev.onClick = [this] { ctx.proc.stepPreset (-1); };
    next.onClick = [this] { ctx.proc.stepPreset (1); };
    save.onClick = [this] { if (onSave) onSave(); };

    out.setSliderStyle (juce::Slider::LinearHorizontal);
    out.getProperties().set ("bipolar", true); // fill from the 0 dB centre
    // The header is built before the editor installs its look-and-feel, so the value box colours are set here.
    out.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    out.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    out.setColour (juce::Slider::textBoxTextColourId, col::text);
    out.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 20); // editable value, as in POLYBRISK
    out.setNumDecimalPlacesToDisplay (1);
    if (auto* p = ctx.proc.apvts.getParameter ("master.gainDb"))
    {
        outAttachment = std::make_unique<juce::SliderParameterAttachment> (*p, out);
        out.onReset = [p] { setHostParam (*p, p->getDefaultValue()); };
        // As in POLYBRISK: a double-click returns the output level to 0 dB.
        out.onDoubleClick = [p] { setHostParam (*p, p->convertTo0to1 (0.0f)); };
    }
    out.textFromValueFunction = [] (double v) { return juce::String (std::abs (v) < 0.05 ? 0.0 : v, 1) + " dB"; }; // never "-0.0"
    out.updateText();
    out.setTooltip ("Output level (Master Gain, host automatable). Double-click the slider = 0 dB, click the number to type a value. The line below shows the output meter.");
    addAndMakeVisible (out);
}

void Header::paint (juce::Graphics& g)
{
    g.setColour (col::panel);
    g.fillRect (getLocalBounds());
    g.setColour (col::outline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // logo mark + wordmark in the hero gradient
    auto logo = logoArea;
    drawLogoMark (g, logo.removeFromLeft (logo.getHeight()).toFloat());
    logo.removeFromLeft (8);
    if (logo.getWidth() > 60)
    {
        g.setGradientFill (juce::ColourGradient (col::accent, (float) logo.getX(), 0.0f, col::violet.interpolatedWith (col::hot, 0.25f), (float) logo.getX() + 104.0f, 0.0f, false));
        g.setFont (font (23.0f, true));
        g.drawText ("FMLUME", logo, juce::Justification::centredLeft);
        if (FmlumeAudioProcessor::kDevBuild)
        {
            // marks the development plug-in so it is never mistaken for the release
            const juce::Rectangle<float> tag ((float) logo.getX() + 106.0f, (float) logo.getCentreY() - 8.0f, 32.0f, 16.0f);
            g.setColour (col::accentB);
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (font (10.5f, true));
            g.drawText ("DEV", tag, juce::Justification::centred);
        }
    }

    // Preset name: a button that opens the browser
    g.setColour (overName || browserOpen ? col::panelHi : col::panel.brighter (0.04f));
    g.fillRoundedRectangle (nameArea.toFloat(), 5.0f);
    g.setColour (browserOpen ? col::accent : col::outline);
    g.drawRoundedRectangle (nameArea.toFloat().reduced (0.5f), 5.0f, 1.0f);
    g.setColour (col::text);
    g.setFont (font (15.0f, true));
    g.drawText (presetName + (dirty ? " *" : ""), nameArea.reduced (26, 0), juce::Justification::centred, true);
    // disclosure chevron
    juce::Path chev;
    const float cx = (float) nameArea.getRight() - 16.0f, cy = (float) nameArea.getCentreY();
    chev.startNewSubPath (cx - 4.5f, cy - 2.0f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (col::textDim);
    g.strokePath (chev, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::textDim);
    g.setFont (font (12.0f, true));
    g.drawText ("OUT", outLabelArea, juce::Justification::centredRight);

    // Slim stereo output meter under the slider (dB scale -48 .. 0).
    const float top = juce::jmax (peak[0], peak[1]);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto bar = meterArea.withHeight (2).translated (0, ch * 3).toFloat();
        g.setColour (col::bg);
        g.fillRect (bar);
        const float db = juce::Decibels::gainToDecibels (peak[ch], -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        g.setColour (db > -1.0f ? col::bad : (db > -9.0f ? col::warm : col::good));
        g.fillRect (bar.withWidth (bar.getWidth() * frac));
    }
    if (top > 0.95f)
    {
        g.setColour (col::bad);
        g.setFont (font (9.5f, true));
        g.drawText ("LIMIT", outLabelArea.translated (0, 13).withHeight (10), juce::Justification::centredRight);
    }
}

void Header::resized()
{
    auto r = getLocalBounds().reduced (12, 9);
    const bool compact = getWidth() < 1100;
    logoArea = r.removeFromLeft (compact ? 38 : (FmlumeAudioProcessor::kDevBuild ? 190 : 152)); // room for the DEV tag
    r.removeFromLeft (compact ? 10 : 22);
    // right side: OUT label, slider, value
    outValueArea = r.removeFromRight (62);
    auto sliderArea = r.removeFromRight (compact ? 64 : 92);
    out.setBounds (sliderArea.withTrimmedBottom (4).withRight (outValueArea.getRight())); // track + value box
    meterArea = sliderArea.reduced (6, 0).removeFromBottom (6);
    outLabelArea = r.removeFromRight (36).withTrimmedRight (6);
    r.removeFromRight (compact ? 4 : 10);
    advancedBtn.setBounds (r.removeFromRight (compact ? 92 : 108));
    simpleBtn.setBounds (r.removeFromRight (compact ? 76 : 90));
    r.removeFromRight (compact ? 8 : 16);
    save.setBounds (r.removeFromRight (compact ? 52 : 64));
    r.removeFromRight (6);
    prev.setBounds (r.removeFromLeft (30));
    r.removeFromLeft (4);
    next.setBounds (r.removeFromRight (30));
    r.removeFromRight (4);
    nameArea = r;
}

void Header::mouseUp (const juce::MouseEvent& e)
{
    if (nameArea.contains (e.getPosition()) && onBrowse) onBrowse();
}

void Header::mouseMove (const juce::MouseEvent& e)
{
    const bool over = nameArea.contains (e.getPosition());
    if (over != overName) { overName = over; setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint (nameArea); }
}

void Header::mouseExit (const juce::MouseEvent&)
{
    if (overName) { overName = false; repaint (nameArea); }
}

void Header::refresh (const Preset& p)
{
    presetName = utf8 (p.meta.displayName);
    dirty = ctx.proc.isPresetDirty();
    repaint();
}

void Header::tick()
{
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float v = juce::jmax (ctx.proc.getPeak (ch), peak[ch] * 0.86f);
        changed = changed || std::abs (v - peak[ch]) > 1.0e-4f;
        peak[ch] = v < 1.0e-4f ? 0.0f : v;
    }
    if (changed) { repaint (meterArea); repaint (outLabelArea.translated (0, 13)); }
    // Macros and the other host parameters do not bump the model version: poll the edited mark.
    if (const bool d = ctx.proc.isPresetDirty(); d != dirty) { dirty = d; repaint (nameArea); }
}
} // namespace ui
