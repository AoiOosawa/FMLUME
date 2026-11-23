#include "UI/Common/Theme.h"

namespace ui
{
namespace
{
// The logo's wave: two clean sine periods. All three layers use it (see tools/branding/make_assets.py).
float logoWave (int, float x) { return std::sin (juce::MathConstants<float>::twoPi * 2.0f * x); }

juce::Path logoPath (int kind, juce::Rectangle<float> box, float amplitude)
{
    juce::Path p;
    const int n = 160;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        const juce::Point<float> pt (box.getX() + x * box.getWidth(), box.getCentreY() - logoWave (kind, x) * amplitude);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    return p;
}
} // namespace

void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (size, size);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2d4a), box.getX(), box.getY(), col::bg, box.getRight(), box.getBottom(), false));
    g.fillRoundedRectangle (box, size * 0.22f);
    const auto wave = box.reduced (size * 0.17f, 0.0f);
    const auto round = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    const float amp = size * 0.2f, dx = size * 0.045f, dy = size * 0.085f;
    g.setColour (col::accentB.withAlpha (0.55f));
    g.strokePath (logoPath (0, wave.translated (-dx, -dy), amp), round (size * 0.042f));
    g.setColour (col::hot.withAlpha (0.5f));
    g.strokePath (logoPath (0, wave.translated (dx, dy), amp), round (size * 0.042f));
    juce::ColourGradient hero (col::accent, wave.getX(), 0.0f, col::hot.interpolatedWith (col::violet, 0.35f), wave.getRight(), 0.0f, false);
    hero.addColour (0.55, col::violet);
    g.setGradientFill (hero);
    g.strokePath (logoPath (0, wave, amp), round (size * 0.062f));
}

namespace
{
// Vertical rocker switch as used across the series.
void drawRocker (juce::Graphics& g, juce::Rectangle<float> body, bool on, juce::Colour colour, bool enabled)
{
    const float alpha = enabled ? 1.0f : 0.4f;
    g.setColour (col::bg.withAlpha (alpha));
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (col::outline.brighter (0.25f).withAlpha (alpha));
    g.drawRoundedRectangle (body.reduced (0.5f), 5.0f, 1.2f);
    auto inner = body.reduced (3.0f);
    const auto cap = on ? inner.removeFromTop (inner.getHeight() * 0.48f) : inner.removeFromBottom (inner.getHeight() * 0.48f);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.25f).withAlpha (alpha), cap.getX(), cap.getY(), colour.withAlpha (alpha), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.5f);
        g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
        g.fillRoundedRectangle (cap.withSizeKeepingCentre (cap.getWidth() * 0.45f, 2.4f), 1.2f);
    }
    else
    {
        g.setColour (juce::Colour (0xff5a5e78).withAlpha (alpha));
        g.fillRoundedRectangle (cap, 3.5f);
        g.setColour (col::textDim.withAlpha (alpha));
        const float d = juce::jmin (cap.getHeight() * 0.42f, 8.0f);
        g.drawEllipse (cap.withSizeKeepingCentre (d, d), 1.4f);
    }
}
} // namespace

LumeLookAndFeel::LumeLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, col::textDim);
    setColour (juce::TextButton::textColourOnId, col::onAccent);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::outlineColourId, col::outline);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::textDim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, col::onAccent);
    setColour (juce::TextEditor::backgroundColourId, col::bg);
    setColour (juce::TextEditor::outlineColourId, col::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::ListBox::backgroundColourId, col::bg);
    setColour (juce::ListBox::outlineColourId, col::outline);
    setColour (juce::ScrollBar::thumbColourId, col::outline);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, col::textDim);
    setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::outline);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::outline);
}

void LumeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax (2.5f, radius * 0.16f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;
    const juce::Colour fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::Colour fill2 = fill == col::accent ? col::violet : (fill == col::accentB ? col::hot : fill.brighter (0.3f));

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (col::outline.withAlpha (alpha));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc: bipolar parameters grow from the centre, others from the start.
    const bool bipolar = s.getProperties()["bipolar"];
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setGradientFill (juce::ColourGradient (fill.withAlpha (alpha), centre.x - radius, centre.y + radius, fill2.withAlpha (alpha), centre.x + radius, centre.y - radius, false));
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - thickness * 1.6f;
    g.setColour (col::panelHi.withAlpha (alpha));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-thickness * 0.3f, -inner, thickness * 0.6f, inner * 0.55f, thickness * 0.3f);
    g.setColour (col::text.withAlpha (alpha));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    if (s.hasKeyboardFocus (false))
    {
        g.setColour (col::text.withAlpha (0.5f));
        g.drawEllipse (centre.x - radius - 1.0f, centre.y - radius - 1.0f, radius * 2.0f + 2.0f, radius * 2.0f + 2.0f, 1.0f);
    }
}

void LumeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::TwoValueHorizontal)
    {
        // range: the part between the two thumbs is lit
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto track = area.withSizeKeepingCentre (area.getWidth(), 5.0f);
        const float a = s.isEnabled() ? 1.0f : 0.35f;
        g.setColour (col::outline.withAlpha (a));
        g.fillRoundedRectangle (track, 2.5f);
        g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (a));
        g.fillRoundedRectangle (juce::Rectangle<float> (minPos, track.getY(), juce::jmax (0.0f, maxPos - minPos), track.getHeight()), 2.5f);
        g.setColour (col::text.withAlpha (a));
        for (float thumb : { minPos, maxPos }) g.fillRoundedRectangle (thumb - 3.0f, area.getCentreY() - 7.0f, 6.0f, 14.0f, 2.0f);
        return;
    }
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;
    const juce::Colour fill = s.findColour (juce::Slider::trackColourId).withAlpha (alpha);
    if (horizontal)
    {
        const auto track = r.withSizeKeepingCentre (r.getWidth(), 5.0f);
        g.setColour (col::outline.withAlpha (alpha));
        g.fillRoundedRectangle (track, 2.5f);
        const bool bipolar = s.getProperties()["bipolar"];
        const float from = bipolar ? track.getCentreX() : track.getX();
        g.setColour (fill);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (from, pos), track.getY(), std::abs (pos - from), track.getHeight()), 2.5f);
        g.setColour (col::text.withAlpha (alpha));
        g.fillEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
    }
    else
    {
        const auto track = r.withSizeKeepingCentre (5.0f, r.getHeight());
        g.setColour (col::outline.withAlpha (alpha));
        g.fillRoundedRectangle (track, 2.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX(), pos, track.getWidth(), track.getBottom() - pos), 2.5f);
        g.setColour (col::text.withAlpha (alpha));
        g.fillEllipse (r.getCentreX() - 6.0f, pos - 6.0f, 12.0f, 12.0f);
    }
}

void LumeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour c = base;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
    else if (down) c = c.brighter (0.25f);
    else if (highlighted) c = c.brighter (0.12f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (b.getToggleState() ? c.brighter (0.3f) : (b.hasKeyboardFocus (false) ? col::textDim : col::outline));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

// Two layouts: tall buttons (>= 50 px) are the series' rocker switch with the caption above and
// ON / OFF below; short ones are an inline pill switch with the caption to the right.
void LumeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const bool on = b.getToggleState(), enabled = b.isEnabled();
    const juce::Colour colour = b.findColour (juce::ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat();
    if (r.getHeight() >= 50.0f)
    {
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (enabled ? col::textDim : col::textDim.withAlpha (0.5f));
            g.setFont (font (12.0f));
            g.drawFittedText (b.getButtonText(), r.removeFromTop (16.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
        const auto state = r.removeFromBottom (16.0f);
        g.setColour ((on ? colour : col::textDim).withAlpha (enabled ? 1.0f : 0.5f));
        g.setFont (font (11.5f, true));
        g.drawText (on ? "ON" : "OFF", state, juce::Justification::centred);
        const float bh = juce::jmin (r.getHeight() - 4.0f, 46.0f);
        drawRocker (g, r.withSizeKeepingCentre (27.0f, bh), on, colour, enabled);
        return;
    }
    const float ph = juce::jmin (16.0f, r.getHeight() - 4.0f), pw = ph * 1.8f;
    const juce::Rectangle<float> pill (1.5f, (r.getHeight() - ph) * 0.5f, pw, ph);
    g.setColour (on ? colour.withAlpha (enabled ? 0.9f : 0.35f) : col::bg);
    g.fillRoundedRectangle (pill, ph * 0.5f);
    g.setColour (b.hasKeyboardFocus (false) ? col::text : (on ? colour : col::outline.brighter (0.25f)));
    g.drawRoundedRectangle (pill.reduced (0.5f), ph * 0.5f, 1.2f);
    const float d = ph - 5.0f;
    g.setColour (on ? juce::Colours::white : col::textDim);
    g.fillEllipse (on ? pill.getRight() - d - 2.5f : pill.getX() + 2.5f, pill.getCentreY() - d * 0.5f, d, d);
    g.setColour (enabled ? col::text : col::textDim);
    g.setFont (font (13.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft ((int) pw + 9), juce::Justification::centredLeft, 2);
}

void LumeLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getActiveArea().toFloat().reduced (2.0f, 3.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? col::panelHi : (over ? col::panel.brighter (0.08f) : col::panel));
    g.fillRoundedRectangle (r, 6.0f);
    if (front)
    {
        g.setGradientFill (juce::ColourGradient (col::accent, r.getX(), 0.0f, col::violet, r.getRight(), 0.0f, false));
        g.fillRoundedRectangle (juce::Rectangle<float> (r).removeFromBottom (3.0f).reduced (10.0f, 0.0f), 1.5f);
    }
    g.setColour (front ? col::text : col::textDim);
    g.setFont (font (14.0f, true));
    g.drawText (b.getButtonText(), b.getActiveArea(), juce::Justification::centred);
}

int LumeLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int)
{
    return 40 + juce::GlyphArrangement::getStringWidthInt (font (14.0f, true), b.getButtonText());
}

void paintPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (col::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        auto head = area.reduced (12, 9).removeFromTop (16);
        g.setGradientFill (juce::ColourGradient (col::accent, 0.0f, (float) head.getY(), col::violet, 0.0f, (float) head.getBottom(), false));
        g.fillRoundedRectangle (head.removeFromLeft (3).toFloat(), 1.5f);
        g.setColour (col::text);
        g.setFont (font (13.5f, true));
        g.drawText (title.toUpperCase(), head.withTrimmedLeft (8), juce::Justification::centredLeft);
    }
}
} // namespace ui
