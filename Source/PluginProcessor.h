#pragma once
#include <JuceHeader.h>
#include "ReverseEngine.h"

namespace ParamID
{
    inline constexpr const char* source    = "source";
    inline constexpr const char* length    = "length";
    inline constexpr const char* size      = "size";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* width     = "width";
    inline constexpr const char* tremolo   = "tremolo";
    inline constexpr const char* tremRate  = "tremRate";
    inline constexpr const char* tremOn    = "tremOn";
    inline constexpr const char* lowCut    = "lowCut";
    inline constexpr const char* output    = "output";
}

class BackrverbProcessor : public juce::AudioProcessor
{
public:
    enum CaptureState { Idle = 0, Armed = 1, Recording = 2, Finished = 3 };
    enum PlayMode     { Stopped = 0, PlaySwell = 1, PlayPreview = 2 };

    BackrverbProcessor();
    ~BackrverbProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BACKRVERB"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    //== UI API (message thread) ================================================
    void toggleCapture();
    void requestPlay (PlayMode m)            { playRequest.store ((int) m); }
    void clearCapture();
    bool hasCapture() const                  { return hasDry.load(); }
    int  getCaptureState() const             { return captureState.load(); }
    int  getPlayMode() const                 { return playModeUI.load(); }
    float getPlayProgress() const            { return playProgress.load(); }
    double getHostBpm() const                { return hostBpm.load(); }
    bool hostProvidesBpm() const             { return hostHasBpm.load(); }
    int  getTsNum() const                    { return tsNum.load(); }
    int  getTsDen() const                    { return tsDen.load(); }
    float getInputLevel() const              { return inputLevel.load(); }
    bool isRendering() const                 { return rendering.load(); }

    std::atomic<bool> normalize { true }, autoStop { true };

    struct DisplayData
    {
        std::vector<float> swellPeaks, dryPeaks;
        int swellLen = 0, dryLen = 0;
        int version = 0;
    };
    int  getRenderVersion() const { return renderVersion.load(); }
    DisplayData getDisplayData();

    /** Writes the current render (with OUTPUT gain applied) as a 24-bit WAV. */
    bool exportRender (const juce::File& file);
    juce::String suggestedFileName() const;

private:
    class Renderer;
    friend class Renderer;

    brv::RenderSettings gatherSettings() const;
    void takeFinishedCapture();
    void renderNow (const brv::RenderSettings&);
    void publish (juce::AudioBuffer<float>&& swell, juce::AudioBuffer<float>&& dryNorm);

    // capture (audio thread writes; renderer reads after Finished)
    juce::CriticalSection captureLock;
    juce::AudioBuffer<float> captureBuf, preRoll;
    int preRollPos = 0;
    std::atomic<int> captureWrite { 0 }, captureState { Idle };
    std::atomic<bool> stopRequested { false };
    int silenceCount = 0;
    double currentSR = 48000.0;

    // dry material used for re-rendering
    juce::CriticalSection dryLock;
    juce::AudioBuffer<float> dryCapture;
    double drySR = 48000.0;
    std::atomic<bool> hasDry { false }, dirty { false }, rendering { false };

    // published render
    juce::SpinLock renderLock;
    juce::AudioBuffer<float> swellBuf, dryPlayBuf;
    std::vector<float> swellPeaks, dryPeaks;
    std::atomic<int> renderVersion { 0 };

    // playback
    std::atomic<int> playRequest { -1 }, playModeUI { Stopped };
    std::atomic<float> playProgress { 0.0f };
    int playMode = Stopped, playPos = 0;
    juce::SmoothedValue<float> outGain;

    // host info
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostHasBpm { false };
    std::atomic<int> tsNum { 4 }, tsDen { 4 };
    std::atomic<float> inputLevel { 0.0f };

    std::unique_ptr<Renderer> renderer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackrverbProcessor)
};
