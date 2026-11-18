// VectorGraphics.cpp

#include "VectorGraphics.h"

namespace Vector
{

juce::Colour paperColour (juce::Colour c)
{
    return juce::Colour::fromHSL (c.getHue(), c.getSaturationHSL(), 1.0f - c.getLightness(), c.getFloatAlpha());
}

//== Recorder ==================================================================

namespace
{
    /** Images made smaller for the page, by source and size: drawn again, the same image. */
    struct ScaledImage { juce::Image image; bool opaque = true; };
    std::map<std::tuple<const void*, int, int>, ScaledImage>& imageCache()
    {
        static std::map<std::tuple<const void*, int, int>, ScaledImage> cache;
        return cache;
    }

    bool isOpaque (const juce::Image& image)
    {
        if (! image.hasAlphaChannel()) return true;
        const juce::Image::BitmapData bits (image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (bits.getPixelColour (x, y).getAlpha() < 255) return false;
        return true;
    }
}

void Recorder::clearImageCache()
{
    imageCache().clear();
}

Recorder::Recorder (Page& p, ColourMap m, float imageDpi) : page (p), map (std::move (m)), dpi (juce::jmax (36.0f, imageDpi))
{
    State first;
    first.clip = { 0.0f, 0.0f, page.width, page.height };
    first.fill = juce::FillType (juce::Colours::black);
    stack.push_back (first);
}

void Recorder::beginGroup (const juce::String& name)
{
    page.groups.add (name);
    group = page.groups.size() - 1;
}

void Recorder::setOrigin (juce::Point<int> p)
{
    s().transform = juce::AffineTransform::translation (p.toFloat()).followedBy (s().transform);
}

void Recorder::addTransform (const juce::AffineTransform& t)
{
    s().transform = t.followedBy (s().transform);
}

float Recorder::getPhysicalPixelScaleFactor() const
{
    // Curves are flattened finely: the page is printed, or zoomed into.
    return 4.0f;
}

bool Recorder::clipToRectangle (const juce::Rectangle<int>& r)
{
    const auto& t = s().transform;
    if (t.mat01 == 0.0f && t.mat10 == 0.0f)
        s().clip = s().clip.getIntersection (r.toFloat().transformedBy (t));
    else
    {
        juce::Path p;
        p.addRectangle (r.toFloat());
        clipToPath (p, {});
    }
    return ! s().clip.isEmpty();
}

bool Recorder::clipToRectangleList (const juce::RectangleList<int>& list)
{
    if (list.getNumRectangles() == 1) return clipToRectangle (list.getRectangle (0));
    juce::Path p;
    for (auto& r : list) p.addRectangle (r.toFloat());
    clipToPath (p, {});
    return ! s().clip.isEmpty();
}

void Recorder::clipToPath (const juce::Path& path, const juce::AffineTransform& t)
{
    auto p = path;
    p.applyTransform (t.followedBy (s().transform));
    s().clip = s().clip.getIntersection (p.getBounds());
    page.clipPaths.push_back (p);
    s().clipPaths.push_back ((int) page.clipPaths.size() - 1);
}

bool Recorder::clipRegionIntersects (const juce::Rectangle<int>& r)
{
    return s().clip.intersects (r.toFloat().transformedBy (s().transform));
}

juce::Rectangle<int> Recorder::getClipBounds() const
{
    if (s().clip.isEmpty()) return {};
    return s().clip.transformedBy (s().transform.inverted()).getSmallestIntegerContainer();
}

bool Recorder::isClipEmpty() const
{
    return s().clip.isEmpty();
}

void Recorder::saveState()                          { stack.push_back (stack.back()); }
void Recorder::restoreState()                       { if (stack.size() > 1) stack.pop_back(); }
void Recorder::beginTransparencyLayer (float o)     { saveState(); s().layerOpacity *= o; }
void Recorder::endTransparencyLayer()               { restoreState(); }
void Recorder::setFill (const juce::FillType& f)    { s().fill = f; }
void Recorder::setOpacity (float o)                 { s().opacity = o; }
void Recorder::setFont (const juce::Font& f)        { s().font = f; }
const juce::Font& Recorder::getFont()               { return s().font; }

std::unique_ptr<juce::ImageType> Recorder::getPreferredImageTypeForTemporaryImages() const
{
    return std::make_unique<juce::SoftwareImageType>();
}

juce::Colour Recorder::colourNow (juce::Colour c) const
{
    c = c.withMultipliedAlpha (s().opacity * s().layerOpacity);
    return map ? map (c) : c;
}

void Recorder::push (Op op, juce::Rectangle<float> bounds)
{
    if (! s().clip.intersects (bounds.expanded (0.5f))) return;
    op.group = group;
    // A clip only where it cuts something.
    if (! s().clipPaths.empty() || ! s().clip.contains (bounds))
    {
        op.clip = s().clip;
        op.clipPaths = s().clipPaths;
    }
    // A fill next to the one before, in the same colour and clip (a waveform's
    // columns, a run of ticks): one path. Not where they overlap, which would
    // change how a see-through colour adds up.
    if (op.kind == Op::Kind::fill && ! op.gradient && ! page.ops.empty())
    {
        auto& last = page.ops.back();
        if (last.kind == Op::Kind::fill && ! last.gradient && last.group == op.group && last.colour == op.colour
            && last.clip == op.clip && last.clipPaths == op.clipPaths
            && last.path.isUsingNonZeroWinding() && op.path.isUsingNonZeroWinding()
            && (op.colour.isOpaque() || ! last.path.getBounds().intersects (bounds)))
        {
            last.path.addPath (op.path);
            return;
        }
    }
    page.ops.push_back (std::move (op));
}

void Recorder::fillRect (const juce::Rectangle<int>& r, bool)   { fillRect (r.toFloat()); }

void Recorder::fillRect (const juce::Rectangle<float>& r)
{
    juce::Path p;
    p.addRectangle (r);
    fillPath (p, {});
}

void Recorder::fillRectList (const juce::RectangleList<float>& list)
{
    juce::Path p;
    for (auto& r : list) p.addRectangle (r);
    fillPath (p, {});
}

void Recorder::fillPath (const juce::Path& path, const juce::AffineTransform& t)
{
    Op op;
    op.path = path;
    op.path.applyTransform (t.followedBy (s().transform));
    const auto& fill = s().fill;
    if (fill.isGradient() && fill.gradient != nullptr)
    {
        op.gradient = true;
        op.gradientFill = *fill.gradient;
        const auto gt = fill.transform.followedBy (s().transform);
        op.gradientFill.point1 = op.gradientFill.point1.transformedBy (gt);
        op.gradientFill.point2 = op.gradientFill.point2.transformedBy (gt);
        for (int i = 0; i < op.gradientFill.getNumColours(); ++i)
            op.gradientFill.setColour (i, colourNow (op.gradientFill.getColour (i).withMultipliedAlpha (fill.getOpacity())));
        op.colour = op.gradientFill.getColourAtPosition (0.5);
    }
    else if (fill.isColour())
        op.colour = colourNow (fill.colour);
    else
        op.colour = colourNow (juce::Colours::grey.withAlpha (fill.getOpacity()));
    if (op.colour.isTransparent() && ! op.gradient) return;
    const auto bounds = op.path.getBounds();
    push (std::move (op), bounds);
}

void Recorder::drawImage (const juce::Image& image, const juce::AffineTransform& t)
{
    if (! image.isValid()) return;
    Op op;
    op.kind = Op::Kind::image;
    const auto toPage = t.followedBy (s().transform);
    op.opacity = s().opacity * s().layerOpacity * (s().fill.isColour() ? s().fill.colour.getFloatAlpha() : 1.0f);
    const auto bounds = image.getBounds().toFloat().transformedBy (toPage);
    if (! s().clip.intersects (bounds)) return;

    // No more pixels than the page shows at the resolution asked for.
    const float pointsPerPixelX = std::hypot (toPage.mat00, toPage.mat10), pointsPerPixelY = std::hypot (toPage.mat01, toPage.mat11);
    const int w = image.getWidth(), h = image.getHeight();
    const int nw = juce::jlimit (1, w, (int) std::ceil ((float) w * pointsPerPixelX * dpi / 72.0f));
    const int nh = juce::jlimit (1, h, (int) std::ceil ((float) h * pointsPerPixelY * dpi / 72.0f));
    const auto key = std::make_tuple (static_cast<const void*> (image.getPixelData().get()), nw, nh);
    auto& cache = imageCache();
    auto it = cache.find (key);
    if (it == cache.end())
    {
        ScaledImage scaled;
        auto smaller = (nw < w || nh < h) ? image.rescaled (nw, nh, juce::Graphics::highResamplingQuality) : image;
        scaled.image = smaller.convertedToFormat (juce::Image::ARGB);
        scaled.opaque = ! image.hasAlphaChannel() || isOpaque (scaled.image);
        it = cache.emplace (key, std::move (scaled)).first;
    }
    op.image = it->second.image;
    op.opaque = it->second.opaque;
    op.imageTransform = juce::AffineTransform::scale ((float) w / (float) op.image.getWidth(), (float) h / (float) op.image.getHeight()).followedBy (toPage);
    push (std::move (op), bounds);
}

void Recorder::drawLine (const juce::Line<float>& line)
{
    drawLineWithThickness (line, 1.0f);
}

namespace
{
    /** For each typeface, which character each glyph draws (the common ones). */
    const std::map<uint16_t, juce_wchar>& charactersOf (const juce::Typeface::Ptr& typeface)
    {
        static std::map<const juce::Typeface*, std::map<uint16_t, juce_wchar>> cache;
        auto& m = cache[typeface.get()];
        if (m.empty() && typeface != nullptr)
        {
            auto add = [&] (juce_wchar from, juce_wchar to)
            {
                for (auto c = from; c <= to; ++c)
                    if (auto g = typeface->getNominalGlyphForCodepoint (c))
                        m.emplace ((uint16_t) *g, c);
            };
            add (0x20, 0x7e);
            add (0xa0, 0x17f);
            add (0x2010, 0x2027);
            add (0x2030, 0x205e);
            add (0x2190, 0x21ff);
            add (0x2200, 0x22ff);
            add (0x25a0, 0x25ff);
        }
        return m;
    }

