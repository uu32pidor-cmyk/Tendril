#include "PluginProcessor.h"
#include "PluginEditor.h"

TendrilProcessor::TendrilProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formats.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout TendrilProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };

    // MUTATE: how far into the sample a random start point may land (0% = always from the beginning)
    l.add (std::make_unique<P> (juce::ParameterID { "mutate", 1 }, "Mutate",
                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    // SPREAD: random stereo position + random detune per note
    l.add (std::make_unique<P> (juce::ParameterID { "spread", 1 }, "Spread",
                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    // COLOR: <0 low-pass (darker), >0 high-pass (thinner)
    l.add (std::make_unique<P> (juce::ParameterID { "color", 1 }, "Color",
                                juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                    [] (float v, int) { return juce::String (v, 2); })));
    // BLOOM: reverb amount
    l.add (std::make_unique<P> (juce::ParameterID { "bloom", 1 }, "Bloom",
                                juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    // TAIL: release time
    l.add (std::make_unique<P> (juce::ParameterID { "tail", 1 }, "Tail",
                                juce::NormalisableRange<float> (0.02f, 8.0f, 0.0f, 0.4f), 0.4f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                    [] (float v, int) { return juce::String (v, 2) + " s"; })));
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output",
                                juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));
    // SPROUT: every note also grows a second, quieter random layer an octave up
    l.add (std::make_unique<B> (juce::ParameterID { "sprout", 1 }, "Sprout", false));
    // LOCK: lock key - samples play at original pitch no matter which key triggers them
    l.add (std::make_unique<B> (juce::ParameterID { "lock", 1 }, "Lock Key", false));
    return l;
}

bool TendrilProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TendrilProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    for (int c = 0; c < 2; ++c)
    {
        lp[c].prepare (spec); lp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        hp[c].prepare (spec); hp[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
    }
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    for (auto& v : voices) v = Voice();
    lastGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());
}

// ------------------------------------------------------------------ library

std::shared_ptr<const Library> TendrilProcessor::getLibrary() const
{
    const juce::SpinLock::ScopedLockType sl (libLock);
    return library;
}

void TendrilProcessor::setLibrary (std::shared_ptr<const Library> lib)
{
    {
        const juce::SpinLock::ScopedLockType sl (libLock);
        library = std::move (lib);
    }
    sendChangeMessage();
}

bool TendrilProcessor::loadSlot (const juce::File& f, SampleSlot& out)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) return false;
    const auto len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 300.0));
    auto buf = std::make_shared<juce::AudioBuffer<float>> (std::min ((int) r->numChannels, 2), len);
    r->read (buf.get(), 0, len, 0, true, true);

    out.file = f;
    out.name = f.getFileNameWithoutExtension();
    out.data = buf;
    out.sampleRate = r->sampleRate;
    out.isLoop = out.name.containsIgnoreCase ("loop") || len / r->sampleRate > 6.0;
    return true;
}

