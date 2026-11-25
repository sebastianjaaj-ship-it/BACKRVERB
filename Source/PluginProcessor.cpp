#include "PluginProcessor.h"
#include "PluginEditor.h"

static constexpr double kMaxCaptureSeconds = 20.0;
static constexpr int    kPreRoll = 1024;
static constexpr float  kTriggerLevel = 0.0056f;  // ~ -45 dBFS
static constexpr float  kSilenceLevel = 0.0018f;  // ~ -55 dBFS
static constexpr double kAutoStopSeconds = 1.2;
static constexpr int    kPeakBins = 600;

//==============================================================================
class BackrverbProcessor::Renderer : public juce::Thread
{
public:
    explicit Renderer (BackrverbProcessor& p) : juce::Thread ("BACKRVERB renderer"), proc (p) {}

    void run() override
    {
        brv::RenderSettings last;
        bool first = true;
        while (! threadShouldExit())
        {
            wait (40);
            if (threadShouldExit()) break;

            if (proc.captureState.load() == Finished)
                proc.takeFinishedCapture();

            if (! proc.hasDry.load()) { first = true; continue; }

            const auto s = proc.gatherSettings();
            if (first || proc.dirty.exchange (false) || s != last)
            {
                proc.renderNow (s);
                last = s;
                first = false;
            }
        }
    }

private:
    BackrverbProcessor& proc;
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BackrverbProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::source, 1 }, "Source", StringArray { "Vocal", "Instrument" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::length, 1 }, "Length", brv::lengthNames(), 2));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::size, 1 },  "Size",  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::tone, 1 },  "Tone",  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.56f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::tremolo, 1 }, "Tremolo", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::tremRate, 1 }, "Tremolo Rate", brv::tremRateNames(), 2));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::tremOn, 1 }, "Tremolo On", false));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::lowCut, 1 }, "Low Cut", false));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { ParamID::output, 1 }, "Output",
                     NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -1.2f,
                     AudioParameterFloatAttributes().withLabel ("dB")
                         .withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
    return { p.begin(), p.end() };
}

BackrverbProcessor::BackrverbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BACKRVERB", createLayout())
{
    captureBuf.setSize (2, (int) (kMaxCaptureSeconds * currentSR));
    preRoll.setSize (2, kPreRoll);
    preRoll.clear();
    renderer = std::make_unique<Renderer> (*this);
    renderer->startThread (juce::Thread::Priority::low);
}

BackrverbProcessor::~BackrverbProcessor()
{
    renderer->signalThreadShouldExit();
    renderer->notify();
    renderer->stopThread (4000);
}

bool BackrverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void BackrverbProcessor::prepareToPlay (double sampleRate, int)
{
    {
        const juce::ScopedLock sl (captureLock);
        if (sampleRate != currentSR || captureBuf.getNumSamples() == 0)
        {
            currentSR = sampleRate;
            captureBuf.setSize (2, (int) (kMaxCaptureSeconds * sampleRate));
        }
        if (captureState.load() == Recording || captureState.load() == Armed)
            captureState.store (Idle);
    }
    preRoll.clear();
    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));
    playMode = Stopped; playPos = 0; playModeUI.store (Stopped);
}