    juce::String familyFor (const juce::Font& font)
    {
        auto name = font.getTypefacePtr() != nullptr ? font.getTypefacePtr()->getName() : font.getTypefaceName();
        const bool mono = name.containsIgnoreCase ("mono") || name.containsIgnoreCase ("menlo") || name.containsIgnoreCase ("courier")
                          || name.containsIgnoreCase ("consol") || font.getTypefaceName() == juce::Font::getDefaultMonospacedFontName();
        juce::String list = name.startsWithChar ('.') || name.startsWithChar ('<') ? juce::String() : "'" + name + "', ";
        list << (mono ? "Menlo, 'SF Mono', Consolas, monospace" : "-apple-system, 'SF Pro Text', 'Helvetica Neue', Helvetica, Arial, sans-serif");
        return list;
    }
}

void Recorder::drawGlyphs (juce::Span<const uint16_t> glyphs, juce::Span<const juce::Point<float>> positions, const juce::AffineTransform& t)
{
    const auto& font = s().font;
    auto typeface = font.getTypefacePtr();
    if (typeface == nullptr || glyphs.empty()) return;
    const float h = font.getHeightInPoints(), hs = font.getHorizontalScale();
    const auto toPage = t.followedBy (s().transform);

    Op op;
    op.kind = Op::Kind::text;
    op.colour = s().fill.isColour() ? colourNow (s().fill.colour) : colourNow (juce::Colours::black);
    if (op.colour.isTransparent()) return;

    // The outline (for the PDF, and where a glyph has no character).
    bool asText = std::abs (toPage.mat01) < 1.0e-4f && std::abs (toPage.mat10) < 1.0e-4f
                  && std::abs (toPage.mat00 - toPage.mat11) < 1.0e-3f * std::abs (toPage.mat11) && toPage.mat11 > 0.0f;
    const auto& chars = charactersOf (typeface);
    const float y0 = positions.empty() ? 0.0f : positions[0].y;
    for (size_t i = 0; i < glyphs.size(); ++i)
    {
        juce::Path gp;
        typeface->getOutlineForGlyph (glyphs[i], gp);
        gp.applyTransform (juce::AffineTransform::scale (h * hs, h).translated (positions[i]).followedBy (toPage));
        op.path.addPath (gp);
        auto c = chars.find (glyphs[i]);
        if (c == chars.end() || std::abs (positions[i].y - y0) > 0.01f) asText = false;
        else if (asText)
        {
            op.text << juce::String::charToString (c->second);
            op.xs.push_back (positions[i].transformedBy (toPage).x);
        }
    }
    if (asText)
    {
        op.baseline = juce::Point<float> (positions[0].x, y0).transformedBy (toPage).y;
        op.fontSize = h * toPage.mat11;
        op.horizontalScale = hs;
        op.family = familyFor (font);
        op.bold = font.isBold();
        op.italic = font.isItalic();
    }
    else
    {
        op.kind = Op::Kind::fill;
        op.text.clear();
        op.xs.clear();
    }
    if (op.path.isEmpty() && op.text.trim().isEmpty()) return;
    const auto bounds = op.path.isEmpty() ? juce::Rectangle<float> (op.xs.front(), op.baseline - op.fontSize, 1.0f, op.fontSize) : op.path.getBounds();
    push (std::move (op), bounds);
}

//== Writing ===================================================================

namespace
{
    juce::String num (float v)
    {
        if (std::abs (v) < 0.0005f) return "0";
        auto s = juce::String (v, 2);
        if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s == "-0" ? "0" : s;
    }