void TendrilProcessor::addFiles (const juce::Array<juce::File>& files)
{
    auto next = std::make_shared<Library> (*getLibrary());
    for (auto& f : files)
    {
        if (f.isDirectory())
        {
            for (auto& c : f.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"))
            {
                SampleSlot s;
                if (loadSlot (c, s)) next->push_back (std::move (s));
            }
        }
        else
        {
            SampleSlot s;
            if (loadSlot (f, s)) next->push_back (std::move (s));
        }
    }
    setLibrary (next);
}

void TendrilProcessor::clearLibrary() { setLibrary (std::make_shared<Library>()); }

void TendrilProcessor::removeSlot (int index)
{
    auto next = std::make_shared<Library> (*getLibrary());
    if (juce::isPositiveAndBelow (index, (int) next->size()))
        next->erase (next->begin() + index);
    setLibrary (next);
}

void TendrilProcessor::modifySlot (int index, const std::function<void (SampleSlot&)>& fn)
{
    auto next = std::make_shared<Library> (*getLibrary());
    if (juce::isPositiveAndBelow (index, (int) next->size()))
        fn ((*next)[(size_t) index]);
    setLibrary (next);
}

void TendrilProcessor::resetParameters()
{
    for (auto* p : getParameters())
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

// ------------------------------------------------------------------ voices

void TendrilProcessor::startVoice (const SampleSlot& s, int note, float vel, float semis, float gainMul)
{
    if (s.data == nullptr || s.data->getNumSamples() < 2) return;

    const float mutate = apvts.getRawParameterValue ("mutate")->load();
    const float spread = apvts.getRawParameterValue ("spread")->load();
    const bool lock    = apvts.getRawParameterValue ("lock")->load() > 0.5f;

    // pick a free voice, otherwise steal the oldest
    Voice* v = &voices[0];
    for (auto& c : voices)
    {
        if (! c.active) { v = &c; break; }
        if (c.age < v->age) v = &c;
    }

    const int n = s.data->getNumSamples();
    const float detuneCents = (rng.nextFloat() * 2.0f - 1.0f) * spread * 25.0f;
    const float pitchSemis = (lock ? 0.0f : (float) (note - 60)) + (float) s.octave * 12.0f + semis + detuneCents / 100.0f;

    v->buf = s.data;
    v->loop = s.isLoop;
    v->pos = (double) rng.nextFloat() * (double) mutate * 0.9 * (double) n;
    v->inc = std::pow (2.0, (double) pitchSemis / 12.0) * s.sampleRate / sr;
    v->gain = vel * vel * gainMul;

    const float pan = (rng.nextFloat() * 2.0f - 1.0f) * spread;          // -1..1
    const float ang = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    v->panL = std::cos (ang) * 1.4142f;
    v->panR = std::sin (ang) * 1.4142f;

    v->env = 0.0f;
    v->releasing = false;
    v->note = note;
    v->age = ++voiceCounter;
    v->active = true;
}

void TendrilProcessor::handleNoteOn (const Library& lib, int note, float vel)
{
    const int zone = juce::jlimit (0, 12, note / 12 - 2);   // C3 = 60 -> 3
    std::vector<const SampleSlot*> cands, any;
    for (auto& s : lib)
    {
        if (! s.enabled || s.data == nullptr) continue;
        any.push_back (&s);
        if (s.zone < 0 || s.zone == zone) cands.push_back (&s);
    }
    if (cands.empty()) cands = any;   // nothing assigned to this key range -> fall back to everything
    if (cands.empty()) return;

    startVoice (*cands[(size_t) rng.nextInt ((int) cands.size())], note, vel, 0.0f, 0.7f);

    if (apvts.getRawParameterValue ("sprout")->load() > 0.5f)
        startVoice (*cands[(size_t) rng.nextInt ((int) cands.size())], note, vel, 12.0f, 0.3f);
}

void TendrilProcessor::handleNoteOff (int note)
{
    const float tail = apvts.getRawParameterValue ("tail")->load();
    const float coef = std::pow (0.001f, 1.0f / (juce::jmax (0.02f, tail) * (float) sr));
    for (auto& v : voices)
        if (v.active && ! v.releasing && v.note == note)
        {
            v.releasing = true;
            v.relCoef = coef;
        }
}

void TendrilProcessor::renderVoices (juce::AudioBuffer<float>& buf, int start, int num)
{
    float* outL = buf.getWritePointer (0, start);
    float* outR = buf.getWritePointer (1, start);
    const float atk = 1.0f / (0.003f * (float) sr);
    int count = 0;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        ++count;

        const int len = v.buf->getNumSamples();
        const float* s0 = v.buf->getReadPointer (0);
        const float* s1 = v.buf->getReadPointer (v.buf->getNumChannels() > 1 ? 1 : 0);

        for (int i = 0; i < num; ++i)
        {
            int i0 = (int) v.pos;
            if (i0 >= len)
            {
                if (v.loop) { v.pos -= (double) len; i0 = juce::jmax (0, (int) v.pos); }
                else { v.active = false; break; }
            }
            int i1 = i0 + 1;
            if (i1 >= len) i1 = v.loop ? 0 : i0;
            const float fr = (float) (v.pos - (double) i0);

            const float a = s0[i0] + (s0[i1] - s0[i0]) * fr;
            const float b = s1[i0] + (s1[i1] - s1[i0]) * fr;

            if (v.releasing)
            {
                v.env *= v.relCoef;
                if (v.env < 0.0003f) { v.active = false; break; }
            }
            else if (v.env < 1.0f)
                v.env = juce::jmin (1.0f, v.env + atk);

            const float g = v.gain * v.env;
            outL[i] += a * g * v.panL;
            outR[i] += b * g * v.panR;
            v.pos += v.inc;
        }
        if (! v.active) v.buf.reset();
    }
    activeVoices.store (count);
}

// ------------------------------------------------------------------ process

void TendrilProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, total, true);
    auto lib = getLibrary();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (pos, total, meta.samplePosition);
        if (t > pos) { renderVoices (buffer, pos, t - pos); pos = t; }

        const auto m = meta.getMessage();
        if (m.isNoteOn())       handleNoteOn (*lib, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) handleNoteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) { v.releasing = true; v.relCoef = 0.995f; }
    }
    if (pos < total) renderVoices (buffer, pos, total - pos);

    // COLOR: negative -> low-pass sweep down, positive -> high-pass sweep up
    const float color = apvts.getRawParameterValue ("color")->load();
    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    if (color < -0.01f)
    {
        const float f = 20000.0f * std::pow (350.0f / 20000.0f, -color);
        for (int c = 0; c < 2; ++c)
        {
            lp[c].setCutoffFrequency (f);
            for (int i = 0; i < total; ++i) ch[c][i] = lp[c].processSample (0, ch[c][i]);
        }
    }
    else if (color > 0.01f)
    {
        const float f = 20.0f * std::pow (4000.0f / 20.0f, color);
        for (int c = 0; c < 2; ++c)
        {
            hp[c].setCutoffFrequency (f);
            for (int i = 0; i < total; ++i) ch[c][i] = hp[c].processSample (0, ch[c][i]);
        }
    }

    // BLOOM: reverb
    const float bloom = apvts.getRawParameterValue ("bloom")->load();
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f + 0.4f * bloom;
    rp.damping = 0.45f;
    rp.wetLevel = bloom * 0.55f;
    rp.dryLevel = 1.0f - bloom * 0.25f;
    rp.width = 1.0f;
    reverb.setParameters (rp);
    reverb.processStereo (ch[0], ch[1], total);

    const float g = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());
    buffer.applyGainRamp (0, total, lastGain, g);
    lastGain = g;
}

