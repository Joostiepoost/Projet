// FactorCurves.h — drawing variation factor curves (stage 8): the editor in
// the Framework tab, and the same editing over a box on the timeline.
//
// Click to add a point (and drag it), drag a point to move it, double-click
// a point to remove it; Alt-drag draws freehand. Right-click: a straight line
// from that point (or a smooth curve again), shapes to start from, Reset.

#pragma once

#include <JuceHeader.h>
#include "../Model/Factors.h"

namespace FactorCurves
{
    /** Drawing a curve in `area` (t across, the value upwards). */
    void paint (juce::Graphics&, juce::Rectangle<float> area, const Factors::Curve&, double neutral,
                juce::Colour colour, bool withPoints, int hotPoint = -1);

    /** Editing in `area`: the state of one drag. */
    struct Edit
    {
        juce::Rectangle<float> area;
        Factors::Curve curve;
        int dragging = -1;                          // the point being moved
        bool freehand = false;
        juce::Array<juce::Point<float>> trail;      // a freehand drag

        juce::Point<float> toScreen (const Factors::Point& p) const;
        Factors::Point fromScreen (juce::Point<float> s) const;
        int pointAt (juce::Point<float> s) const;
        /** Mouse down: a point to move, a new one, or a freehand start. Returns true if the curve changed. */
        bool down (juce::Point<float> s, bool alt);
        bool drag (juce::Point<float> s);
        /** Mouse up: a freehand trail becomes points. */
        bool up();
        bool doubleClick (juce::Point<float> s);
    };

    /** The right-click menu (a point's straight line, shapes, Reset); `done` gets the new curve. */
    void showMenu (const Factors::Curve&, int pointIndex, double neutral, juce::Component* target,
                   std::function<void (const Factors::Curve&)> done);

    /** One factor's curve in the Framework tab: its name, its two ends, the curve. */
    class Editor : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Editor (const juce::String& name, const juce::String& low, const juce::String& high, double neutral, juce::Colour);
        void setCurve (const Factors::Curve& c)     { edit.curve = c; repaint(); }
        const Factors::Curve& getCurve() const      { return edit.curve; }
        /** A change: `starting` on the first change of a gesture (a new undo step). */
        std::function<void (const Factors::Curve&, bool starting)> onChange;
        bool isDragging() const                     { return isMouseButtonDown(); }

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        static constexpr int preferredHeight = 46;

    private:
        juce::String name, low, high;
        double neutral;
        juce::Colour colour;
        Edit edit;
        bool started = false;
        juce::Rectangle<float> curveArea() const;
        void changed();
    };
}
