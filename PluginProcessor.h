#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>

struct SampleSlot
{
    juce::String name;
    juce::File file;
    std::shared_ptr<juce::AudioBuffer<float>> data;
    double sampleRate = 44100.0;
    bool enabled = true;
    bool isLoop = false;
    int zone = -1;     // -1 = ANY, otherwise octave number (C3..C7 style, C3 = MIDI 60)
    int octave = 0;    // per-sample octave shift
};

using Library = std::vector<SampleSlot>;

class TendrilProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster
{
public:
    TendrilProcessor();
    ~TendrilProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Tendril"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // library (message thread)
    std::shared_ptr<const Library> getLibrary() const;
    void addFiles (const juce::Array<juce::File>& files);
    void clearLibrary();
    void removeSlot (int index);
    void modifySlot (int index, const std::function<void (SampleSlot&)>& fn);
    void resetParameters();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> activeVoices { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setLibrary (std::shared_ptr<const Library> lib);
    bool loadSlot (const juce::File& f, SampleSlot& out);

    struct Voice
    {
        bool active = false, releasing = false, loop = false;
        std::shared_ptr<juce::AudioBuffer<float>> buf;
        double pos = 0.0, inc = 1.0;
        float gain = 1.0f, panL = 1.0f, panR = 1.0f;
        float env = 0.0f, relCoef = 0.999f;
        int note = 0;
        uint32_t age = 0;
    };
    static constexpr int numVoices = 24;
    Voice voices[numVoices];
    uint32_t voiceCounter = 0;

    void handleNoteOn (const Library& lib, int note, float vel);
    void handleNoteOff (int note);
    void startVoice (const SampleSlot& s, int note, float vel, float semis, float gainMul);
    void renderVoices (juce::AudioBuffer<float>& buf, int start, int num);

    mutable juce::SpinLock libLock;
    std::shared_ptr<const Library> library { std::make_shared<Library>() };
    juce::AudioFormatManager formats;

    double sr = 44100.0;
    juce::Random rng;
    float lastGain = 1.0f;

    juce::dsp::StateVariableTPTFilter<float> lp[2], hp[2];
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TendrilProcessor)
};