//==============================================================================
void BackrverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (nc == 0 || n == 0) return;

    // ---- host tempo / time signature
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { hostBpm.store (*b); hostHasBpm.store (true); }
            if (auto ts = pos->getTimeSignature()) { tsNum.store (ts->numerator); tsDen.store (ts->denominator); }
        }

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (nc > 1 ? 1 : 0);

    float lvl = 0.0f;
    for (int i = 0; i < n; ++i) lvl = juce::jmax (lvl, std::abs (inL[i]), std::abs (inR[i]));
    inputLevel.store (juce::jmax (lvl, inputLevel.load() * 0.85f));

    // ---- capture
    int state = captureState.load();
    if (state == Armed && stopRequested.exchange (false)) { captureState.store (Idle); state = Idle; }

    if (state == Armed || state == Recording)
    {
        const int cap = captureBuf.getNumSamples();
        int w = captureWrite.load();
        for (int i = 0; i < n; ++i)
        {
            const float l = inL[i], r = inR[i];
            const float a = juce::jmax (std::abs (l), std::abs (r));

            if (state == Armed)
            {
                preRoll.setSample (0, preRollPos, l);
                preRoll.setSample (1, preRollPos, r);
                preRollPos = (preRollPos + 1) % kPreRoll;
                if (a >= kTriggerLevel)
                {
                    // copy pre-roll (oldest first) so the attack is never lost
                    for (int k = 0; k < kPreRoll; ++k)
                    {
                        const int idx = (preRollPos + k) % kPreRoll;
                        captureBuf.setSample (0, k, preRoll.getSample (0, idx));
                        captureBuf.setSample (1, k, preRoll.getSample (1, idx));
                    }
                    w = kPreRoll;
                    silenceCount = 0;
                    state = Recording;
                    captureState.store (Recording);
                }
                continue;
            }

            if (w < cap)
            {
                captureBuf.setSample (0, w, l);
                captureBuf.setSample (1, w, r);
                ++w;
            }
            silenceCount = (a < kSilenceLevel) ? silenceCount + 1 : 0;

            if (stopRequested.load() || w >= cap
                || (autoStop.load() && silenceCount > (int) (kAutoStopSeconds * currentSR)))
            {
                stopRequested.store (false);
                captureWrite.store (w);
                captureState.store (Finished);
                state = Finished;
                if (renderer) renderer->notify();
                break;
            }
        }
        if (state != Finished) captureWrite.store (w);
    }

    // ---- playback (added on top of the dry signal)
    const int req = playRequest.exchange (-1);
    if (req >= 0) { playMode = req; playPos = 0; }

    outGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));

    if (playMode != Stopped)
    {
        const juce::SpinLock::ScopedTryLockType tl (renderLock);
        if (tl.isLocked())
        {
            const int sLen = swellBuf.getNumSamples();
            const int dLen = (playMode == PlayPreview) ? dryPlayBuf.getNumSamples() : 0;
            const int total = sLen + dLen;
            if (total == 0) playMode = Stopped;

            for (int i = 0; i < n && playMode != Stopped; ++i)
            {
                const float g = outGain.getNextValue();
                float l = 0, r = 0;
                if (playPos < sLen)          { l = swellBuf.getSample (0, playPos);          r = swellBuf.getSample (1, playPos); }
                else if (playPos < total)    { l = dryPlayBuf.getSample (0, playPos - sLen); r = dryPlayBuf.getSample (1, playPos - sLen); }

                if (nc > 1) { buffer.addSample (0, i, l * g); buffer.addSample (1, i, r * g); }
                else        { buffer.addSample (0, i, 0.5f * (l + r) * g); }

                if (++playPos >= total) playMode = Stopped;
            }
            playProgress.store (total > 0 ? (float) playPos / (float) total : 0.0f);
        }
    }
    else
    {
        outGain.skip (n);
    }
    playModeUI.store (playMode);
}

//==============================================================================
void BackrverbProcessor::toggleCapture()
{
    switch (captureState.load())
    {
        case Idle:
            stopRequested.store (false);
            captureWrite.store (0);
            playRequest.store (Stopped);
            captureState.store (Armed);
            break;
        case Armed:
        {
            int expected = Armed; // cancel directly (works even if no audio is flowing)
            if (! captureState.compare_exchange_strong (expected, Idle))
                stopRequested.store (true);
            break;
        }
        case Recording:
            stopRequested.store (true);
            break;
        default: break;
    }
}

void BackrverbProcessor::clearCapture()
{
    playRequest.store (Stopped);
    {
        const juce::ScopedLock sl (dryLock);
        dryCapture.setSize (0, 0);
        hasDry.store (false);
    }
    publish ({}, {});
}

brv::RenderSettings BackrverbProcessor::gatherSettings() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    brv::RenderSettings s;
    s.source        = (int) v (ParamID::source);
    s.lengthIndex   = (int) v (ParamID::length);
    s.size          = v (ParamID::size);
    s.tone          = v (ParamID::tone);
    s.width         = v (ParamID::width);
    s.tremDepth     = v (ParamID::tremolo);
    s.tremRateIndex = (int) v (ParamID::tremRate);
    s.tremOn        = v (ParamID::tremOn) > 0.5f;
    s.lowCut        = v (ParamID::lowCut) > 0.5f;
    s.normalize     = normalize.load();
    s.bpm           = hostBpm.load();
    s.tsNum         = tsNum.load();
    s.tsDen         = tsDen.load();
    s.sampleRate    = drySR;
    return s;
}

void BackrverbProcessor::takeFinishedCapture()
{
    juce::AudioBuffer<float> trimmed;
    double sr;
    {
        const juce::ScopedLock sl (captureLock);
        trimmed = brv::trimCapture (captureBuf, captureWrite.load());
        sr = currentSR;
    }
    if (trimmed.getNumSamples() > 64)
    {
        const juce::ScopedLock sl (dryLock);
        dryCapture = std::move (trimmed);
        drySR = sr;
        hasDry.store (true);
        dirty.store (true);
    }
    captureState.store (Idle);
}

void BackrverbProcessor::renderNow (const brv::RenderSettings& s)
{
    rendering.store (true);
    juce::AudioBuffer<float> dry;
    {
        const juce::ScopedLock sl (dryLock);
        dry.makeCopyOf (dryCapture);
    }
    auto swell = brv::renderSwell (dry, s);
    brv::normalizeTo (dry, 0.891f);
    publish (std::move (swell), std::move (dry));
    rendering.store (false);
}

