#include "PluginProcessor.h"
#include "PluginEditor.h"

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

static void addF (Layout& l, const char* id, const char* name, float lo, float hi, float def,
                  float centre = -1.f, const char* unit = "")
{
    juce::NormalisableRange<float> r (lo, hi);
    if (centre > 0.f) r.setSkewForCentre (centre);
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                       juce::AudioParameterFloatAttributes().withLabel (unit)));
}

static void addC (Layout& l, const char* id, const char* name, juce::StringArray items, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
}

Layout BarrelKickProcessor::createLayout()
{
    Layout l;
    addF (l, "pStart", "Pitch Start",   40.f, 1500.f, 280.f, 250.f, " Hz");
    addF (l, "pEnd",   "Pitch End",     25.f,  120.f,  48.f,  55.f, " Hz");
    addF (l, "pTime",  "Pitch Time",   0.005f, 0.3f, 0.045f, 0.05f, " s");
    addF (l, "pCurve", "Pitch Curve",   0.3f,   4.f,  1.5f,   1.2f);

    addF (l, "subLvl",   "Sub Level",   0.f, 1.f, 0.8f);
    addF (l, "subDecay", "Sub Decay", 0.05f, 1.2f, 0.45f, 0.35f, " s");
    addF (l, "subRatio", "Sub Ratio", 0.25f, 1.f, 1.f);

    addF (l, "bodyLvl",   "Body Level", 0.f, 1.f, 0.8f);
    addF (l, "bodyDecay", "Body Decay", 0.02f, 1.f, 0.25f, 0.2f, " s");
    addC (l, "bodyShape", "Body Wave", { "Sine", "Triangle", "Saw", "Square", "Soft Square" }, 1);

    addF (l, "clickLvl",   "Click Level", 0.f, 1.f, 0.5f);
    addF (l, "clickDecay", "Click Decay", 0.001f, 0.03f, 0.006f, 0.008f, " s");
    addF (l, "clickTone",  "Click Tone", 500.f, 16000.f, 5000.f, 3000.f, " Hz");

    addF (l, "noiseLvl",   "Noise Level", 0.f, 1.f, 0.15f);
    addF (l, "noiseDecay", "Noise Decay", 0.01f, 0.5f, 0.08f, 0.1f, " s");
    addF (l, "noiseCut",   "Noise Cutoff", 200.f, 16000.f, 3000.f, 2000.f, " Hz");

    addF (l, "resLvl",   "Resonance Level", 0.f, 1.f, 0.4f);
    addF (l, "resFreq",  "Resonance Freq", 40.f, 400.f, 90.f, 120.f, " Hz");
    addF (l, "resDecay", "Resonance Decay", 0.05f, 1.f, 0.35f, 0.3f, " s");

    addF (l, "drive",    "Drive", 0.f, 40.f, 12.f, -1.f, " dB");
    addC (l, "distType", "Distortion Type", { "Soft Clip", "Hard Clip", "Wavefold", "Saturation" }, 0);
    addF (l, "distMix",  "Distortion Mix", 0.f, 1.f, 1.f);
    addC (l, "oversample", "Oversampling", { "Off", "2x", "4x" }, 1);

    addF (l, "outGain", "Output", -24.f, 6.f, -3.f, -1.f, " dB");
    return l;
}

BarrelKickProcessor::BarrelKickProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool BarrelKickProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BarrelKickProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
    mono.setSize (1, 4096);
    for (int i = 0; i < 2; ++i)
    {
        os[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) (i + 1), juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os[i]->initProcessing ((size_t) kMaxBlock);
    }
    lastOsIdx = -1;
    hpX = hpY = 0.f;
}

float BarrelKickProcessor::distortSample (float x) const
{
    const float d = x * driveLin;
    float w;
    switch (distType)
    {
        case 0:  w = std::tanh (d); break;
        case 1:  w = juce::jlimit (-1.f, 1.f, d); break;
        case 2:  w = std::sin (d * 1.5707963f); break;
        default: w = std::tanh (d + 0.25f) - std::tanh (0.25f); break;
    }
    return x + (w - x) * distMix;
}

void BarrelKickProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    KickParams k;
    k.pitchStart = get ("pStart"); k.pitchEnd = get ("pEnd");
    k.pitchTime  = get ("pTime");  k.pitchCurve = get ("pCurve");
    k.subLvl = get ("subLvl"); k.subDecay = get ("subDecay"); k.subRatio = get ("subRatio");
    k.bodyLvl = get ("bodyLvl"); k.bodyDecay = get ("bodyDecay"); k.bodyShape = (int) get ("bodyShape");
    k.clickLvl = get ("clickLvl"); k.clickDecay = get ("clickDecay"); k.clickTone = get ("clickTone");
    k.noiseLvl = get ("noiseLvl"); k.noiseDecay = get ("noiseDecay"); k.noiseCut = get ("noiseCut");
    k.resLvl = get ("resLvl"); k.resFreq = get ("resFreq"); k.resDecay = get ("resDecay");
    engine.setParams (k);

    driveLin = juce::Decibels::decibelsToGain (get ("drive"));
    distType = (int) get ("distType");
    distMix  = get ("distMix");
    outGain  = juce::Decibels::decibelsToGain (get ("outGain"));
    const int osIdx = (int) get ("oversample");

    if (osIdx != lastOsIdx)
    {
        if (osIdx > 0) os[osIdx - 1]->reset();
        setLatencySamples (osIdx > 0 ? juce::roundToInt (os[osIdx - 1]->getLatencyInSamples()) : 0);
        lastOsIdx = osIdx;
    }

    if (mono.getNumSamples() < n) mono.setSize (1, n, false, false, true);
    float* m = mono.getWritePointer (0);

    auto renderTo = [&] (int a, int b) { for (int i = a; i < b; ++i) m[i] = engine.process(); };

    if (auditionRequested.exchange (false)) engine.trigger (1.f);

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int ep = juce::jlimit (pos, n, meta.samplePosition);
        renderTo (pos, ep);
        pos = ep;
        if (msg.isNoteOn())                                   engine.trigger (msg.getFloatVelocity());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())  engine.kill();
    }
    renderTo (pos, n);

    // Distortion (optionally oversampled)
    for (int start = 0; start < n; start += kMaxBlock)
    {
        const int len = juce::jmin (kMaxBlock, n - start);
        float* ptr = m + start;
        if (osIdx == 0)
        {
            for (int i = 0; i < len; ++i) ptr[i] = distortSample (ptr[i]);
        }
        else
        {
            float* chans[1] = { ptr };
            juce::dsp::AudioBlock<float> blk (chans, 1, (size_t) len);
            auto up = os[osIdx - 1]->processSamplesUp (blk);
            float* u = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = distortSample (u[i]);
            os[osIdx - 1]->processSamplesDown (blk);
        }
    }

    // DC blocker, output gain, safety clip
    for (int i = 0; i < n; ++i)
    {
        const float x = m[i];
        hpY = x - hpX + 0.9995f * hpY;
        hpX = x;
        m[i] = juce::jlimit (-1.f, 1.f, hpY * outGain);
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, m, n);
}

juce::AudioProcessorEditor* BarrelKickProcessor::createEditor() { return new BarrelKickEditor (*this); }

void BarrelKickProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BarrelKickProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BarrelKickProcessor(); }