    juce::String hex (juce::Colour c)
    {
        return "#" + c.toDisplayString (false).toLowerCase();
    }

    juce::String escape (const juce::String& s)
    {
        return s.replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;").replace ("\"", "&quot;");
    }

    template <typename Emit>
    void walk (const juce::Path& p, Emit&& emit)
    {
        juce::Path::Iterator it (p);
        float lastX = 0.0f, lastY = 0.0f;
        while (it.next())
        {
            switch (it.elementType)
            {
                case juce::Path::Iterator::startNewSubPath: emit ('M', lastX = it.x1, lastY = it.y1, 0, 0, 0, 0); break;
                case juce::Path::Iterator::lineTo:          emit ('L', lastX = it.x1, lastY = it.y1, 0, 0, 0, 0); break;
                case juce::Path::Iterator::quadraticTo:
                {
                    // As a cubic.
                    const float c1x = lastX + 2.0f / 3.0f * (it.x1 - lastX), c1y = lastY + 2.0f / 3.0f * (it.y1 - lastY);
                    const float c2x = it.x2 + 2.0f / 3.0f * (it.x1 - it.x2), c2y = it.y2 + 2.0f / 3.0f * (it.y1 - it.y2);
                    emit ('C', c1x, c1y, c2x, c2y, lastX = it.x2, lastY = it.y2);
                    break;
                }
                case juce::Path::Iterator::cubicTo:         emit ('C', it.x1, it.y1, it.x2, it.y2, lastX = it.x3, lastY = it.y3); break;
                case juce::Path::Iterator::closePath:       emit ('Z', 0, 0, 0, 0, 0, 0); break;
            }
        }
    }

