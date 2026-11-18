// VectorGraphics.h — drawing recorded as vectors, written out as SVG or PDF
// (stage 10b: the graphic score).
//
// Recorder is a JUCE graphics context: what the timeline and the boards
// paint on screen is painted into it instead, and kept as filled paths,
// images and runs of text, in page coordinates (points, y down), with their
// clips. A colour mapping can turn the screen's dark colours light ("on
// paper"). toSvg writes one page as SVG, with text kept as text (in the
// fonts' families) so a graphics editor can change it; toPdf writes the
// pages as one PDF, with text as outlines, so no fonts need embedding.
// Neither needs a library.

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>

namespace Vector
{
    struct Op
    {
        enum class Kind { fill, image, text };
        Kind kind = Kind::fill;
        int group = 0;                          // index into Page::groups
        juce::Rectangle<float> clip;            // page coordinates; empty: none
        std::vector<int> clipPaths;             // indices into Page::clipPaths, intersected with clip

        // fill (also a text run's outline)
        juce::Path path;
        juce::Colour colour;
        bool gradient = false;
        juce::ColourGradient gradientFill;      // page coordinates

        // image
        juce::Image image;
        juce::AffineTransform imageTransform;   // image pixels to page
        float opacity = 1.0f;

        // text: the characters at their x positions on one baseline
        juce::String text;
        std::vector<float> xs;
        float baseline = 0.0f, fontSize = 0.0f, horizontalScale = 1.0f;
        juce::String family;
        bool bold = false, italic = false;
    };

    struct Page
    {
        float width = 0.0f, height = 0.0f;      // points
        juce::Colour background;
        std::vector<Op> ops;
        std::vector<juce::Path> clipPaths;
        juce::StringArray groups { "page" };
    };

    class Recorder : public juce::LowLevelGraphicsContext
    {
    public:
        using ColourMap = std::function<juce::Colour (juce::Colour)>;
        explicit Recorder (Page&, ColourMap = {});

        /** What follows goes in a group of this name (an SVG <g>). */
        void beginGroup (const juce::String& name);

        bool isVectorDevice() const override                        { return true; }
        void setOrigin (juce::Point<int>) override;
        void addTransform (const juce::AffineTransform&) override;
        float getPhysicalPixelScaleFactor() const override;
        bool clipToRectangle (const juce::Rectangle<int>&) override;
        bool clipToRectangleList (const juce::RectangleList<int>&) override;
        void excludeClipRectangle (const juce::Rectangle<int>&) override {}
        void clipToPath (const juce::Path&, const juce::AffineTransform&) override;
        void clipToImageAlpha (const juce::Image&, const juce::AffineTransform&) override {}
        bool clipRegionIntersects (const juce::Rectangle<int>&) override;
        juce::Rectangle<int> getClipBounds() const override;
        bool isClipEmpty() const override;
        void saveState() override;
        void restoreState() override;
        void beginTransparencyLayer (float opacity) override;
        void endTransparencyLayer() override;
        void setFill (const juce::FillType&) override;
        void setOpacity (float) override;
        void setImageBlendMode (juce::BlendMode) override {}
        void setInterpolationQuality (juce::Graphics::ResamplingQuality) override {}
        void fillRect (const juce::Rectangle<int>&, bool) override;
        void fillRect (const juce::Rectangle<float>&) override;
        void fillRectList (const juce::RectangleList<float>&) override;
        void fillPath (const juce::Path&, const juce::AffineTransform&) override;
        void drawImage (const juce::Image&, const juce::AffineTransform&) override;
        void drawLine (const juce::Line<float>&) override;
        void setFont (const juce::Font&) override;
        const juce::Font& getFont() override;
        void drawGlyphs (juce::Span<const uint16_t>, juce::Span<const juce::Point<float>>, const juce::AffineTransform&) override;
        std::unique_ptr<juce::ImageType> getPreferredImageTypeForTemporaryImages() const override;
        uint64_t getFrameId() const override                        { return 0; }

    private:
        struct State
        {
            juce::AffineTransform transform;
            juce::Rectangle<float> clip;        // page coordinates
            std::vector<int> clipPaths;
            juce::FillType fill;
            float opacity = 1.0f, layerOpacity = 1.0f;
            juce::Font font { juce::FontOptions() };
        };
        Page& page;
        ColourMap map;
        std::vector<State> stack;
        int group = 0;
        State& s()                                                  { return stack.back(); }
        const State& s() const                                      { return stack.back(); }
        void push (Op op, juce::Rectangle<float> bounds);
        juce::Colour colourNow (juce::Colour) const;
    };

    /** One page as an SVG document. */
    juce::String toSvg (const Page&);
    /** The pages as one PDF document. */
    juce::MemoryBlock toPdf (const std::vector<Page>&, const juce::String& title);

    /** "On paper": the screen's dark colours turned light, their hues kept. */
    juce::Colour paperColour (juce::Colour);
}
