// Project.cpp

#include "Project.h"
#include "Proportion.h"

const char* fadeShapeName (FadeShape s)
{
    switch (s)
    {
        case FadeShape::linear:     return "Linear";
        case FadeShape::equalPower: return "Equal power";
        case FadeShape::sCurve:     return "S-curve";
    }
    return "Linear";
}

Project::Project()
{
    state = createState ("Untitled", defaultSampleRate);
    state.addListener (this);
}

Project::~Project()
{
    state.removeListener (this);
}

juce::ValueTree Project::createState (const juce::String& name, double rate)
{
    juce::ValueTree s (IDs::PROJET);
    s.setProperty (IDs::formatVersion, currentFormatVersion, nullptr);
    s.setProperty (IDs::name, name, nullptr);
    s.setProperty (IDs::sampleRate, rate, nullptr);
    s.setProperty (IDs::placeholderFormat, "ogg", nullptr);
    s.setProperty (IDs::vorbisQuality, 7, nullptr);
    s.setProperty (IDs::viewStart, 0.0, nullptr);
    s.setProperty (IDs::pixelsPerSecond, 20.0, nullptr);
    s.setProperty (IDs::playhead, 0.0, nullptr);
    s.appendChild (juce::ValueTree (IDs::SOUNDS), nullptr);
    s.appendChild (juce::ValueTree (IDs::TRACKS), nullptr);
    return s;
}

void Project::resetToDefault (const juce::String& name, double sampleRate)
{
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        state.copyPropertiesAndChildrenFrom (createState (name, sampleRate), nullptr);
        addTrack ("Track 1");
        ensureRulers();
    }
    undoManager.clearUndoHistory();
}

void Project::changed()
{
    if (! applying && ! loading)
        applyProportions();
    if (onChanged) onChanged();
}

//== Sounds ====================================================================

juce::ValueTree Project::findSound (const juce::String& soundId) const
{
    return state.getChildWithName (IDs::SOUNDS).getChildWithProperty (IDs::id, soundId);
}

juce::ValueTree Project::findSoundByHash (const juce::String& hash, const juce::String& format) const
{
    for (auto s : state.getChildWithName (IDs::SOUNDS))
        if (s[IDs::originalHash].toString() == hash && s[IDs::format].toString() == format)
            return s;
    return {};
}

juce::ValueTree Project::addSound (const juce::ValueTree& record)
{
    auto copy = record.createCopy();
    getSounds().appendChild (copy, nullptr);
    return copy;
}

double Project::soundSeconds (const juce::String& soundId) const
{
    auto s = findSound (soundId);
    if (! s.isValid()) return 0.0;
    return (double) (juce::int64) s[IDs::lengthSamples] / getSampleRate();
}

//== Tracks ====================================================================

juce::ValueTree Project::addTrack (const juce::String& name, int index, const juce::String& folderId)
{
    auto tracks = getTracks();
    int lanes = 0;
    for (auto t : tracks) if (! isFolder (t)) ++lanes;
    juce::ValueTree t (IDs::TRACK);
    t.setProperty (IDs::id, newId(), nullptr);
    t.setProperty (IDs::name, name.isNotEmpty() ? name : "Track " + juce::String (lanes + 1), nullptr);
    t.setProperty (IDs::colour, tracks.getNumChildren() % 8, nullptr);
    t.setProperty (IDs::height, 72, nullptr);
    t.setProperty (IDs::mute, false, nullptr);
    t.setProperty (IDs::solo, false, nullptr);
    if (folderId.isNotEmpty()) t.setProperty (IDs::folderId, folderId, nullptr);
    tracks.addChild (t, index, &undoManager);
    return t;
}

void Project::removeTrack (juce::ValueTree track)
{
    auto tracks = getTracks();
    const int first = tracks.indexOf (track);
    if (first < 0) return;
    for (int i = blockEnd (track); --i >= first;)
        tracks.removeChild (i, &undoManager);
}

juce::ValueTree Project::findTrack (const juce::String& trackId)
{
    if (trackId.isEmpty()) return {};
    return getTracks().getChildWithProperty (IDs::id, trackId);
}

//== Folders ===================================================================

juce::ValueTree Project::folderOf (const juce::ValueTree& track)
{
    return findTrack (track[IDs::folderId].toString());
}

int Project::depthOf (const juce::ValueTree& track)
{
    int depth = 0;
    for (auto f = folderOf (track); f.isValid() && depth < 64; f = folderOf (f)) ++depth;
    return depth;
}

