#include "StemSplitterLibrary.h"

namespace stems {

juce::File projectsRoot() {
   #if JUCE_IOS
    return {};   // iPadOS: appar kan inte läsa varandras filer
   #else
    // StemSplitter finns bara för Mac; på Linux finns mappen normalt inte (används i tester).
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Application Support/StemSplitter/projects");
   #endif
}

std::vector<Project> listProjects() {
    std::vector<Project> out;
    const auto root = projectsRoot();
    if (root == juce::File() || !root.isDirectory()) return out;
    for (const auto& dir : root.findChildFiles(juce::File::findDirectories, false)) {
        if (dir.getFileName().startsWith(".")) continue;   // StemSplitters halvfärdiga ".staging-"
        bool complete = true;
        for (const char* s : { "vocals", "drums", "bass", "other" })
            complete = complete && dir.getChildFile(juce::String(s) + ".wav").existsAsFile();
        if (!complete) continue;
        Project p;
        p.key = dir.getFileName();
        p.dir = dir;
        p.name = juce::String::fromUTF8("Okänd låt");
        p.created = dir.getCreationTime();
        const auto json = juce::JSON::parse(dir.getChildFile("project.json"));
        if (json.isObject()) {
            const auto src = json.getProperty("sourceName", "").toString();
            if (src.isNotEmpty()) p.name = src.upToLastOccurrenceOf(".", false, false).isNotEmpty()
                                            ? src.upToLastOccurrenceOf(".", false, false) : src;
            const auto created = json.getProperty("createdAt", "").toString();
            if (created.isNotEmpty()) p.created = juce::Time::fromISO8601(created);
            if (const auto* grid = json.getProperty("beatGrid", {}).getDynamicObject())
                p.bpm = static_cast<float>(static_cast<double>(grid->getProperty("bpm")));
        }
        out.push_back(p);
    }
    std::sort(out.begin(), out.end(), [](const Project& a, const Project& b) { return a.created > b.created; });
    return out;
}

namespace {
// Läser en stämma som stereo (mono dupliceras), högst maxSeconds.
bool readStem(juce::AudioFormatManager& fm, const juce::File& f, double maxSeconds,
              juce::AudioBuffer<float>& buf, double& rate) {
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr) return false;
    rate = r->sampleRate;
    const int n = static_cast<int>(juce::jmin(r->lengthInSamples, static_cast<juce::int64>(r->sampleRate * maxSeconds)));
    if (n <= 0) return false;
    buf.setSize(2, n);
    r->read(&buf, 0, n, 0, true, true);
    if (r->numChannels < 2) buf.copyFrom(1, 0, buf, 0, 0, n);
    return true;
}
} // namespace

Result analyse(const Project& p, const std::function<bool(float)>& progress) {
    Result res;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::AudioBuffer<float> band, stem;
    double rate = 48000.0, r2 = 48000.0;
    // Bandet utan gitarr/klaviatur: bas + trummor + sång.
    const char* bandStems[] = { "bass", "drums", "vocals" };
    for (int i = 0; i < 3; ++i) {
        if (!readStem(fm, p.dir.getChildFile(juce::String(bandStems[i]) + ".wav"), 360.0, stem, r2)) {
            res.error = juce::String::fromUTF8("Kunde inte läsa stämmorna från StemSplitter.");
            return res;
        }
        if (i == 0) { band.makeCopyOf(stem); rate = r2; }
        else {
            const int n = juce::jmin(band.getNumSamples(), stem.getNumSamples());
            band.setSize(2, n, true);
            for (int ch = 0; ch < 2; ++ch) band.addFrom(ch, 0, stem, ch, 0, n);
        }
        if (progress && !progress(0.1f * static_cast<float>(i + 1))) return res;
    }
    auto sub = [&](float from, float to) {
        return [&progress, from, to](float x) { return !progress || progress(from + (to - from) * x); };
    };
    const auto bandA = gfx::analyseSong(band.getReadPointer(0), band.getReadPointer(1), band.getNumSamples(), rate, sub(0.3f, 0.7f));
    if (!bandA.valid) { res.error = juce::String::fromUTF8("Stämmorna är för korta eller tysta för att analysera."); return res; }
    // Kompet ("övrigt"): rummet i inspelningen.
    gfx::SongAnalysis compA;
    if (readStem(fm, p.dir.getChildFile("other.wav"), 360.0, stem, r2))
        compA = gfx::analyseSong(stem.getReadPointer(0), stem.getReadPointer(1), stem.getNumSamples(), r2, sub(0.7f, 1.0f));
    res.fit = gfx::fitGuitarToStems(bandA, compA, p.bpm);
    res.seconds = bandA.seconds;
    res.bpm = p.bpm > 0.0f ? p.bpm : bandA.bpm;
    res.valid = true;
    return res;
}

} // namespace stems
