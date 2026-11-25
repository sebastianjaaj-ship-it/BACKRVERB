// Headless test host: simulates a capture, checks the rendered reverse-reverb swell,
// playback, export, state save/restore, and renders the editor to PNG.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void setParam (BackrverbProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void runBlocks (BackrverbProcessor& p, int blocks, std::function<float (int)> gen, float* maxOut = nullptr)
{
    static int t = 0;
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer m;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i) { const float s = gen ? gen (t + i) : 0.0f; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        t += 512;
        p.processBlock (buf, m);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                if (! std::isfinite (buf.getSample (c, i))) { std::printf ("NON-FINITE OUTPUT!\n"); std::exit (1); }
                if (maxOut) *maxOut = juce::jmax (*maxOut, std::abs (buf.getSample (c, i)));
            }
    }
}

static bool waitFor (std::function<bool()> cond, int ms = 4000)
{
    for (int i = 0; i < ms / 10; ++i) { if (cond()) return true; juce::Thread::sleep (10); }
    return cond();
}

static void snapshot (BackrverbProcessor& p, const juce::String& path)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (640, 660);
    juce::ignoreUnused (0);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (path);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const double fs = 48000.0;
    BackrverbProcessor p;
    p.setPlayConfigDetails (2, 2, fs, 512);
    p.prepareToPlay (fs, 512);

    snapshot (p, outDir + "/editor_empty.png");

    // ---- capture: silence, then a "snare" (noise + tone, 250 ms decay), then silence -> auto-stop
    p.toggleCapture();
    std::printf ("state after arm: %d (expect 1)\n", p.getCaptureState());
    juce::Random rnd (1);
    runBlocks (p, 20, nullptr);
    const int hitStart = 0;
    juce::ignoreUnused (hitStart);
    int t0 = -1;
    runBlocks (p, 40, [&] (int t)
    {
        if (t0 < 0) t0 = t;
        const double tt = (t - t0) / fs;
        return (float) (0.8 * std::exp (-tt * 14.0) * (0.6 * (rnd.nextFloat() * 2 - 1) + 0.4 * std::sin (2 * juce::MathConstants<double>::pi * 190 * tt)));
    });
    std::printf ("state during sound: %d (expect 2)\n", p.getCaptureState());
    runBlocks (p, (int) (1.6 * fs / 512), nullptr);
    std::printf ("state after silence: %d (expect 3 or 0)\n", p.getCaptureState());

    if (! waitFor ([&] { return p.hasCapture() && p.getDisplayData().swellLen > 0 && ! p.isRendering(); }))
    { std::printf ("FAIL: no render\n"); return 1; }

    auto d = p.getDisplayData();
    std::printf ("swell len %d (expect %d = 1 bar @120), dry len %d\n", d.swellLen, (int) (2.0 * fs), d.dryLen);

    auto quarterEnergy = [&] (const std::vector<float>& pk, int q)
    {
        const int n = (int) pk.size(); float e = 0;
        for (int i = q * n / 4; i < (q + 1) * n / 4; ++i) e = juce::jmax (e, pk[(size_t) i]);
        return e;
    };
    const float q0 = quarterEnergy (d.swellPeaks, 0), q3 = quarterEnergy (d.swellPeaks, 3);
    std::printf ("swell peaks: first quarter %.3f, last quarter %.3f -> %s\n", q0, q3, q3 > q0 * 2 ? "RISING OK" : "NOT RISING!");
    const float lastBin = d.swellPeaks[d.swellPeaks.size() - 3];
    std::printf ("level just before hit %.3f\n", lastBin);

    // ---- lengths follow the parameter
    for (int li = 0; li < 5; ++li)
    {
        setParam (p, ParamID::length, (float) li);
        const int expect = (int) std::round (brv::lengthInBeats (li, 4, 4) * 0.5 * fs);
        waitFor ([&] { return p.getDisplayData().swellLen == expect && ! p.isRendering(); });
        std::printf ("length %s -> %d samples (expect %d)\n", brv::lengthNames()[li].toRawUTF8(), p.getDisplayData().swellLen, expect);
    }
    setParam (p, ParamID::length, 2);

    // ---- every control renders finite audio
    const int v0 = p.getRenderVersion();
    setParam (p, ParamID::source, 1); setParam (p, ParamID::size, 1.0f); setParam (p, ParamID::tone, 0.0f);
    setParam (p, ParamID::width, 1.0f); setParam (p, ParamID::tremOn, 1); setParam (p, ParamID::tremolo, 1.0f);
    setParam (p, ParamID::lowCut, 1);
    waitFor ([&] { return p.getRenderVersion() > v0 && ! p.isRendering(); });
    juce::Thread::sleep (150);
    waitFor ([&] { return ! p.isRendering(); });
    std::printf ("re-rendered with all controls changed (version %d -> %d)\n", v0, p.getRenderVersion());

    // ---- playback
    p.requestPlay (BackrverbProcessor::PlaySwell);
    float mx = 0;
    runBlocks (p, 10, nullptr, &mx);
    std::printf ("play: mode %d, output peak %.3f\n", p.getPlayMode(), mx);
    runBlocks (p, (int) (2.2 * fs / 512), nullptr);
    std::printf ("play stopped automatically: %s\n", p.getPlayMode() == 0 ? "yes" : "NO");
    p.requestPlay (BackrverbProcessor::PlayPreview);
    runBlocks (p, 5, nullptr);
    std::printf ("preview mode %d\n", p.getPlayMode());
    p.requestPlay (BackrverbProcessor::Stopped);
    runBlocks (p, 1, nullptr);

    // ---- export
    juce::File wav (outDir + "/export_test.wav");
    std::printf ("export ok: %d\n", (int) p.exportRender (wav));
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (wav));
        if (r) std::printf ("wav: %lld samples, %d ch, %d bit, %.0f Hz\n", (long long) r->lengthInSamples, (int) r->numChannels, (int) r->bitsPerSample, r->sampleRate);
    }

    // ---- state save / restore keeps the capture
    juce::MemoryBlock st;
    p.getStateInformation (st);
    std::printf ("state size %d bytes\n", (int) st.getSize());
    {
        BackrverbProcessor p2;
        p2.setPlayConfigDetails (2, 2, fs, 512);
        p2.prepareToPlay (fs, 512);
        p2.setStateInformation (st.getData(), (int) st.getSize());
        const bool ok = waitFor ([&] { return p2.getDisplayData().swellLen > 0 && ! p2.isRendering(); });
        std::printf ("restored capture: %s, size param %.2f, source %d\n", ok ? "yes" : "NO",
                     p2.apvts.getRawParameterValue (ParamID::size)->load(), (int) p2.apvts.getRawParameterValue (ParamID::source)->load());
    }

    // nice-looking defaults for the screenshot
    setParam (p, ParamID::source, 0); setParam (p, ParamID::size, 0.5f); setParam (p, ParamID::tone, 0.5f);
    setParam (p, ParamID::width, 0.56f); setParam (p, ParamID::tremOn, 0); setParam (p, ParamID::tremolo, 0.5f);
    setParam (p, ParamID::lowCut, 0);
    juce::Thread::sleep (100);
    waitFor ([&] { return ! p.isRendering(); });
    snapshot (p, outDir + "/editor_captured.png");
    p.clearCapture();
    std::printf ("cleared: hasCapture=%d\n", (int) p.hasCapture());
    return 0;
}