    juce::String svgPath (const juce::Path& p)
    {
        juce::String d;
        walk (p, [&d] (char k, float a, float b, float c, float e, float f, float g)
        {
            d << k;
            if (k == 'M' || k == 'L') d << num (a) << " " << num (b);
            else if (k == 'C') d << num (a) << " " << num (b) << " " << num (c) << " " << num (e) << " " << num (f) << " " << num (g);
        });
        return d;
    }

    juce::String pdfPath (const juce::Path& p)
    {
        juce::String d;
        walk (p, [&d] (char k, float a, float b, float c, float e, float f, float g)
        {
            if (k == 'M')      d << num (a) << " " << num (b) << " m\n";
            else if (k == 'L') d << num (a) << " " << num (b) << " l\n";
            else if (k == 'C') d << num (a) << " " << num (b) << " " << num (c) << " " << num (e) << " " << num (f) << " " << num (g) << " c\n";
            else               d << "h\n";
        });
        return d;
    }

    juce::String rectKey (juce::Rectangle<float> r)
    {
        return num (r.getX()) + "," + num (r.getY()) + "," + num (r.getWidth()) + "," + num (r.getHeight());
    }
}

juce::String toSvg (const Page& page)
{
    juce::String defs, body;
    std::map<juce::String, juce::String> clips;
    std::map<const void*, juce::String> images;
    int nextClip = 0, nextGradient = 0;

    auto clipFor = [&] (const Op& op) -> juce::String
    {
        if (op.clip.isEmpty() && op.clipPaths.empty()) return {};
        juce::String key = rectKey (op.clip);
        auto it = clips.find (key);
        juce::String id;
        if (it == clips.end())
        {
            id = "c" + juce::String (nextClip++);
            defs << "<clipPath id=\"" << id << "\"><rect x=\"" << num (op.clip.getX()) << "\" y=\"" << num (op.clip.getY())
                 << "\" width=\"" << num (op.clip.getWidth()) << "\" height=\"" << num (op.clip.getHeight()) << "\"/></clipPath>\n";
            clips[key] = id;
        }
        else id = it->second;
        for (int index : op.clipPaths)
        {
            key << "|" << index;
            it = clips.find (key);
            if (it != clips.end()) { id = it->second; continue; }
            const auto& p = page.clipPaths[(size_t) index];
            const auto parent = id;
            id = "c" + juce::String (nextClip++);
            defs << "<clipPath id=\"" << id << "\" clip-path=\"url(#" << parent << ")\"><path d=\"" << svgPath (p) << "\""
                 << (p.isUsingNonZeroWinding() ? "" : " clip-rule=\"evenodd\"") << "/></clipPath>\n";
            clips[key] = id;
        }
        return " clip-path=\"url(#" + id + ")\"";
    };

    auto paintFor = [&] (const Op& op) -> juce::String
    {
        if (! op.gradient)
            return " fill=\"" + hex (op.colour) + "\"" + (op.colour.getAlpha() < 255 ? " fill-opacity=\"" + num (op.colour.getFloatAlpha()) + "\"" : juce::String());
        const auto& g = op.gradientFill;
        const auto id = "g" + juce::String (nextGradient++);
        if (g.isRadial)
            defs << "<radialGradient id=\"" << id << "\" gradientUnits=\"userSpaceOnUse\" cx=\"" << num (g.point1.x) << "\" cy=\"" << num (g.point1.y)
                 << "\" r=\"" << num (g.point1.getDistanceFrom (g.point2)) << "\">";
        else
            defs << "<linearGradient id=\"" << id << "\" gradientUnits=\"userSpaceOnUse\" x1=\"" << num (g.point1.x) << "\" y1=\"" << num (g.point1.y)
                 << "\" x2=\"" << num (g.point2.x) << "\" y2=\"" << num (g.point2.y) << "\">";
        for (int i = 0; i < g.getNumColours(); ++i)
            defs << "<stop offset=\"" << num ((float) g.getColourPosition (i)) << "\" stop-color=\"" << hex (g.getColour (i))
                 << "\" stop-opacity=\"" << num (g.getColour (i).getFloatAlpha()) << "\"/>";
        defs << (g.isRadial ? "</radialGradient>\n" : "</linearGradient>\n");
        return " fill=\"url(#" + id + ")\"";
    };

    int openGroup = -1;
    for (auto& op : page.ops)
    {
        if (op.group != openGroup)
        {
            if (openGroup > 0) body << "</g>\n";
            openGroup = op.group;
            if (openGroup > 0) body << "<g id=\"" << escape (page.groups[openGroup].replaceCharacter (' ', '-')) << "\">\n";
        }
        const auto clip = clipFor (op);
        switch (op.kind)
        {
            case Op::Kind::fill:
                body << "<path d=\"" << svgPath (op.path) << "\"" << paintFor (op)
                     << (op.path.isUsingNonZeroWinding() ? "" : " fill-rule=\"evenodd\"") << clip << "/>\n";
                break;
            case Op::Kind::text:
            {
                juce::String xs;
                for (auto x : op.xs) xs << num (x) << " ";
                body << "<text xml:space=\"preserve\" x=\"" << xs.trimEnd() << "\" y=\"" << num (op.baseline) << "\" font-family=\"" << escape (op.family)
                     << "\" font-size=\"" << num (op.fontSize) << "\"" << (op.bold ? " font-weight=\"bold\"" : "") << (op.italic ? " font-style=\"italic\"" : "")
                     << paintFor (op) << clip << ">" << escape (op.text) << "</text>\n";
                break;
            }
            case Op::Kind::image:
            {
                // Each image once, in the defs; drawn where it is used.
                const void* pixels = op.image.getPixelData().get();
                auto it = images.find (pixels);
                if (it == images.end())
                {
                    juce::MemoryOutputStream data;
                    juce::String type = "png";
                    if (op.opaque)
                    {
                        juce::JPEGImageFormat jpeg;
                        jpeg.setQuality (0.85f);
                        jpeg.writeImageToStream (op.image, data);
                        type = "jpeg";
                    }
                    else juce::PNGImageFormat().writeImageToStream (op.image, data);
                    const auto id = "i" + juce::String ((int) images.size());
                    defs << "<image id=\"" << id << "\" width=\"" << op.image.getWidth() << "\" height=\"" << op.image.getHeight()
                         << "\" preserveAspectRatio=\"none\" xlink:href=\"data:image/" << type << ";base64,"
                         << juce::Base64::toBase64 (data.getData(), data.getDataSize()) << "\"/>\n";
                    it = images.emplace (pixels, id).first;
                }
                const auto& t = op.imageTransform;
                body << "<use xlink:href=\"#" << it->second << "\" transform=\"matrix("
                     << num (t.mat00) << " " << num (t.mat10) << " " << num (t.mat01) << " " << num (t.mat11) << " " << num (t.mat02) << " " << num (t.mat12) << ")\""
                     << (op.opacity < 0.999f ? " opacity=\"" + num (op.opacity) + "\"" : juce::String()) << clip << "/>\n";
                break;
            }
        }
    }
    if (openGroup > 0) body << "</g>\n";

    juce::String out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << num (page.width) << "pt\" height=\"" << num (page.height) << "pt\" viewBox=\"0 0 "
        << num (page.width) << " " << num (page.height) << "\">\n";
    if (defs.isNotEmpty()) out << "<defs>\n" << defs << "</defs>\n";
    out << "<rect width=\"" << num (page.width) << "\" height=\"" << num (page.height) << "\" fill=\"" << hex (page.background) << "\"/>\n";
    out << body << "</svg>\n";
    return out;
}

namespace
{
    juce::MemoryBlock deflate (const void* data, size_t size)
    {
        juce::MemoryOutputStream out;
        {
            juce::GZIPCompressorOutputStream z (out, 6);     // zlib format, as PDF's FlateDecode wants
            z.write (data, size);
            z.flush();
        }
        return out.getMemoryBlock();
    }
}

juce::MemoryBlock toPdf (const std::vector<Page>& pages, const juce::String& title)
{
    std::vector<juce::MemoryBlock> objects;              // object n is objects[n - 1]
    auto reserve = [&] { objects.emplace_back(); return (int) objects.size(); };
    auto set = [&] (int n, const juce::MemoryBlock& b) { objects[(size_t) n - 1] = b; };
    auto text = [] (const juce::String& s) { return juce::MemoryBlock (s.toRawUTF8(), s.getNumBytesAsUTF8()); };
    auto stream = [&] (const juce::String& dict, const juce::MemoryBlock& data)
    {
        juce::MemoryOutputStream o;
        o << "<< " << dict << " /Length " << (int) data.getSize() << " >>\nstream\n";
        o.write (data.getData(), data.getSize());
        o << "\nendstream";
        return o.getMemoryBlock();
    };

    const int catalog = reserve(), pagesObj = reserve(), info = reserve();
    juce::StringArray kids;
    std::map<const void*, int> imageObjects;             // each image once in the document

    for (auto& page : pages)
    {
        const int pageObj = reserve(), contentObj = reserve();
        kids.add (juce::String (pageObj) + " 0 R");
        std::map<int, juce::String> alphas;              // alpha (0..1000) -> name
        juce::StringArray xobjects;
        juce::String c;
        c << "1 0 0 -1 0 " << num (page.height) << " cm\n";
        c << num (page.background.getFloatRed()) << " " << num (page.background.getFloatGreen()) << " " << num (page.background.getFloatBlue())
          << " rg 0 0 " << num (page.width) << " " << num (page.height) << " re f\n";

        auto alphaName = [&] (float a)
        {
            const int k = juce::roundToInt (a * 1000.0f);
            auto it = alphas.find (k);
            if (it != alphas.end()) return it->second;
            const auto name = "a" + juce::String ((int) alphas.size());
            alphas[k] = name;
            return name;
        };
        auto clipOps = [&] (const Op& op)
        {
            juce::String s;
            if (! op.clip.isEmpty())
                s << num (op.clip.getX()) << " " << num (op.clip.getY()) << " " << num (op.clip.getWidth()) << " " << num (op.clip.getHeight()) << " re W n\n";
            for (int index : op.clipPaths)
            {
                const auto& p = page.clipPaths[(size_t) index];
                s << pdfPath (p) << (p.isUsingNonZeroWinding() ? "W n\n" : "W* n\n");
            }
            return s;
        };

        for (auto& op : page.ops)
        {
            if (op.kind == Op::Kind::image)
            {
                const int w = op.image.getWidth(), h = op.image.getHeight();
                const void* pixels = op.image.getPixelData().get();
                auto found = imageObjects.find (pixels);
                if (found == imageObjects.end())
                {
                    const juce::String size = " /Width " + juce::String (w) + " /Height " + juce::String (h);
                    const int image = reserve();
                    if (op.opaque)
                    {
                        // A photo: JPEG, as PDF reads it.
                        juce::MemoryOutputStream jpg;
                        juce::JPEGImageFormat jpeg;
                        jpeg.setQuality (0.85f);
                        jpeg.writeImageToStream (op.image, jpg);
                        set (image, stream ("/Type /XObject /Subtype /Image" + size + " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode",
                                            jpg.getMemoryBlock()));
                    }
                    else
                    {
                        // RGB, with its alpha as a soft mask.
                        juce::MemoryBlock rgb ((size_t) (w * h * 3)), alpha ((size_t) (w * h));
                        const juce::Image::BitmapData bits (op.image, juce::Image::BitmapData::readOnly);
                        auto* p = static_cast<uint8_t*> (rgb.getData());
                        auto* a = static_cast<uint8_t*> (alpha.getData());
                        for (int y = 0; y < h; ++y)
                            for (int x = 0; x < w; ++x)
                            {
                                const auto col = bits.getPixelColour (x, y);
                                *p++ = col.getRed(); *p++ = col.getGreen(); *p++ = col.getBlue();
                                *a++ = col.getAlpha();
                            }
                        const int mask = reserve();
                        set (mask, stream ("/Type /XObject /Subtype /Image" + size + " /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /FlateDecode",
                                           deflate (alpha.getData(), alpha.getSize())));
                        set (image, stream ("/Type /XObject /Subtype /Image" + size + " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /SMask "
                                            + juce::String (mask) + " 0 R", deflate (rgb.getData(), rgb.getSize())));
                    }
                    found = imageObjects.emplace (pixels, image).first;
                }
                const int image = found->second;
                const auto name = "Im" + juce::String (image);
                if (! xobjects.contains ("/" + name + " " + juce::String (image) + " 0 R"))
                    xobjects.add ("/" + name + " " + juce::String (image) + " 0 R");
                const auto t = juce::AffineTransform (1.0f / (float) w, 0.0f, 0.0f, 0.0f, -1.0f / (float) h, 1.0f).inverted()
                                   .followedBy (op.imageTransform);
                c << "q\n" << clipOps (op);
                if (op.opacity < 0.999f) c << "/" << alphaName (op.opacity) << " gs\n";
                c << num (t.mat00) << " " << num (t.mat10) << " " << num (t.mat01) << " " << num (t.mat11) << " " << num (t.mat02) << " " << num (t.mat12)
                  << " cm /" << name << " Do\nQ\n";
                continue;
            }
            if (op.path.isEmpty()) continue;
            c << "q\n" << clipOps (op);
            if (op.colour.getAlpha() < 255) c << "/" << alphaName (op.colour.getFloatAlpha()) << " gs\n";
            c << num (op.colour.getFloatRed()) << " " << num (op.colour.getFloatGreen()) << " " << num (op.colour.getFloatBlue()) << " rg\n"
              << pdfPath (op.path) << (op.path.isUsingNonZeroWinding() ? "f\n" : "f*\n") << "Q\n";
        }

        juce::String gs;
        for (auto& [k, name] : alphas) gs << "/" << name << " << /ca " << num ((float) k / 1000.0f) << " /CA " << num ((float) k / 1000.0f) << " >> ";
        juce::String resources = "<< /ExtGState << " + gs + ">> /XObject << " + xobjects.joinIntoString (" ") + " >> >>";
        const auto content = juce::MemoryBlock (c.toRawUTF8(), c.getNumBytesAsUTF8());
        set (contentObj, stream ("/Filter /FlateDecode", deflate (content.getData(), content.getSize())));
        set (pageObj, text ("<< /Type /Page /Parent " + juce::String (pagesObj) + " 0 R /MediaBox [0 0 " + num (page.width) + " " + num (page.height)
                            + "] /Resources " + resources + " /Contents " + juce::String (contentObj) + " 0 R >>"));
    }

    set (catalog, text ("<< /Type /Catalog /Pages " + juce::String (pagesObj) + " 0 R >>"));
    set (pagesObj, text ("<< /Type /Pages /Kids [" + kids.joinIntoString (" ") + "] /Count " + juce::String (kids.size()) + " >>"));
    auto pdfString = [] (const juce::String& s)
    {
        juce::String out ("(");
        for (auto ch : s)
        {
            if (ch == '(' || ch == ')' || ch == '\\') out << "\\" << juce::String::charToString (ch);
            else if (ch >= 32 && ch < 127) out << juce::String::charToString (ch);
            else out << "?";
        }
        return out + ")";
    };
    set (info, text ("<< /Title " + pdfString (title) + " /Creator (Projet) >>"));

    juce::MemoryOutputStream out;
    out << "%PDF-1.4\n%" << juce::String::fromUTF8 ("\xc3\xa2\xc3\xa3\xc3\x8f\xc3\x93") << "\n";
    std::vector<juce::int64> offsets;
    for (size_t i = 0; i < objects.size(); ++i)
    {
        offsets.push_back (out.getPosition());
        out << (int) (i + 1) << " 0 obj\n";
        out.write (objects[i].getData(), objects[i].getSize());
        out << "\nendobj\n";
    }
    const auto xref = out.getPosition();
    out << "xref\n0 " << (int) (objects.size() + 1) << "\n0000000000 65535 f \n";
    for (auto o : offsets) out << juce::String (o).paddedLeft ('0', 10) << " 00000 n \n";
    out << "trailer\n<< /Size " << (int) (objects.size() + 1) << " /Root " << catalog << " 0 R /Info " << info << " 0 R >>\nstartxref\n"
        << (int) xref << "\n%%EOF\n";
    return out.getMemoryBlock();
}

} // namespace Vector
