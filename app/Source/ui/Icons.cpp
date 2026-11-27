#include "Icons.h"
#include <cmath>

namespace ui::icons {

namespace {
juce::PathStrokeType stroke(float w) { return { w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded }; }

juce::Path wave(juce::Rectangle<float> r, float cycles, float amp, float phase = 0.0f) {
    juce::Path p;
    const int n = 40;
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / n;
        const float x = r.getX() + t * r.getWidth();
        const float y = r.getCentreY() - amp * r.getHeight() * 0.5f * std::sin(6.2831853f * (cycles * t + phase));
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    return p;
}
} // namespace

void drawEffect(juce::Graphics& g, const std::string& id, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    const float sw = juce::jmax(1.5f, r.getWidth() * 0.09f);
    const float x0 = r.getX(), y0 = r.getY(), w = r.getWidth(), h = r.getHeight();
    juce::Path p;
    if (id == "eq") {
        // Tre reglage med knoppar på olika höjd.
        const float knob[3] = { 0.65f, 0.3f, 0.5f };
        for (int i = 0; i < 3; ++i) {
            const float x = x0 + w * (0.2f + 0.3f * static_cast<float>(i));
            p.startNewSubPath(x, y0 + h * 0.1f); p.lineTo(x, y0 + h * 0.9f);
            g.fillEllipse(x - sw * 1.4f, y0 + h * knob[i] - sw * 1.4f, sw * 2.8f, sw * 2.8f);
        }
    } else if (id == "sustain") {
        // Kuvert: snabbt anslag, lång ton.
        p.startNewSubPath(x0 + w * 0.05f, y0 + h * 0.85f);
        p.lineTo(x0 + w * 0.22f, y0 + h * 0.18f);
        p.lineTo(x0 + w * 0.35f, y0 + h * 0.38f);
        p.lineTo(x0 + w * 0.8f, y0 + h * 0.42f);
        p.lineTo(x0 + w * 0.95f, y0 + h * 0.85f);
    } else if (id == "chorus") {
        g.strokePath(wave(r.reduced(w * 0.05f, h * 0.2f), 1.5f, 0.7f), stroke(sw));
        p = wave(r.reduced(w * 0.05f, h * 0.2f), 1.5f, 0.7f, 0.18f);
        g.setColour(c.withAlpha(0.5f));
    } else if (id == "flanger") {
        p = wave(r.reduced(w * 0.05f, h * 0.15f), 2.5f, 0.55f);
        g.strokePath(p, stroke(sw));
        p.clear();
        p.startNewSubPath(x0 + w * 0.1f, y0 + h * 0.92f); p.lineTo(x0 + w * 0.9f, y0 + h * 0.08f);
        g.setColour(c.withAlpha(0.5f));
    } else if (id == "widener") {
        // Pilar åt båda håll.
        const float cy = y0 + h * 0.5f;
        p.startNewSubPath(x0 + w * 0.08f, cy); p.lineTo(x0 + w * 0.92f, cy);
        p.startNewSubPath(x0 + w * 0.28f, cy - h * 0.2f); p.lineTo(x0 + w * 0.08f, cy); p.lineTo(x0 + w * 0.28f, cy + h * 0.2f);
        p.startNewSubPath(x0 + w * 0.72f, cy - h * 0.2f); p.lineTo(x0 + w * 0.92f, cy); p.lineTo(x0 + w * 0.72f, cy + h * 0.2f);
        g.fillEllipse(x0 + w * 0.5f - sw, y0 + h * 0.2f - sw, sw * 2, sw * 2);
        g.fillEllipse(x0 + w * 0.5f - sw, y0 + h * 0.8f - sw, sw * 2, sw * 2);
    } else if (id == "reverb") {
        // Punkt med vågfronter som breder ut sig.
        const auto o = juce::Point<float>(x0 + w * 0.2f, y0 + h * 0.5f);
        g.fillEllipse(o.x - sw * 1.2f, o.y - sw * 1.2f, sw * 2.4f, sw * 2.4f);
        for (int i = 1; i <= 3; ++i) {
            const float rad = w * 0.24f * static_cast<float>(i);
            juce::Path arc;
            arc.addCentredArc(o.x, o.y, rad, rad, 0.0f, 0.6f, 2.55f, true);
            g.setColour(c.withAlpha(1.0f - 0.22f * static_cast<float>(i - 1)));
            g.strokePath(arc, stroke(sw));
        }
        return;
    } else if (id == "distortion") {
        // Avklippt sinus.
        const float top = y0 + h * 0.25f, bot = y0 + h * 0.75f;
        p.startNewSubPath(x0 + w * 0.05f, y0 + h * 0.5f);
        p.lineTo(x0 + w * 0.18f, top); p.lineTo(x0 + w * 0.4f, top); p.lineTo(x0 + w * 0.6f, bot);
        p.lineTo(x0 + w * 0.82f, bot); p.lineTo(x0 + w * 0.95f, y0 + h * 0.5f);
    } else if (id == "cabinet") {
        const auto box = r.reduced(w * 0.12f, h * 0.06f);
        p.addRoundedRectangle(box, w * 0.12f);
        g.strokePath(p, stroke(sw));
        p.clear();
        const float rr = box.getWidth() * 0.3f;
        p.addEllipse(box.getCentreX() - rr, box.getY() + box.getHeight() * 0.58f - rr, rr * 2, rr * 2);
        g.fillEllipse(box.getCentreX() - sw, box.getY() + box.getHeight() * 0.2f - sw, sw * 2, sw * 2);
    } else if (id == "body") {
        // Gitarrkropp: liten övre bukt, midja, större nedre bukt och ljudhål.
        const float cx = x0 + w * 0.5f;
        p.addEllipse(cx - w * 0.24f, y0 + h * 0.04f, w * 0.48f, h * 0.42f);
        p.addEllipse(cx - w * 0.34f, y0 + h * 0.36f, w * 0.68f, h * 0.6f);
        g.strokePath(p, stroke(sw));
        p.clear();
        g.fillEllipse(cx - sw * 1.5f, y0 + h * 0.6f - sw * 1.5f, sw * 3.0f, sw * 3.0f);
        return;
    } else if (id == "feedback") {
        // Plan linje med en smal notch nedåt.
        const float cy = y0 + h * 0.4f;
        p.startNewSubPath(x0 + w * 0.05f, cy);
        p.lineTo(x0 + w * 0.36f, cy);
        p.lineTo(x0 + w * 0.5f, y0 + h * 0.9f);
        p.lineTo(x0 + w * 0.64f, cy);
        p.lineTo(x0 + w * 0.95f, cy);
    } else if (id == "doubler") {
        // Två överlappande cirklar.
        const float rr = w * 0.26f;
        p.addEllipse(x0 + w * 0.36f - rr, y0 + h * 0.5f - rr, rr * 2, rr * 2);
        p.addEllipse(x0 + w * 0.64f - rr, y0 + h * 0.5f - rr, rr * 2, rr * 2);
    } else if (id == "tremolo") {
        // Våg vars amplitud växer och avtar.
        const int n = 48;
        for (int i = 0; i <= n; ++i) {
            const float t = static_cast<float>(i) / n;
            const float env = 0.25f + 0.75f * std::sin(3.14159265f * t);
            const float y = y0 + h * 0.5f - env * h * 0.4f * std::sin(6.2831853f * 3.0f * t);
            if (i == 0) p.startNewSubPath(x0 + w * (0.05f + 0.9f * t), y); else p.lineTo(x0 + w * (0.05f + 0.9f * t), y);
        }
    } else if (id == "delay") {
        // Ekon: prickar som blir mindre och svagare.
        for (int i = 0; i < 4; ++i) {
            const float rr = sw * (1.7f - 0.3f * static_cast<float>(i));
            g.setColour(c.withAlpha(1.0f - 0.22f * static_cast<float>(i)));
            g.fillEllipse(x0 + w * (0.14f + 0.24f * static_cast<float>(i)) - rr, y0 + h * 0.5f - rr, rr * 2, rr * 2);
        }
        return;
    } else if (id == "volume") {
        // Stigande staplar.
        for (int i = 0; i < 4; ++i) {
            const float x = x0 + w * (0.17f + 0.22f * static_cast<float>(i));
            p.startNewSubPath(x, y0 + h * 0.9f);
            p.lineTo(x, y0 + h * (0.68f - 0.18f * static_cast<float>(i)));
        }
    }
    g.strokePath(p, stroke(sw));
}

void draw(juce::Graphics& g, Symbol s, juce::Rectangle<float> r, juce::Colour c, float sw) {
    g.setColour(c);
    const auto ctr = r.getCentre();
    const float k = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
    juce::Path p;
    switch (s) {
        case Symbol::Particles: {   // punktmoln (skärmsläckare)
            const float pts[7][3] = { { -0.55f, -0.35f, 0.16f }, { 0.05f, -0.6f, 0.11f }, { 0.55f, -0.25f, 0.14f }, { -0.2f, 0.05f, 0.2f },
                                      { 0.4f, 0.35f, 0.12f }, { -0.55f, 0.5f, 0.1f }, { 0.1f, 0.6f, 0.15f } };
            for (const auto& q : pts) g.fillEllipse(ctr.x + q[0] * k - q[2] * k, ctr.y + q[1] * k - q[2] * k, q[2] * k * 2, q[2] * k * 2);
            return;
        }
        case Symbol::Dot:
            g.fillEllipse(ctr.x - k * 0.4f, ctr.y - k * 0.4f, k * 0.8f, k * 0.8f);
            return;
        case Symbol::ArrowUp:
            p.startNewSubPath(ctr.x, ctr.y + k * 0.7f); p.lineTo(ctr.x, ctr.y - k * 0.7f);
            p.startNewSubPath(ctr.x - k * 0.5f, ctr.y - k * 0.2f); p.lineTo(ctr.x, ctr.y - k * 0.7f); p.lineTo(ctr.x + k * 0.5f, ctr.y - k * 0.2f);
            break;
        case Symbol::More:
            for (int i = -1; i <= 1; ++i) g.fillEllipse(ctr.x + static_cast<float>(i) * k * 0.6f - 2.0f, ctr.y - 2.0f, 4.0f, 4.0f);
            return;
        case Symbol::Plus:
            p.startNewSubPath(ctr.x - k * 0.6f, ctr.y); p.lineTo(ctr.x + k * 0.6f, ctr.y);
            p.startNewSubPath(ctr.x, ctr.y - k * 0.6f); p.lineTo(ctr.x, ctr.y + k * 0.6f);
            break;
        case Symbol::Prev:
            p.startNewSubPath(ctr.x + k * 0.25f, ctr.y - k * 0.5f); p.lineTo(ctr.x - k * 0.25f, ctr.y); p.lineTo(ctr.x + k * 0.25f, ctr.y + k * 0.5f);
            break;
        case Symbol::Next:
            p.startNewSubPath(ctr.x - k * 0.25f, ctr.y - k * 0.5f); p.lineTo(ctr.x + k * 0.25f, ctr.y); p.lineTo(ctr.x - k * 0.25f, ctr.y + k * 0.5f);
            break;
        case Symbol::ChevronDown:
            p.startNewSubPath(ctr.x - k * 0.45f, ctr.y - k * 0.2f); p.lineTo(ctr.x, ctr.y + k * 0.25f); p.lineTo(ctr.x + k * 0.45f, ctr.y - k * 0.2f);
            break;
        case Symbol::ChevronUp:
            p.startNewSubPath(ctr.x - k * 0.45f, ctr.y + k * 0.2f); p.lineTo(ctr.x, ctr.y - k * 0.25f); p.lineTo(ctr.x + k * 0.45f, ctr.y + k * 0.2f);
            break;
        case Symbol::Check:
            p.startNewSubPath(ctr.x - k * 0.45f, ctr.y); p.lineTo(ctr.x - k * 0.1f, ctr.y + k * 0.35f); p.lineTo(ctr.x + k * 0.5f, ctr.y - k * 0.35f);
            break;
        case Symbol::Sliders: {
            const float knobs[3] = { 0.3f, -0.35f, 0.1f };
            for (int i = 0; i < 3; ++i) {
                const float y = ctr.y + static_cast<float>(i - 1) * k * 0.5f;
                p.startNewSubPath(ctr.x - k * 0.65f, y); p.lineTo(ctr.x + k * 0.65f, y);
                g.fillEllipse(ctr.x + knobs[i] * k - 3.0f, y - 3.0f, 6.0f, 6.0f);
            }
            break;
        }
        case Symbol::Waveform: {
            // Ljudvåg (staplar av olika höjd) – för låtanalys.
            const float h[5] = { 0.35f, 0.75f, 1.0f, 0.6f, 0.3f };
            for (int i = 0; i < 5; ++i) {
                const float x = ctr.x + (static_cast<float>(i) - 2.0f) * k * 0.36f;
                p.startNewSubPath(x, ctr.y - h[i] * k * 0.62f);
                p.lineTo(x, ctr.y + h[i] * k * 0.62f);
            }
            break;
        }
        case Symbol::Speaker:
            p.startNewSubPath(ctr.x - k * 0.6f, ctr.y - k * 0.2f); p.lineTo(ctr.x - k * 0.3f, ctr.y - k * 0.2f);
            p.lineTo(ctr.x + k * 0.05f, ctr.y - k * 0.55f); p.lineTo(ctr.x + k * 0.05f, ctr.y + k * 0.55f);
            p.lineTo(ctr.x - k * 0.3f, ctr.y + k * 0.2f); p.lineTo(ctr.x - k * 0.6f, ctr.y + k * 0.2f); p.closeSubPath();
            break;
    }
    g.strokePath(p, stroke(sw));
}

} // namespace ui::icons
