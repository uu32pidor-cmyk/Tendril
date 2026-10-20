#include "PluginEditor.h"

namespace pal
{
    const juce::Colour bg        { 0xffeceee6 };
    const juce::Colour panel     { 0xfff6f6f0 };
    const juce::Colour deep      { 0xff1f4a3a };
    const juce::Colour mid       { 0xff5f8470 };
    const juce::Colour sage      { 0xff8fb09a };
    const juce::Colour mist      { 0xffc9d6dc };
    const juce::Colour line      { 0xffd9ddd2 };
}

// ------------------------------------------------------------------ look & feel

TendrilLookAndFeel::TendrilLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, pal::panel);
    setColour (juce::TextButton::buttonOnColourId, pal::deep);
    setColour (juce::TextButton::textColourOffId, pal::deep);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, pal::deep.withAlpha (0.8f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, pal::deep);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

void TendrilLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float a0, float a1, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();

    juce::ColourGradient glow (juce::Colours::white.withAlpha (0.9f), c.x, c.y,
                               juce::Colours::white.withAlpha (0.0f), c.x, c.y - r * 1.5f, true);
    g.setGradientFill (glow);
    g.fillEllipse (c.x - r * 1.5f, c.y - r * 1.5f, r * 3.0f, r * 3.0f);

    g.setColour (pal::panel);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (pal::deep.withAlpha (0.35f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);

    const float ang = a0 + pos * (a1 - a0);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r + 5.0f, r + 5.0f, 0.0f, a0, a1, true);
    g.setColour (pal::deep.withAlpha (0.15f));
    g.strokePath (arc, juce::PathStrokeType (2.0f));

    const float zero = s.getMinimum() < 0 ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r + 5.0f, r + 5.0f, 0.0f, a0 + zero * (a1 - a0), ang, true);
    g.setColour (pal::deep);
    g.strokePath (val, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path p;
    p.startNewSubPath (0.0f, -r * 0.35f);
    p.lineTo (0.0f, -r * 0.85f);
    g.setColour (pal::deep);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void TendrilLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                               bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto col = b.getToggleState() ? pal::deep : pal::panel;
    if (over) col = col.brighter (0.08f);
    if (down) col = col.darker (0.1f);
    g.setColour (col);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (pal::deep.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void TendrilLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (pal::deep.withAlpha (0.2f));
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (pal::deep);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

// ------------------------------------------------------------------ editor

TendrilEditor::TendrilEditor (TendrilProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    setSize (860, 620);

    auto setupKnob = [this] (juce::Slider& s, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (s);
        addHint (s, tip);
    };
    setupKnob (mutate, "MUTATE - random starting point inside each sample. 0% always starts at the beginning.");
    setupKnob (spread, "SPREAD - random stereo position and slight detune for every note.");
    setupKnob (color,  "COLOR - left = darker (low-pass), right = thinner (high-pass).");
    setupKnob (bloom,  "BLOOM - reverb amount.");
    setupKnob (tail,   "TAIL - how long a note rings out after you release the key.");

    aMutate = std::make_unique<SA> (proc.apvts, "mutate", mutate);
    aSpread = std::make_unique<SA> (proc.apvts, "spread", spread);
    aColor  = std::make_unique<SA> (proc.apvts, "color", color);
    aBloom  = std::make_unique<SA> (proc.apvts, "bloom", bloom);
    aTail   = std::make_unique<SA> (proc.apvts, "tail", tail);

    output.setSliderStyle (juce::Slider::LinearHorizontal);
    output.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    output.setTextValueSuffix (" dB");
    addAndMakeVisible (output);
    aOutput = std::make_unique<SA> (proc.apvts, "output", output);
    addHint (output, "OUTPUT - master volume.");

    for (auto* b : { &sproutBtn, &lockBtn })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    aSprout = std::make_unique<BA> (proc.apvts, "sprout", sproutBtn);
    aLock   = std::make_unique<BA> (proc.apvts, "lock", lockBtn);
    addHint (sproutBtn, "SPROUT - every note grows a second, quieter random layer an octave up.");
    addHint (lockBtn, "LOCK KEY - samples keep their original pitch; the key only triggers them.");

    for (auto* b : { &addBtn, &clearBtn, &resetBtn }) addAndMakeVisible (b);
    addHint (addBtn, "ADD - load audio files or folders (you can also drag & drop them anywhere).");
    addHint (clearBtn, "CLEAR - remove every loaded sample.");
    addHint (resetBtn, "RESET - put all knobs back to their defaults.");

    addBtn.onClick = [this]
    {
        chooser = std::make_shared<juce::FileChooser> ("Choose audio files", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { proc.addFiles (fc.getResults()); });
    };
    clearBtn.onClick = [this] { proc.clearLibrary(); };
    resetBtn.onClick = [this] { proc.resetParameters(); };

    list.setRowHeight (28);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    keyboard.setKeyWidth (24.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, pal::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, pal::deep);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, pal::line);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, pal::sage.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, pal::sage.withAlpha (0.4f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, pal::panel);
    addAndMakeVisible (keyboard);

    hint.setText (defaultHint, juce::dontSendNotification);
    hint.setFont (juce::FontOptions (12.0f));
    hint.setColour (juce::Label::textColourId, pal::deep.withAlpha (0.7f));
    addAndMakeVisible (hint);
    info.setJustificationType (juce::Justification::centredRight);
    info.setFont (juce::FontOptions (12.0f));
    info.setColour (juce::Label::textColourId, pal::mid);
    addAndMakeVisible (info);

    proc.addChangeListener (this);
    startTimerHz (10);
    timerCallback();
}

TendrilEditor::~TendrilEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void TendrilEditor::addHint (juce::Component& c, const juce::String& text)
{
    hints[&c] = text;
    c.addMouseListener (this, true);
}

void TendrilEditor::mouseEnter (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr; c = c->getParentComponent())
    {
        auto it = hints.find (c);
        if (it != hints.end()) { hint.setText (it->second, juce::dontSendNotification); return; }
    }
}

