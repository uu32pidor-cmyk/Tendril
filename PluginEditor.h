#pragma once
#include "PluginProcessor.h"
#include <map>

class TendrilLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TendrilLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

class TendrilEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      public juce::ListBoxModel,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit TendrilEditor (TendrilProcessor&);
    ~TendrilEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void addHint (juce::Component&, const juce::String&);

    TendrilProcessor& proc;
    TendrilLookAndFeel lnf;

    juce::Slider mutate, spread, color, bloom, tail, output;
    juce::TextButton addBtn { "ADD" }, clearBtn { "CLEAR" }, resetBtn { "RESET" },
                     sproutBtn { "SPROUT" }, lockBtn { "LOCK KEY" };
    juce::ListBox list { "files", this };
    juce::MidiKeyboardComponent keyboard;
    juce::Label hint, info;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aMutate, aSpread, aColor, aBloom, aTail, aOutput;
    std::unique_ptr<BA> aSprout, aLock;

    std::map<juce::Component*, juce::String> hints;
    std::shared_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> field, header, listArea;
    const juce::String defaultHint = "TENDRIL  -  hover a control for a quick guide";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TendrilEditor)
};
