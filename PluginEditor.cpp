#include "PluginEditor.h"

BarrelKickEditor::BarrelKickEditor (BarrelKickProcessor& p)
    : AudioProcessorEditor (&p), proc (p), generic (p)
{
    const auto red = juce::Colour (0xffd01818);
    const auto bg  = juce::Colour (0xff0b0b0b);
    const auto mid = juce::Colour (0xff222222);

    laf.setColour (juce::ResizableWindow::backgroundColourId, bg);
    laf.setColour (juce::Slider::thumbColourId, red);
    laf.setColour (juce::Slider::trackColourId, red);
    laf.setColour (juce::Slider::backgroundColourId, mid);
    laf.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    laf.setColour (juce::ComboBox::backgroundColourId, mid);
    laf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff444444));
    laf.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    laf.setColour (juce::TextButton::buttonColourId, red);
    laf.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setLookAndFeel (&laf);

    addAndMakeVisible (generic);
    addAndMakeVisible (audition);
    audition.onClick = [this] { proc.auditionRequested = true; };

    setSize (900, 650);
}

BarrelKickEditor::~BarrelKickEditor() { setLookAndFeel (nullptr); }

void BarrelKickEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0b));
    g.setColour (juce::Colour (0xffd01818));
    g.fillRect (0, 52, getWidth(), 2);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.f, juce::Font::bold));
    g.drawText ("BARREL KICK GENERATOR", 16, 8, 500, 36, juce::Justification::centredLeft);
}

void BarrelKickEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (52);
    audition.setBounds (top.removeFromRight (160).reduced (8, 10));
    generic.setBounds (r.withTrimmedTop (2));
}