bool Project::isInside (const juce::ValueTree& track, const juce::ValueTree& folder)
{
    int guard = 0;
    for (auto f = folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        if (f == folder) return true;
    return false;
}

bool Project::isVisible (const juce::ValueTree& track)
{
    int guard = 0;
    for (auto f = folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        if ((bool) f[IDs::folded]) return false;
    return true;
}

int Project::effectiveColour (const juce::ValueTree& track)
{
    auto outer = track;
    int guard = 0;
    for (auto f = folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        outer = f;
    return (int) outer[IDs::colour];
}

bool Project::effectiveMute (const juce::ValueTree& track)
{
    if ((bool) track[IDs::mute]) return true;
    int guard = 0;
    for (auto f = folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        if ((bool) f[IDs::mute]) return true;
    return false;
}

bool Project::effectiveSolo (const juce::ValueTree& track)
{
    if ((bool) track[IDs::solo]) return true;
    int guard = 0;
    for (auto f = folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        if ((bool) f[IDs::solo]) return true;
    return false;
}

bool Project::anySolo()
{
    for (auto t : getTracks())
        if ((bool) t[IDs::solo]) return true;
    return false;
}

juce::Array<juce::ValueTree> Project::laneTracks()
{
    juce::Array<juce::ValueTree> lanes;
    for (auto t : getTracks())
        if (! isFolder (t)) lanes.add (t);
    return lanes;
}

int Project::blockEnd (const juce::ValueTree& track)
{
    auto tracks = getTracks();
    int i = tracks.indexOf (track);
    if (i < 0) return -1;
    ++i;
    if (! isFolder (track)) return i;
    while (i < tracks.getNumChildren() && isInside (tracks.getChild (i), track)) ++i;
    return i;
}

juce::ValueTree Project::addFolder (const juce::String& name, int index, const juce::String& folderId)
{
    juce::ValueTree f (IDs::TRACK);
    f.setProperty (IDs::id, newId(), nullptr);
    f.setProperty (IDs::name, name.isNotEmpty() ? name : juce::String ("Folder"), nullptr);
    f.setProperty (IDs::isFolder, true, nullptr);
    f.setProperty (IDs::colour, getTracks().getNumChildren() % 8, nullptr);
    f.setProperty (IDs::height, 30, nullptr);
    f.setProperty (IDs::mute, false, nullptr);
    f.setProperty (IDs::solo, false, nullptr);
    f.setProperty (IDs::folded, false, nullptr);
    if (folderId.isNotEmpty()) f.setProperty (IDs::folderId, folderId, nullptr);
    getTracks().addChild (f, index, &undoManager);
    return f;
}

juce::ValueTree Project::putInNewFolder (juce::ValueTree track)
{
    auto tracks = getTracks();
    const int index = tracks.indexOf (track);
    if (index < 0) return {};
    auto f = addFolder ("Folder", index, track[IDs::folderId].toString());
    f.setProperty (IDs::colour, track[IDs::colour], &undoManager);
    track.setProperty (IDs::folderId, f[IDs::id].toString(), &undoManager);
    return f;
}

void Project::moveTrack (juce::ValueTree track, int toIndex, const juce::String& folderId)
{
    auto tracks = getTracks();
    const int first = tracks.indexOf (track);
    if (first < 0) return;
    auto target = findTrack (folderId);
    if (target == track || (target.isValid() && isInside (target, track))) return;   // not into itself

    const int end = blockEnd (track);
    toIndex = juce::jlimit (0, tracks.getNumChildren(), toIndex);

    if (toIndex < first || toIndex > end)
    {
        juce::Array<juce::ValueTree> block;
        for (int i = first; i < end; ++i) block.add (tracks.getChild (i));
        for (int i = end; --i >= first;) tracks.removeChild (i, &undoManager);
        if (toIndex > first) toIndex -= block.size();
        for (int i = 0; i < block.size(); ++i)
            tracks.addChild (block[i], toIndex + i, &undoManager);
    }

    if (track[IDs::folderId].toString() != folderId)
    {
        if (folderId.isEmpty()) track.removeProperty (IDs::folderId, &undoManager);
        else track.setProperty (IDs::folderId, folderId, &undoManager);
    }
}

void Project::ungroupFolder (juce::ValueTree folder)
{
    if (! isFolder (folder)) return;
    const auto up = folder[IDs::folderId].toString();
    const auto id = folder[IDs::id].toString();
    for (auto t : getTracks())
        if (t[IDs::folderId].toString() == id)
        {
            if (up.isEmpty()) t.removeProperty (IDs::folderId, &undoManager);
            else t.setProperty (IDs::folderId, up, &undoManager);
        }
    getTracks().removeChild (folder, &undoManager);
}

//== Boxes =====================================================================

juce::ValueTree Project::addSoundBox (juce::ValueTree track, double start, const juce::String& soundId)
{
    auto sound = findSound (soundId);
    juce::ValueTree b (IDs::BOX);
    b.setProperty (IDs::id, newId(), nullptr);
    b.setProperty (IDs::kind, "sound", nullptr);
    b.setProperty (IDs::name, sound[IDs::name].toString(), nullptr);
    b.setProperty (IDs::start, juce::jmax (0.0, start), nullptr);
    b.setProperty (IDs::length, juce::jmax (minBoxLength, soundSeconds (soundId)), nullptr);
    b.setProperty (IDs::soundId, soundId, nullptr);
    b.setProperty (IDs::offset, 0.0, nullptr);
    b.setProperty (IDs::gainDb, 0.0, nullptr);
    b.setProperty (IDs::fadeIn, 0.01, nullptr);
    b.setProperty (IDs::fadeOut, 0.01, nullptr);
    b.setProperty (IDs::fadeInShape, (int) FadeShape::sCurve, nullptr);
    b.setProperty (IDs::fadeOutShape, (int) FadeShape::sCurve, nullptr);
    b.setProperty (IDs::opacity, 1.0, nullptr);
    b.setProperty (IDs::notes, juce::String(), nullptr);
    track.appendChild (b, &undoManager);
    return b;
}

juce::ValueTree Project::addTextBox (juce::ValueTree track, double start, double length, const juce::String& name)
{
    juce::ValueTree b (IDs::BOX);
    b.setProperty (IDs::id, newId(), nullptr);
    b.setProperty (IDs::kind, "text", nullptr);
    b.setProperty (IDs::name, name, nullptr);
    b.setProperty (IDs::start, juce::jmax (0.0, start), nullptr);
    b.setProperty (IDs::length, juce::jmax (minBoxLength, length), nullptr);
    b.setProperty (IDs::opacity, 1.0, nullptr);
    b.setProperty (IDs::notes, juce::String(), nullptr);
    track.appendChild (b, &undoManager);
    return b;
}

void Project::setKind (juce::ValueTree box, const juce::String& kind)
{
    if (kindOf (box) == kind) return;
    box.setProperty (IDs::kind, kind, &undoManager);

    if (kind == "sound")
    {
        // Defaults for a box that has never been a sound box.
        if (! box.hasProperty (IDs::gainDb))       box.setProperty (IDs::gainDb, 0.0, &undoManager);
        if (! box.hasProperty (IDs::offset))       box.setProperty (IDs::offset, 0.0, &undoManager);
        if (! box.hasProperty (IDs::fadeIn))       box.setProperty (IDs::fadeIn, 0.01, &undoManager);
        if (! box.hasProperty (IDs::fadeOut))      box.setProperty (IDs::fadeOut, 0.01, &undoManager);
        if (! box.hasProperty (IDs::fadeInShape))  box.setProperty (IDs::fadeInShape, (int) FadeShape::sCurve, &undoManager);
        if (! box.hasProperty (IDs::fadeOutShape)) box.setProperty (IDs::fadeOutShape, (int) FadeShape::sCurve, &undoManager);
    }
}

void Project::setSound (juce::ValueTree box, const juce::String& soundId)
{
    if (! findSound (soundId).isValid()) return;
    box.setProperty (IDs::soundId, soundId, &undoManager);
    box.setProperty (IDs::offset, 0.0, &undoManager);
    box.setProperty (IDs::length, juce::jmax (minBoxLength, soundSeconds (soundId)), &undoManager);
    if (box[IDs::name].toString().isEmpty() || box[IDs::name].toString() == "Text")
        box.setProperty (IDs::name, findSound (soundId)[IDs::name].toString(), &undoManager);
    setFades (box, box.getProperty (IDs::fadeIn, 0.01), box.getProperty (IDs::fadeOut, 0.01));
}

juce::ValueTree Project::splitBox (juce::ValueTree box, double t)
{
    const double start = boxStart (box), end = boxEnd (box);
    if (t <= start + minBoxLength || t >= end - minBoxLength) return {};

    auto right = box.createCopy();
    right.setProperty (IDs::id, newId(), nullptr);
    right.setProperty (IDs::start, t, nullptr);
    right.setProperty (IDs::length, end - t, nullptr);
    if (box.hasProperty (IDs::offset))
        right.setProperty (IDs::offset, (double) box[IDs::offset] + (t - start), nullptr);

    // The cut gets short fades so it does not click; the outer fades stay where they were.
    constexpr double cutFade = 0.005;
    const double fadeIn = box.getProperty (IDs::fadeIn, 0.0), fadeOut = box.getProperty (IDs::fadeOut, 0.0);
    right.setProperty (IDs::fadeIn, cutFade, nullptr);
    right.setProperty (IDs::fadeOut, juce::jmin (fadeOut, end - t), nullptr);

    rewriteProportions (right, t, end - t, nullptr);
    rewriteProportions (box, start, t - start, &undoManager);
    box.setProperty (IDs::length, t - start, &undoManager);
    box.setProperty (IDs::fadeIn, juce::jmin (fadeIn, t - start), &undoManager);
    box.setProperty (IDs::fadeOut, cutFade, &undoManager);

    auto track = box.getParent();
    track.addChild (right, track.indexOf (box) + 1, &undoManager);
    return right;
}

juce::Array<juce::ValueTree> Project::allBoxes()
{
    juce::Array<juce::ValueTree> boxes;
    for (auto t : getTracks())
        for (auto b : t)
            boxes.add (b);
    return boxes;
}

void Project::removeBox (juce::ValueTree box)
{
    auto parent = box.getParent();
    if (parent.isValid())
        parent.removeChild (box, &undoManager);
}

juce::ValueTree Project::duplicateBox (juce::ValueTree box)
{
    auto copy = box.createCopy();
    copy.setProperty (IDs::id, newId(), nullptr);
    copy.setProperty (IDs::start, boxEnd (box), nullptr);
    refitProportions (copy);
    box.getParent().appendChild (copy, &undoManager);
    return copy;
}

void Project::moveBox (juce::ValueTree box, juce::ValueTree toTrack, double newStart)
{
    newStart = juce::jmax (0.0, newStart);
    auto from = box.getParent();

    if (toTrack.isValid() && from != toTrack && ! isFolder (toTrack))
    {
        from.removeChild (box, &undoManager);
        toTrack.appendChild (box, &undoManager);
    }

    rewriteProportions (box, newStart, boxLength (box), &undoManager);
    box.setProperty (IDs::start, newStart, &undoManager);
}

juce::ValueTree Project::findBox (const juce::String& boxId)
{
    for (auto t : getTracks())
    {
        auto b = t.getChildWithProperty (IDs::id, boxId);
        if (b.isValid()) return b;
    }
    return {};
}

void Project::trimStart (juce::ValueTree box, double newStart)
{
    const double end = boxEnd (box);
    const double offset = box.getProperty (IDs::offset, 0.0);
    // A sound may not start before its first sample; the box keeps a minimum length.
    const double earliest = isSound (box) ? juce::jmax (0.0, boxStart (box) - offset) : 0.0;
    newStart = juce::jlimit (earliest, end - minBoxLength, newStart);
    const double delta = newStart - boxStart (box);

    if (isSound (box)) box.setProperty (IDs::offset, offset + delta, &undoManager);
    rewriteProportions (box, newStart, end - newStart, &undoManager);
    box.setProperty (IDs::start, newStart, &undoManager);
    box.setProperty (IDs::length, end - newStart, &undoManager);
    setFades (box, box[IDs::fadeIn], box[IDs::fadeOut]);
}

void Project::trimEnd (juce::ValueTree box, double newEnd)
{
    // Text boxes can be any length; sound boxes end with their sound.
    const double available = isSound (box) ? soundSeconds (box[IDs::soundId]) - (double) box[IDs::offset] : 1.0e7;
    const double length = juce::jlimit (minBoxLength, juce::jmax (minBoxLength, available), newEnd - boxStart (box));
    rewriteProportions (box, boxStart (box), length, &undoManager);
    box.setProperty (IDs::length, length, &undoManager);
    setFades (box, box[IDs::fadeIn], box[IDs::fadeOut]);
}

void Project::setFades (juce::ValueTree box, double fadeIn, double fadeOut)
{
    const double len = boxLength (box);
    fadeIn = juce::jlimit (0.0, len, fadeIn);
    fadeOut = juce::jlimit (0.0, len, fadeOut);
    if (fadeIn + fadeOut > len)
    {
        const double scale = len / (fadeIn + fadeOut);
        fadeIn *= scale;
        fadeOut *= scale;
    }
    if (std::abs ((double) box[IDs::fadeIn] - fadeIn) > 1.0e-9)   box.setProperty (IDs::fadeIn, fadeIn, &undoManager);
    if (std::abs ((double) box[IDs::fadeOut] - fadeOut) > 1.0e-9) box.setProperty (IDs::fadeOut, fadeOut, &undoManager);
}

double Project::getContentEnd()
{
    double end = 0.0;
    for (auto t : getTracks())
        for (auto b : t)
            end = juce::jmax (end, boxEnd (b));
    return end;
}

//== Proportion frames =========================================================

double Project::getPieceLength()
{
    if (hasSetPieceLength()) return (double) state[IDs::frameLength];
    return juce::jmax (1.0, getContentEnd());
}

void Project::setPieceLength (double seconds)
{
    state.setProperty (IDs::frameLength, juce::jmax (0.0, seconds), &undoManager);
}

bool Project::hasSpan() const
{
    return (double) state.getProperty (IDs::spanEnd, 0.0) - (double) state.getProperty (IDs::spanStart, 0.0) > 1.0e-6;
}

void Project::setSpan (double a, double b)
{
    if (a > b) std::swap (a, b);
    a = juce::jmax (0.0, a);
    b = juce::jmax (0.0, b);
    state.setProperty (IDs::spanStart, a, &undoManager);
    state.setProperty (IDs::spanEnd, b, &undoManager);
}

Project::Frame Project::frameFor (const juce::String& ref)
{
    if (ref == "piece") return { 0.0, getPieceLength() };
    if (ref == "span")
    {
        if (! hasSpan()) return {};
        const double a = state[IDs::spanStart], b = state[IDs::spanEnd];
        return { a, b - a };
    }
    if (ref.isEmpty()) return {};
    auto b = findBox (ref);
    if (! b.isValid()) return {};
    return { boxStart (b), boxLength (b) };
}

juce::String Project::frameName (const juce::String& ref)
{
    if (ref == "piece") return "piece";
    if (ref == "span") return "span";
    auto b = findBox (ref);
    return b.isValid() ? b[IDs::name].toString() : juce::String ("(removed box)");
}

//== Ruler layers ==============================================================

juce::Array<double> Project::defaultDenominators()
{
    // Every denominator up to 20; which ones show depends on the zoom.
    juce::Array<double> all;
    for (int d = 2; d <= 20; ++d) all.add (d);
    return all;
}

void Project::ensureRulers()
{
    // Projects from before stage 3b: a time layer and one proportion layer
    // carrying the old ruler settings.
    auto rulers = state.getOrCreateChildWithName (IDs::RULERS, nullptr);
    state.getOrCreateChildWithName (IDs::MARKS, nullptr);
    if (rulers.getNumChildren() > 0) return;

    juce::ValueTree time (IDs::LAYER);
    time.setProperty (IDs::id, newId(), nullptr);
    time.setProperty (IDs::kind, "time", nullptr);
    time.setProperty (IDs::name, "Time", nullptr);
    time.setProperty (IDs::height, 30, nullptr);
    time.setProperty (IDs::visible, true, nullptr);
    rulers.appendChild (time, nullptr);

    juce::ValueTree prop (IDs::LAYER);
    prop.setProperty (IDs::id, newId(), nullptr);
    prop.setProperty (IDs::kind, "proportion", nullptr);
    prop.setProperty (IDs::name, "Proportion", nullptr);
    prop.setProperty (IDs::height, 20, nullptr);
    prop.setProperty (IDs::visible, true, nullptr);
    prop.setProperty (IDs::snap, true, nullptr);
    prop.setProperty (IDs::frame, state.getProperty (IDs::rulerFrame, "piece"), nullptr);
    prop.setProperty (IDs::denominators, state.getProperty (IDs::denominators, Proportion::formatList (defaultDenominators())), nullptr);
    prop.setProperty (IDs::autoDivisions, state.getProperty (IDs::autoDivisions, true), nullptr);
    rulers.appendChild (prop, nullptr);
    state.setProperty (IDs::activeLayer, prop[IDs::id].toString(), nullptr);
    for (auto id : { IDs::rulerFrame, IDs::denominators, IDs::autoDivisions }) state.removeProperty (id, nullptr);
}

juce::ValueTree Project::getRulers()
{
    return state.getOrCreateChildWithName (IDs::RULERS, nullptr);
}

juce::ValueTree Project::findLayer (const juce::String& layerId)
{
    if (layerId.isEmpty()) return {};
    return getRulers().getChildWithProperty (IDs::id, layerId);
}

juce::ValueTree Project::addLayer (const juce::String& kind, int index)
{
    juce::ValueTree l (IDs::LAYER);
    l.setProperty (IDs::id, newId(), nullptr);
    l.setProperty (IDs::kind, kind, nullptr);
    l.setProperty (IDs::name, kind == "time" ? "Time" : "Proportion", nullptr);
    l.setProperty (IDs::height, kind == "time" ? 30 : 22, nullptr);
    l.setProperty (IDs::visible, true, nullptr);
    if (kind != "time")
    {
        l.setProperty (IDs::snap, true, nullptr);
        l.setProperty (IDs::frame, "piece", nullptr);
        l.setProperty (IDs::denominators, Proportion::formatList (defaultDenominators()), nullptr);
        l.setProperty (IDs::autoDivisions, true, nullptr);
    }
    getRulers().addChild (l, index, nullptr);
    return l;
}

void Project::removeLayer (juce::ValueTree layer)
{
    getRulers().removeChild (layer, nullptr);
}

juce::ValueTree Project::getActiveLayer()
{
    auto l = findLayer (state[IDs::activeLayer].toString());
    if (l.isValid() && ! isTimeLayer (l)) return l;
    for (auto c : getRulers()) if (! isTimeLayer (c) && (bool) c.getProperty (IDs::visible, true)) return c;
    for (auto c : getRulers()) if (! isTimeLayer (c)) return c;
    return {};
}

juce::String Project::layerFrame (const juce::ValueTree& layer)
{
    const auto ref = layer.getProperty (IDs::frame, "piece").toString();
    return frameFor (ref).isValid() ? ref : juce::String ("piece");
}

juce::Array<double> Project::layerDenominators (const juce::ValueTree& layer) const
{
    if (! layer.hasProperty (IDs::denominators)) return defaultDenominators();
    return Proportion::parseList (layer[IDs::denominators].toString());
}

void Project::setLayerDenominators (juce::ValueTree layer, const juce::Array<double>& list)
{
    layer.setProperty (IDs::denominators, Proportion::formatList (list), nullptr);
}

juce::var Project::layersToVar()
{
    juce::Array<juce::var> list;
    for (auto l : getRulers())
    {
        auto copy = l.createCopy();
        copy.removeProperty (IDs::id, nullptr);
        list.add (toVar (copy));
    }
    return list;
}

void Project::layersFromVar (const juce::var& v)
{
    if (! v.isArray()) return;
    auto rulers = getRulers();
    rulers.removeAllChildren (nullptr);
    for (auto& item : *v.getArray())
    {
        auto l = fromVar (item);
        if (! l.isValid() || l.getType() != IDs::LAYER) continue;
        l.setProperty (IDs::id, newId(), nullptr);
        // A preset from another project may name a box that is not here.
        if (! isTimeLayer (l) && ! frameFor (l[IDs::frame].toString()).isValid()) l.setProperty (IDs::frame, "piece", nullptr);
        rulers.appendChild (l, nullptr);
    }
    if (rulers.getNumChildren() == 0) ensureRulers();
}

juce::String Project::getRulerFrame()
{
    auto l = getActiveLayer();
    return l.isValid() ? layerFrame (l) : juce::String ("piece");
}

void Project::setRulerFrame (const juce::String& ref)
{
    auto l = getActiveLayer();
    if (! l.isValid()) { l = addLayer ("proportion"); setActiveLayer (l); }
    setLayerFrame (l, ref);
}

juce::Array<double> Project::getDenominators()
{
    auto l = getActiveLayer();
    return l.isValid() ? layerDenominators (l) : defaultDenominators();
}

void Project::setDenominators (const juce::Array<double>& list)
{
    auto l = getActiveLayer();
    if (l.isValid()) setLayerDenominators (l, list);
}

bool Project::getAutoDivisions()
{
    auto l = getActiveLayer();
    return ! l.isValid() || (bool) l.getProperty (IDs::autoDivisions, true);
}

void Project::setAutoDivisions (bool on)
{
    auto l = getActiveLayer();
    if (l.isValid()) l.setProperty (IDs::autoDivisions, on, nullptr);
}

//== Named marks ===============================================================

juce::ValueTree Project::getMarks()
{
    return state.getOrCreateChildWithName (IDs::MARKS, nullptr);
}

juce::ValueTree Project::findMark (const juce::String& markId)
{
    if (markId.isEmpty()) return {};
    return getMarks().getChildWithProperty (IDs::id, markId);
}

juce::ValueTree Project::findMarkByName (const juce::String& name)
{
    if (name.isEmpty()) return {};
    return getMarks().getChildWithProperty (IDs::name, name);
}

juce::String Project::uniqueMarkName (const juce::String& prefix)
{
    const auto base = Proportion::isValidName (prefix) ? prefix : juce::String ("m");
    for (int i = 1; i < 10000; ++i)
        if (! findMarkByName (base + juce::String (i)).isValid()) return base + juce::String (i);
    return base + newId();
}

juce::ValueTree Project::addMark (const juce::String& frameRef, const juce::String& expression, const juce::String& name)
{
    juce::ValueTree m (IDs::MARK);
    m.setProperty (IDs::id, newId(), nullptr);
    const bool free = Proportion::isValidName (name) && ! findMarkByName (name).isValid();
    m.setProperty (IDs::name, free ? name : uniqueMarkName ("m"), nullptr);
    m.setProperty (IDs::frame, frameRef, nullptr);
    m.setProperty (IDs::expr, expression, nullptr);
    getMarks().appendChild (m, &undoManager);
    return m;
}

bool Project::renameMark (juce::ValueTree mark, const juce::String& newName, juce::String& error)
{
    const auto oldName = mark[IDs::name].toString();
    if (newName == oldName) return true;
    if (! Proportion::isValidName (newName)) { error = "A mark name starts with a letter and has only letters, digits and _ (not phi, golden, pi, sqrt or of)."; return false; }
    if (findMarkByName (newName).isValid()) { error = "There is already a mark called " + newName + "."; return false; }

    auto rename = [&] (juce::ValueTree t, const juce::Identifier& id)
    {
        const auto e = t[id].toString();
        if (Proportion::refersTo (e, oldName)) t.setProperty (id, Proportion::replaceName (e, oldName, newName), &undoManager);
    };
    for (auto b : allBoxes()) { rename (b, IDs::propStart); rename (b, IDs::propLength); }
    for (auto m : getMarks()) rename (m, IDs::expr);
    mark.setProperty (IDs::name, newName, &undoManager);
    return true;
}

void Project::removeMark (juce::ValueTree mark)
{
    const auto name = mark[IDs::name].toString();
    const auto extra = getDenominators();
    // Whatever names it keeps its present value.
    auto freeze = [&] (juce::ValueTree t, const juce::Identifier& id, const juce::String& frameRef)
    {
        const auto e = t[id].toString();
        if (! Proportion::refersTo (e, name)) return;
        const auto r = evaluate (e, frameRef);
        if (r.ok) t.setProperty (id, Proportion::expressionFor (r.value, extra), &undoManager);
    };
    for (auto b : allBoxes()) { freeze (b, IDs::propStart, boundFrame (b)); freeze (b, IDs::propLength, boundFrame (b)); }
    for (auto m : getMarks()) if (m != mark) freeze (m, IDs::expr, m[IDs::frame].toString());
    getMarks().removeChild (mark, &undoManager);
}

bool Project::setMarkExpression (juce::ValueTree mark, const juce::String& text, juce::String& error)
{
    const auto r = evaluate (text, mark[IDs::frame].toString());
    if (! r.ok) { error = r.error; return false; }
    if (Proportion::refersTo (text, mark[IDs::name].toString())) { error = "A mark cannot be measured from itself."; return false; }
    mark.setProperty (IDs::expr, text.trim(), &undoManager);
    return true;
}

double Project::markTime (const juce::ValueTree& mark)
{
    const auto ref = mark[IDs::frame].toString();
    const auto f = frameFor (ref);
    if (! f.isValid() || markDepth > 16) return -1.0;
    ++markDepth;
    const auto r = evaluate (mark[IDs::expr].toString(), ref);
    --markDepth;
    return r.ok ? f.at (r.value) : -1.0;
}

Proportion::Result Project::evaluate (const juce::String& expression, const juce::String& frameRef)
{
    const auto f = frameFor (frameRef);
    return Proportion::parse (expression, [this, f] (const juce::String& name) -> std::optional<double>
    {
        auto m = findMarkByName (name);
        if (! m.isValid() || ! f.isValid()) return std::nullopt;
        const double t = markTime (m);
        if (t < 0.0) return std::nullopt;
        return f.proportionOf (t);
    });
}

void Project::pinEdge (juce::ValueTree box, bool start, const juce::String& markName)
{
    if (! isBound (box) || ! findMarkByName (markName).isValid()) return;
    if (start)
    {
        box.setProperty (IDs::propStart, markName, &undoManager);
        return;
    }
    // The end at the mark: the length is the mark minus the start.
    auto s = box[IDs::propStart].toString().trim();
    const bool simple = s.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./");
    box.setProperty (IDs::propLength, markName + " - " + (simple ? s : "(" + s + ")"), &undoManager);
}

juce::Array<juce::ValueTree> Project::splitFrameByRatio (const juce::String& frameRef, const juce::Array<double>& parts, const juce::String& prefix)
{
    juce::Array<juce::ValueTree> marks;
    if (! frameFor (frameRef).isValid()) return marks;
    for (auto& b : Proportion::partBoundaries (parts))
        marks.add (addMark (frameRef, b, uniqueMarkName (prefix)));
    return marks;
}

juce::Array<juce::ValueTree> Project::cutBoxByRatio (juce::ValueTree box, const juce::Array<double>& parts)
{
    juce::Array<juce::ValueTree> pieces { box };
    double total = 0.0;
    for (auto p : parts) total += p;
    if (total <= 0.0) return pieces;
    const double start = boxStart (box), length = boxLength (box);
    double cumulative = 0.0;
    auto current = box;
    for (int i = 0; i + 1 < parts.size(); ++i)
    {
        cumulative += parts[i];
        auto right = splitBox (current, start + length * cumulative / total);
        if (! right.isValid()) break;
        pieces.add (right);
        current = right;
    }
    return pieces;
}

//== Proportional boxes ========================================================

bool Project::dependsOn (const juce::ValueTree& box, const juce::ValueTree& frameBox)
{
    auto b = box;
    for (int guard = 0; guard < 64; ++guard)
    {
        const auto ref = boundFrame (b);
        if (ref.isEmpty() || ref == "piece" || ref == "span") return false;
        b = findBox (ref);
        if (! b.isValid()) return false;
        if (b == frameBox) return true;
    }
    return true;    // a loop: treat as dependent
}

bool Project::canUseAsFrame (const juce::ValueTree& box, const juce::String& ref)
{
    if (ref == "piece" || ref == "span" || ref.isEmpty()) return true;
    auto frameBox = findBox (ref);
    if (! frameBox.isValid() || frameBox == box) return false;
    return ! dependsOn (frameBox, box);
}

void Project::freezePieceLengthFor (const juce::String& ref)
{
    // A box bound to the piece would otherwise change the length it is measured against.
    if (ref == "piece" && ! hasSetPieceLength())
        setPieceLength (juce::jmax (1.0, getContentEnd()));
}

void Project::bindBox (juce::ValueTree box, const juce::String& ref)
{
    if (ref.isEmpty())
    {
        box.removeProperty (IDs::propFrame, &undoManager);
        return;
    }
    if (! canUseAsFrame (box, ref)) return;
    freezePieceLengthFor (ref);
    const auto f = frameFor (ref);
    if (! f.isValid()) return;

    const auto extra = getDenominators();
    box.setProperty (IDs::propStart, Proportion::expressionFor (f.proportionOf (boxStart (box)), extra), &undoManager);
    box.setProperty (IDs::propLength, Proportion::expressionFor (boxLength (box) / f.length, extra), &undoManager);
    box.setProperty (IDs::propFrame, ref, &undoManager);
    if (! box.hasProperty (IDs::propLock)) box.setProperty (IDs::propLock, true, &undoManager);
}

bool Project::setProportion (juce::ValueTree box, bool start, const juce::String& text, juce::String& error)
{
    const auto ref = boundFrame (box);
    const auto f = frameFor (ref);
    if (! f.isValid()) { error = "Choose a frame first."; return false; }

    const auto r = evaluate (text, ref);
    if (! r.ok) { error = r.error; return false; }
    if (! start && r.value * f.length < minBoxLength) { error = "That length is too short."; return false; }
    if (start && f.at (r.value) < 0.0) { error = "That start lies before 0:00."; return false; }

    box.setProperty (start ? IDs::propStart : IDs::propLength, text.trim(), &undoManager);
    if (! isLocked (box))
    {
        // Unlocked boxes take the new times once.
        if (start) box.setProperty (IDs::start, f.at (r.value), &undoManager);
        else box.setProperty (IDs::length, r.value * f.length, &undoManager);
    }
    return true;
}

void Project::setLock (juce::ValueTree box, bool locked)
{
    if (locked && ! isLocked (box))
    {
        // Take the present times as the proportions, so nothing jumps.
        const auto f = frameFor (boundFrame (box));
        if (f.isValid())
        {
            const auto extra = getDenominators();
            box.setProperty (IDs::propStart, Proportion::expressionFor (f.proportionOf (boxStart (box)), extra), &undoManager);
            box.setProperty (IDs::propLength, Proportion::expressionFor (boxLength (box) / f.length, extra), &undoManager);
        }
    }
    box.setProperty (IDs::propLock, locked, &undoManager);
}

double Project::actualProportion (const juce::ValueTree& box, bool start)
{
    const auto f = frameFor (boundFrame (box));
    if (! f.isValid()) return 0.0;
    return start ? f.proportionOf (boxStart (box)) : boxLength (box) / f.length;
}

void Project::rewriteProportions (juce::ValueTree box, double newStart, double newLength, juce::UndoManager* undo)
{
    if (! isLocked (box)) return;
    const auto f = frameFor (boundFrame (box));
    if (! f.isValid()) return;

    const auto extra = getDenominators();
    auto write = [&] (const juce::Identifier& id, double value)
    {
        // Keep what was typed when it still says the same.
        const auto old = evaluate (box[id].toString(), boundFrame (box));
        if (old.ok && std::abs (old.value - value) < 1.0e-9) return;
        box.setProperty (id, Proportion::expressionFor (value, extra), undo);
    };
    write (IDs::propStart, f.proportionOf (newStart));
    write (IDs::propLength, newLength / f.length);
}

void Project::applyProportions()
{
    const juce::ScopedValueSetter<bool> svs (applying, true);

    // Several passes, so boxes measured against boxes that just moved follow too.
    for (int pass = 0; pass < 8; ++pass)
    {
        bool any = false;
        for (auto t : getTracks())
            for (auto b : t)
            {
                const auto ref = boundFrame (b);
                if (ref.isEmpty() || ! isLocked (b)) continue;
                const auto f = frameFor (ref);
                if (! f.isValid()) continue;
                const auto ps = evaluate (b[IDs::propStart].toString(), ref);
                const auto pl = evaluate (b[IDs::propLength].toString(), ref);
                if (! ps.ok || ! pl.ok) continue;

                const double start = juce::jmax (0.0, f.at (ps.value));
                double length = juce::jmax (minBoxLength, pl.value * f.length);
                if (isSound (b))
                {
                    const double available = soundSeconds (b[IDs::soundId]) - (double) b.getProperty (IDs::offset, 0.0);
                    if (available > minBoxLength) length = juce::jmin (length, available);
                }

                if (std::abs (start - boxStart (b)) > 1.0e-9)   { b.setProperty (IDs::start, start, nullptr); any = true; }
                if (std::abs (length - boxLength (b)) > 1.0e-9) { b.setProperty (IDs::length, length, nullptr); any = true; }
            }
        if (! any) break;
    }
}

//== JSON ======================================================================
// Each tree is an object: its properties, plus "type" and (if any) "children".

juce::var Project::toVar (const juce::ValueTree& tree)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("type", tree.getType().toString());

    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const auto name = tree.getPropertyName (i);
        obj->setProperty (name, tree[name]);
    }

    if (tree.getNumChildren() > 0)
    {
        juce::Array<juce::var> children;
        for (auto c : tree)
            children.add (toVar (c));
        obj->setProperty ("children", children);
    }

    return juce::var (obj);
}

juce::ValueTree Project::fromVar (const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) return {};

    const auto type = obj->getProperty ("type").toString();
    if (type.isEmpty()) return {};

    juce::ValueTree tree { juce::Identifier (type) };

    for (auto& p : obj->getProperties())
    {
        const auto name = p.name.toString();
        if (name == "type" || name == "children") continue;
        tree.setProperty (p.name, p.value, nullptr);
    }

    if (auto* children = obj->getProperty ("children").getArray())
        for (auto& c : *children)
        {
            auto child = fromVar (c);
            if (child.isValid()) tree.appendChild (child, nullptr);
        }

    return tree;
}

juce::String Project::toJson() const
{
    return juce::JSON::toString (toVar (state), false);
}

bool Project::fromJson (const juce::String& text, juce::String& error)
{
    juce::var v;
    const auto result = juce::JSON::parse (text, v);
    if (result.failed()) { error = "Not a readable project file: " + result.getErrorMessage(); return false; }

    auto tree = fromVar (v);
    if (! tree.hasType (IDs::PROJET)) { error = "This is not a Projet file."; return false; }

    if ((int) tree[IDs::formatVersion] > currentFormatVersion)
    {
        error = "This project was saved by a newer version of Projet.";
        return false;
    }

    if (! tree.getChildWithName (IDs::SOUNDS).isValid()) tree.appendChild (juce::ValueTree (IDs::SOUNDS), nullptr);
    if (! tree.getChildWithName (IDs::TRACKS).isValid()) tree.appendChild (juce::ValueTree (IDs::TRACKS), nullptr);

    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        state.copyPropertiesAndChildrenFrom (tree, nullptr);
        ensureRulers();
    }
    applyProportions();
    undoManager.clearUndoHistory();
    return true;
}