void TendrilEditor::mouseExit (const juce::MouseEvent&) { hint.setText (defaultHint, juce::dontSendNotification); }

void TendrilEditor::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> fs;
    for (auto& f : files) fs.add (juce::File (f));
    proc.addFiles (fs);
}

void TendrilEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    list.updateContent();
    list.repaint();
    repaint();
}

void TendrilEditor::timerCallback()
{
    auto lib = proc.getLibrary();
    int on = 0;
    for (auto& s : *lib) on += s.enabled ? 1 : 0;
    info.setText (juce::String (proc.activeVoices.load()) + " VOICES  /  " + juce::String (on) + " of "
                      + juce::String ((int) lib->size()) + " SAMPLES",
                  juce::dontSendNotification);
}

// ------------------------------------------------------------------ list

int TendrilEditor::getNumRows() { return (int) proc.getLibrary()->size(); }

static juce::String zoneText (int z) { return z < 0 ? "ANY" : "C" + juce::String (z) + "-B" + juce::String (z); }

void TendrilEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    auto lib = proc.getLibrary();
    if (! juce::isPositiveAndBelow (row, (int) lib->size())) return;
    auto& s = (*lib)[(size_t) row];

    g.setColour (pal::line);
    g.drawHorizontalLine (h - 1, 6.0f, (float) w - 6.0f);

    auto on = juce::Rectangle<int> (8, 4, 40, h - 8);
    g.setColour (s.enabled ? pal::deep : pal::line);
    g.fillRoundedRectangle (on.toFloat(), 4.0f);
    g.setColour (s.enabled ? juce::Colours::white : pal::deep.withAlpha (0.6f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (s.enabled ? "ON" : "OFF", on, juce::Justification::centred);

    g.setColour (pal::deep.withAlpha (s.enabled ? 1.0f : 0.4f));
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (s.name, 58, 0, w - 330, h, juce::Justification::centredLeft, true);

    g.setColour (pal::mid);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (s.isLoop ? "LOOP" : "ONE-SHOT", w - 260, 0, 64, h, juce::Justification::centred);
    g.drawText (zoneText (s.zone), w - 190, 0, 64, h, juce::Justification::centred);
    g.drawText ("OCT " + juce::String (s.octave), w - 120, 0, 56, h, juce::Justification::centred);
    g.setColour (pal::deep.withAlpha (0.6f));
    g.drawText ("X", w - 36, 0, 28, h, juce::Justification::centred);
}

void TendrilEditor::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const int w = list.getVisibleRowWidth();
    const int x = e.x;

    if (x < 54)                    proc.modifySlot (row, [] (SampleSlot& s) { s.enabled = ! s.enabled; });
    else if (x >= w - 36)          proc.removeSlot (row);
    else if (x >= w - 260 && x < w - 196)
        proc.modifySlot (row, [] (SampleSlot& s) { s.isLoop = ! s.isLoop; });
    else if (x >= w - 190 && x < w - 126)
        proc.modifySlot (row, [] (SampleSlot& s)
        {
            static const int zones[] = { -1, 3, 4, 5, 6, 7 };
            int idx = 0;
            for (int i = 0; i < 6; ++i) if (zones[i] == s.zone) idx = i;
            s.zone = zones[(idx + 1) % 6];
        });
    else if (x >= w - 120 && x < w - 64)
        proc.modifySlot (row, [mid = x < w - 92] (SampleSlot& s) { s.octave = juce::jlimit (-3, 3, s.octave + (mid ? -1 : 1)); });
}

// ------------------------------------------------------------------ layout & paint

void TendrilEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 18);

    header = r.removeFromTop (48);
    auto h = header;
    h.removeFromLeft (96);
    auto outArea = h.removeFromRight (200);
    output.setBounds (outArea.reduced (4, 10));
    info.setBounds (h.removeFromRight (230));
    hint.setBounds (h);

    r.removeFromTop (8);
    auto tools = r.removeFromTop (34).reduced (8, 3);
    addBtn.setBounds (tools.removeFromLeft (70)); tools.removeFromLeft (6);
    clearBtn.setBounds (tools.removeFromLeft (70)); tools.removeFromLeft (6);
    resetBtn.setBounds (tools.removeFromLeft (70));
    lockBtn.setBounds (tools.removeFromRight (100)); tools.removeFromRight (6);
    sproutBtn.setBounds (tools.removeFromRight (90));

    listArea = r.removeFromTop (150);
    list.setBounds (listArea.reduced (6, 6));
    r.removeFromTop (10);

    keyboard.setBounds (r.removeFromBottom (80));
    r.removeFromBottom (10);
    field = r;

    // knobs staggered like a vine
    const int kw = 120, kh = 110;
    auto place = [&] (juce::Slider& s, float fx, float fy)
    {
        s.setBounds ((int) ((float) field.getX() + fx * (float) field.getWidth()) - kw / 2,
                     (int) ((float) field.getY() + fy * (float) field.getHeight()) - kh / 2, kw, kh);
    };
    place (mutate, 0.28f, 0.30f);
    place (spread, 0.72f, 0.30f);
    place (color,  0.14f, 0.70f);
    place (bloom,  0.50f, 0.68f);
    place (tail,   0.86f, 0.70f);
}

void TendrilEditor::paint (juce::Graphics& g)
{
    g.fillAll (pal::bg);

    // header + tool panel
    g.setColour (pal::panel);
    g.fillRoundedRectangle (header.toFloat(), 8.0f);
    g.setColour (pal::line);
    g.drawRoundedRectangle (header.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (pal::deep);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("TENDRIL", header.withTrimmedLeft (14).withWidth (120), juce::Justification::centredLeft);

    auto tools = juce::Rectangle<int> (header.getX(), header.getBottom() + 8, header.getWidth(), 34 + 150);
    g.setColour (pal::panel.withAlpha (0.8f));
    g.fillRoundedRectangle (tools.toFloat(), 8.0f);
    g.setColour (pal::line);
    g.drawRoundedRectangle (tools.toFloat().reduced (0.5f), 8.0f, 1.0f);

    if (proc.getLibrary()->empty())
    {
        g.setColour (pal::deep.withAlpha (0.45f));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("DROP AUDIO FILES OR FOLDERS HERE", listArea, juce::Justification::centred);
    }

    // sage field
    juce::ColourGradient grad (pal::mist, (float) field.getX(), (float) field.getY(),
                               pal::sage, (float) field.getRight(), (float) field.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (field.toFloat(), 10.0f);

    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (field.toFloat(), 10.0f);
    g.reduceClipRegion (clip);

    juce::Path hill;
    const float by = (float) field.getY() + (float) field.getHeight() * 0.52f;
    hill.startNewSubPath ((float) field.getX(), (float) field.getBottom());
    hill.lineTo ((float) field.getX(), by);
    for (int i = 0; i <= 40; ++i)
    {
        const float t = (float) i / 40.0f;
        hill.lineTo ((float) field.getX() + t * (float) field.getWidth(),
                     by + 10.0f * std::sin (t * 9.0f) + 6.0f * std::sin (t * 4.0f + 1.0f));
    }
    hill.lineTo ((float) field.getRight(), (float) field.getBottom());
    hill.closeSubPath();
    g.setColour (pal::mid.withAlpha (0.35f));
    g.fillPath (hill);

    // frosted panes
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    const float fx = (float) field.getX(), fy = (float) field.getY(), fw = (float) field.getWidth(), fh = (float) field.getHeight();
    g.fillRect (fx + fw * 0.10f, fy + fh * 0.10f, fw * 0.16f, fh * 0.30f);
    g.fillRect (fx + fw * 0.36f, fy + fh * 0.22f, fw * 0.09f, fh * 0.62f);
    g.fillRect (fx + fw * 0.60f, fy + fh * 0.18f, fw * 0.10f, fh * 0.60f);
    g.fillRect (fx + fw * 0.76f, fy + fh * 0.08f, fw * 0.14f, fh * 0.36f);

    // knob labels
    g.setColour (pal::deep);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (auto* s : { &mutate, &spread, &color, &bloom, &tail })
        g.drawText (s == &mutate ? "MUTATE" : s == &spread ? "SPREAD" : s == &color ? "COLOR"
                                  : s == &bloom ? "BLOOM" : "TAIL",
                    s->getX(), s->getY() - 14, s->getWidth(), 14, juce::Justification::centred);
}