// ------------------------------------------------------------------ state

void TendrilProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("LIBRARY"), nullptr);

    juce::ValueTree lib ("LIBRARY");
    for (auto& s : *getLibrary())
    {
        juce::ValueTree t ("SAMPLE");
        t.setProperty ("path", s.file.getFullPathName(), nullptr);
        t.setProperty ("enabled", s.enabled, nullptr);
        t.setProperty ("loop", s.isLoop, nullptr);
        t.setProperty ("zone", s.zone, nullptr);
        t.setProperty ("octave", s.octave, nullptr);
        lib.appendChild (t, nullptr);
    }
    state.appendChild (lib, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void TendrilProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    auto libTree = state.getChildWithName ("LIBRARY");
    state.removeChild (libTree, nullptr);
    apvts.replaceState (state);

    auto next = std::make_shared<Library>();
    for (auto t : libTree)
    {
        SampleSlot s;
        juce::File f (t.getProperty ("path").toString());
        if (! f.existsAsFile() || ! loadSlot (f, s)) continue;
        s.enabled = (bool) t.getProperty ("enabled", true);
        s.isLoop  = (bool) t.getProperty ("loop", s.isLoop);
        s.zone    = (int) t.getProperty ("zone", -1);
        s.octave  = (int) t.getProperty ("octave", 0);
        next->push_back (std::move (s));
    }
    setLibrary (next);
}

juce::AudioProcessorEditor* TendrilProcessor::createEditor() { return new TendrilEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TendrilProcessor(); }