void BackrverbProcessor::publish (juce::AudioBuffer<float>&& swell, juce::AudioBuffer<float>&& dryNorm)
{
    auto sp = brv::computePeaks (swell, kPeakBins);
    auto dp = brv::computePeaks (dryNorm, kPeakBins);
    {
        const juce::SpinLock::ScopedLockType sl (renderLock);
        std::swap (swellBuf, swell);
        std::swap (dryPlayBuf, dryNorm);
        std::swap (swellPeaks, sp);
        std::swap (dryPeaks, dp);
    }
    ++renderVersion;
    // old buffers are released here, on the renderer / message thread
}

BackrverbProcessor::DisplayData BackrverbProcessor::getDisplayData()
{
    DisplayData d;
    const juce::SpinLock::ScopedLockType sl (renderLock);
    d.swellPeaks = swellPeaks;
    d.dryPeaks = dryPeaks;
    d.swellLen = swellBuf.getNumSamples();
    d.dryLen = dryPlayBuf.getNumSamples();
    d.version = renderVersion.load();
    return d;
}

juce::String BackrverbProcessor::suggestedFileName() const
{
    const auto lenName = brv::lengthNames()[(int) apvts.getRawParameterValue (ParamID::length)->load()]
                             .replace ("/", "-").replace (" ", "");
    const auto src = apvts.getRawParameterValue (ParamID::source)->load() < 0.5f ? "Vocal" : "Inst";
    return "BACKRVERB_" + juce::String (src) + "_" + lenName + "_" + juce::String (juce::roundToInt (hostBpm.load())) + "bpm_"
           + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".wav";
}

bool BackrverbProcessor::exportRender (const juce::File& file)
{
    juce::AudioBuffer<float> out;
    double sr;
    {
        const juce::SpinLock::ScopedLockType sl (renderLock);
        out.makeCopyOf (swellBuf);
    }
    {
        const juce::ScopedLock sl (dryLock);
        sr = drySR;
    }
    if (out.getNumSamples() == 0) return false;

    out.applyGain (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));
    for (int c = 0; c < out.getNumChannels(); ++c)
        juce::FloatVectorOperations::clip (out.getWritePointer (c), out.getReadPointer (c), -1.0f, 1.0f, out.getNumSamples());

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    os.release();
    return writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
}

//==============================================================================
void BackrverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("normalize", normalize.load(), nullptr);
    state.setProperty ("autoStop", autoStop.load(), nullptr);

    state.removeChild (state.getChildWithName ("CAPTURE"), nullptr);
    {
        const juce::ScopedLock sl (dryLock);
        if (hasDry.load() && dryCapture.getNumSamples() > 0)
        {
            const int len = dryCapture.getNumSamples();
            juce::MemoryBlock mb ((size_t) len * 2 * sizeof (int16_t));
            auto* d = static_cast<int16_t*> (mb.getData());
            for (int i = 0; i < len; ++i)
                for (int c = 0; c < 2; ++c)
                    d[i * 2 + c] = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (dryCapture.getSample (c, i) * 32767.0f));

            juce::ValueTree cap ("CAPTURE");
            cap.setProperty ("sr", drySR, nullptr);
            cap.setProperty ("len", len, nullptr);
            cap.setProperty ("pcm", juce::var (mb), nullptr);
            state.appendChild (cap, nullptr);
        }
    }

    juce::MemoryOutputStream mos (destData, false);
    state.writeToStream (mos);
}

void BackrverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;

    normalize.store ((bool) state.getProperty ("normalize", true));
    autoStop.store ((bool) state.getProperty ("autoStop", true));

    auto cap = state.getChildWithName ("CAPTURE");
    if (cap.isValid())
    {
        const int len = (int) cap.getProperty ("len", 0);
        if (auto* mb = cap.getProperty ("pcm").getBinaryData(); mb != nullptr && len > 0
                && mb->getSize() >= (size_t) len * 2 * sizeof (int16_t))
        {
            juce::AudioBuffer<float> b (2, len);
            auto* d = static_cast<const int16_t*> (mb->getData());
            for (int i = 0; i < len; ++i)
                for (int c = 0; c < 2; ++c)
                    b.setSample (c, i, d[i * 2 + c] / 32767.0f);

            const juce::ScopedLock sl (dryLock);
            dryCapture = std::move (b);
            drySR = (double) cap.getProperty ("sr", 48000.0);
            hasDry.store (true);
            dirty.store (true);
        }
        state.removeChild (cap, nullptr);
    }

    apvts.replaceState (state);
}

juce::AudioProcessorEditor* BackrverbProcessor::createEditor() { return new BackrverbEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BackrverbProcessor(); }
