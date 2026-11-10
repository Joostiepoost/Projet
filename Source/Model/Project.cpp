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
    joinsValid = false;
    ++changeCount;
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
    {
        const auto id = tracks.getChild (i)[IDs::id].toString();
        for (int n = getNodes().getNumChildren(); --n >= 0;)                 // a folder's nodes go with it
            if (getNodes().getChild (n)[IDs::folderId].toString() == id) getNodes().removeChild (n, &undoManager);
        tracks.removeChild (i, &undoManager);
    }
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

juce::Array<juce::ValueTree> Project::addFoldersFor (const Framework::Category& c)
{
    juce::Array<juce::ValueTree> made;
    if (framework == nullptr || ! c.folders || c.foldersOnly || c.kind != Framework::Kind::choice) return made;
    auto values = framework->valuesOf (c);
    std::stable_sort (values.begin(), values.end(), [] (const Framework::Value& a, const Framework::Value& b) { return a.order < b.order; });
    for (auto& v : values)
    {
        auto f = addFolder (framework->valueName (c, v.id), -1);
        f.setProperty (c.property(), v.id, &undoManager);
        addTrack ({}, -1, f[IDs::id].toString());
        made.add (f);
    }
    return made;
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

    // Changes along the box (stage 4a, 4b) are proportions of it: each part
    // keeps its own. Every fw.<package>.<category>.along field, whatever the
    // framework: the model needs no framework to split.
    const double cut = (t - start) / (end - start);
    juce::Array<juce::Identifier> alongFields;
    for (int i = 0; i < box.getNumProperties(); ++i)
    {
        const auto name = box.getPropertyName (i).toString();
        if (name.startsWith ("fw.") && name.endsWith (".along")) alongFields.add (box.getPropertyName (i));
    }
    for (auto& alongId : alongFields)
    {
        const auto changes = Along::parse (box[alongId].toString());
        const juce::Identifier baseId (alongId.toString().dropLastCharacters (6));
        juce::String leftBase, rightBase;
        juce::Array<Along::Change> leftChanges, rightChanges;
        const auto base = box[baseId].toString();
        Along::section (base, changes, 0.0, cut, leftBase, leftChanges);
        Along::section (base, changes, cut, 1.0, rightBase, rightChanges);
        if (rightBase.isEmpty()) right.removeProperty (baseId, nullptr); else right.setProperty (baseId, rightBase, nullptr);
        if (rightChanges.isEmpty()) right.removeProperty (alongId, nullptr); else right.setProperty (alongId, Along::format (rightChanges), nullptr);
        const auto leftText = Along::format (leftChanges);
        if (leftText.isEmpty()) box.removeProperty (alongId, &undoManager);
        else if (leftText != box[alongId].toString()) box.setProperty (alongId, leftText, &undoManager);
    }

    // Stage 4c: the join with the box before stays with the left part (the
    // right one now follows it closely); how the box ends goes with the right.
    if (framework != nullptr)
        for (auto& c : framework->getCategories())
        {
            if (c.between) right.removeProperty (c.property(), nullptr);
            if (c.atEnd && box.hasProperty (c.property())) box.removeProperty (c.property(), &undoManager);
        }

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
    state.getOrCreateChildWithName (IDs::DRAWN, nullptr);      // stage 4d.2
    state.getOrCreateChildWithName (IDs::NODES, nullptr);
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

juce::ValueTree Project::addLaneLayer (const juce::String& laneKey, const juce::String& name, int height, int index)
{
    juce::ValueTree l (IDs::LAYER);
    l.setProperty (IDs::id, newId(), nullptr);
    l.setProperty (IDs::kind, "lane", nullptr);
    l.setProperty (IDs::lane, laneKey, nullptr);
    l.setProperty (IDs::name, name.isNotEmpty() ? name : laneKey, nullptr);
    l.setProperty (IDs::height, height, nullptr);
    l.setProperty (IDs::visible, true, nullptr);
    getRulers().addChild (l, index, nullptr);
    return l;
}

void Project::removeLayer (juce::ValueTree layer)
{
    // The last time layer stays: it is where you move the playhead.
    if (isTimeLayer (layer))
    {
        int timeLayers = 0;
        for (auto l : getRulers()) if (isTimeLayer (l)) ++timeLayers;
        if (timeLayers <= 1) return;
    }
    getRulers().removeChild (layer, nullptr);
}

juce::ValueTree Project::getActiveLayer()
{
    auto l = findLayer (state[IDs::activeLayer].toString());
    if (isProportionLayer (l)) return l;
    for (auto c : getRulers()) if (isProportionLayer (c) && (bool) c.getProperty (IDs::visible, true)) return c;
    for (auto c : getRulers()) if (isProportionLayer (c)) return c;
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
        if (l.hasProperty (IDs::frame) && ! frameFor (l[IDs::frame].toString()).isValid()) l.setProperty (IDs::frame, "piece", nullptr);
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

//== Drawn lanes and nodes (stage 4d.2) =======================================

juce::ValueTree Project::getDrawn()                 { return state.getOrCreateChildWithName (IDs::DRAWN, nullptr); }
juce::ValueTree Project::getNodes()                 { return state.getOrCreateChildWithName (IDs::NODES, nullptr); }

juce::Array<juce::ValueTree> Project::drawnItems (const juce::String& laneKey)
{
    juce::Array<juce::ValueTree> out;
    for (auto c : getDrawn()) if (c[IDs::lane].toString() == laneKey) out.add (c);
    return out;
}

juce::ValueTree Project::addSpan (const juce::String& laneKey, double start, double end, const juce::String& kind)
{
    juce::ValueTree s (IDs::ITEM);
    s.setProperty (IDs::id, newId(), nullptr);
    s.setProperty (IDs::lane, laneKey, nullptr);
    s.setProperty (IDs::kind, "span", nullptr);
    s.setProperty (IDs::start, juce::jmin (start, end), nullptr);
    s.setProperty (IDs::end, juce::jmax (start, end), nullptr);
    s.setProperty (IDs::value, kind, nullptr);
    getDrawn().appendChild (s, &undoManager);
    return s;
}

juce::ValueTree Project::addPoint (const juce::String& laneKey, double time, const juce::String& functions)
{
    juce::ValueTree s (IDs::ITEM);
    s.setProperty (IDs::id, newId(), nullptr);
    s.setProperty (IDs::lane, laneKey, nullptr);
    s.setProperty (IDs::kind, "point", nullptr);
    s.setProperty (IDs::start, juce::jmax (0.0, time), nullptr);
    s.setProperty (IDs::value, functions, nullptr);
    getDrawn().appendChild (s, &undoManager);
    return s;
}

juce::ValueTree Project::findItem (const juce::String& itemId)
{
    return itemId.isEmpty() ? juce::ValueTree() : getDrawn().getChildWithProperty (IDs::id, itemId);
}

void Project::removeItem (juce::ValueTree item)
{
    if (item.getParent() == getDrawn()) getDrawn().removeChild (item, &undoManager);
}

juce::Array<juce::ValueTree> Project::nodesOf (const juce::ValueTree& folder)
{
    juce::Array<juce::ValueTree> out;
    const auto id = folder[IDs::id].toString();
    for (auto n : getNodes()) if (n[IDs::folderId].toString() == id) out.add (n);
    std::sort (out.begin(), out.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
               { return (double) a[IDs::start] < (double) b[IDs::start]; });
    return out;
}

juce::ValueTree Project::addNode (const juce::ValueTree& folder, double start, double length)
{
    juce::ValueTree n (IDs::NODE);
    n.setProperty (IDs::id, newId(), nullptr);
    n.setProperty (IDs::folderId, folder[IDs::id].toString(), nullptr);
    n.setProperty (IDs::start, juce::jmax (0.0, start), nullptr);
    n.setProperty (IDs::length, juce::jmax (0.001, length), nullptr);
    getNodes().appendChild (n, &undoManager);
    return n;
}

juce::ValueTree Project::findNode (const juce::String& nodeId)
{
    return nodeId.isEmpty() ? juce::ValueTree() : getNodes().getChildWithProperty (IDs::id, nodeId);
}

void Project::removeNode (juce::ValueTree node)
{
    if (node.getParent() == getNodes()) getNodes().removeChild (node, &undoManager);
}

juce::Array<juce::ValueTree> Project::tracksIn (const juce::ValueTree& folder)
{
    juce::Array<juce::ValueTree> out;
    const auto id = folder[IDs::id].toString();
    for (auto t : getTracks())
    {
        if (isFolder (t)) continue;
        for (auto f = folderOf (t); f.isValid(); f = folderOf (f))
            if (f[IDs::id].toString() == id) { out.add (t); break; }
    }
    return out;
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

//== Framework values (stage 4b) ===============================================

juce::String Project::folderValue (const juce::ValueTree& track, const juce::Identifier& property)
{
    int guard = 0;
    for (auto f = isFolder (track) ? track : folderOf (track); f.isValid() && guard < 64; f = folderOf (f), ++guard)
        if (f[property].toString().isNotEmpty())
            return f[property].toString();
    return {};
}

Project::FrameworkValue Project::frameworkValue (const juce::ValueTree& box, const Framework::Category& c, bool ignoreOwn)
{
    FrameworkValue v;
    if (! c.forBoxes() || ! applies (box, c)) return v;
    const auto& own = box[c.property()];
    if (! ignoreOwn && ! own.isVoid() && own.toString().isNotEmpty())
    {
        if (! valueApplies (box, c, own.toString())) return v;
        v.value = own.toString();
        v.from = FrameworkValue::From::own;
        return v;
    }
    if (c.between)
    {
        // A join without a value of its own: what the timing says.
        v.value = derivedValue (c, joinBefore (box));
        if (v.value.isNotEmpty() && valueApplies (box, c, v.value)) { v.from = FrameworkValue::From::timing; return v; }
        v.value = {};
        return impliedValue (box, c);
    }
    if (c.folders && box.getParent().isValid())
    {
        v.value = folderValue (box.getParent(), c.property());
        if (v.value.isNotEmpty()) { v.from = FrameworkValue::From::folder; return v; }
    }
    if (auto implied = impliedValue (box, c); implied.value.isNotEmpty()) return implied;
    if (c.defaultValue.isNotEmpty()) { v.value = c.defaultValue; v.from = FrameworkValue::From::defaultValue; }
    return v;
}

Project::FrameworkValue Project::impliedValue (const juce::ValueTree& box, const Framework::Category& c)
{
    // A value the box's folders give through another category (stage 4d):
    // the nearest folder with a value for that category decides.
    FrameworkValue v;
    if (framework == nullptr || ! box.getParent().isValid()) return v;
    for (auto& imp : framework->implicationsFor (c))
    {
        auto& source = framework->getCategories().getReference (imp.source);
        if (folderValue (box.getParent(), source.property()) != imp.value) continue;
        if (! valueApplies (box, c, imp.implied)) continue;
        v.value = imp.implied;
        v.from = FrameworkValue::From::implied;
        v.source = source.key();
        return v;
    }
    return v;
}

bool Project::holds (const juce::ValueTree& box, const Framework::Condition& cond)
{
    if (! cond.isSet() || framework == nullptr) return true;
    auto* other = framework->findCategory (cond.category);
    if (other == nullptr || conditionDepth > 4) return true;     // a loop of conditions is not followed
    ++conditionDepth;
    const auto v = frameworkValue (box, *other);
    --conditionDepth;
    return cond.values.contains (v.value);
}

bool Project::applies (const juce::ValueTree& box, const Framework::Category& c)
{
    return holds (box, c.onlyWith);
}

bool Project::valueApplies (const juce::ValueTree& box, const Framework::Category& c, const juce::String& valueId)
{
    if (framework == nullptr) return true;
    auto* value = framework->findValue (c, valueId);
    return value == nullptr || holds (box, value->onlyWith);
}

juce::String Project::derivedValue (const Framework::Category& c, const Join& join)
{
    if (! join.isValid()) return {};
    for (auto& rule : c.derive)
    {
        if (join.gap < rule.from || join.gap >= rule.to) continue;
        if (rule.covered >= 0 && join.covered != (rule.covered == 1)) continue;
        return rule.value;
    }
    return {};
}

void Project::buildJoins()
{
    joins.clear();
    joinsValid = true;
    constexpr double eps = 1.0e-9;

    // Every box that sounds or plans a sound, with its track, for "covered".
    struct Span { double start, end; juce::String track; };
    std::vector<Span> spans;
    for (auto t : getTracks())
    {
        if (isFolder (t) || effectiveMute (t)) continue;
        const auto trackId = t[IDs::id].toString();
        for (auto b : t)
            if (b.hasType (IDs::BOX)) spans.push_back ({ boxStart (b), boxEnd (b), trackId });
    }

    for (auto t : getTracks())
    {
        if (isFolder (t)) continue;
        std::vector<juce::ValueTree> boxes;
        for (auto b : t) if (b.hasType (IDs::BOX)) boxes.push_back (b);
        // By start; boxes starting together keep their order on the track.
        std::stable_sort (boxes.begin(), boxes.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
                          { return boxStart (a) < boxStart (b); });
        const auto trackId = t[IDs::id].toString();
        for (size_t i = 1; i < boxes.size(); ++i)
        {
            const auto& a = boxes[i - 1];
            const auto& b = boxes[i];
            JoinInfo info;
            info.before = a[IDs::id].toString();
            info.gap = boxStart (b) - boxEnd (a);
            if (info.gap > 0.0)
                for (auto& s : spans)
                    if (s.track != trackId && s.start <= boxEnd (a) + eps && s.end >= boxStart (b) - eps) { info.covered = true; break; }
            const auto id = b[IDs::id].toString();
            auto& entry = joins[id];
            entry.before = info.before;
            entry.gap = info.gap;
            entry.covered = info.covered;
            joins[info.before].after = id;
        }
    }
}

Project::Join Project::joinBefore (const juce::ValueTree& box)
{
    if (! joinsValid) buildJoins();
    Join j;
    const auto it = joins.find (box[IDs::id].toString());
    if (it == joins.end() || it->second.before.isEmpty()) return j;
    j.before = box.getParent().getChildWithProperty (IDs::id, it->second.before);
    j.gap = it->second.gap;
    j.covered = it->second.covered;
    return j;
}

juce::ValueTree Project::boxAfter (const juce::ValueTree& box)
{
    if (! joinsValid) buildJoins();
    const auto it = joins.find (box[IDs::id].toString());
    if (it == joins.end() || it->second.after.isEmpty()) return {};
    return box.getParent().getChildWithProperty (IDs::id, it->second.after);
}

void Project::setFrameworkValue (juce::ValueTree target, const Framework::Category& c, const juce::var& value)
{
    if (value.isVoid() || value.toString().isEmpty()) target.removeProperty (c.property(), &undoManager);
    else if (! (target[c.property()] == value)) target.setProperty (c.property(), value, &undoManager);
}

void Project::setChangesAlong (juce::ValueTree box, const Framework::Category& c, const juce::Array<Along::Change>& changes)
{
    const auto text = Along::format (changes);
    if (text == box[c.alongProperty()].toString()) return;
    if (text.isEmpty()) box.removeProperty (c.alongProperty(), &undoManager);
    else box.setProperty (c.alongProperty(), text, &undoManager);
}

juce::StringArray Project::valuesShown (const juce::ValueTree& box, const Framework::Category& c)
{
    const auto v = frameworkValue (box, c);
    if (c.kind == Framework::Kind::tags) return Tags::parse (v.value);
    juce::StringArray out;
    if (v.value.isNotEmpty()) out.add (v.value);
    if (c.along)
        for (auto& change : changesAlong (box, c)) out.addIfNotAlreadyThere (change.to);
    return out;
}

bool Project::migrateFrameworkFields (juce::ValueTree tree)
{
    // 4a stored the framework in fixed fields; 4b in fw.<package>.<category>.
    const auto fields = Framework::legacyFields();
    bool changed = false;
    std::function<void (juce::ValueTree)> walk = [&] (juce::ValueTree node)
    {
        if (node.hasType (IDs::TRACK) || node.hasType (IDs::BOX))
            for (int i = 0; i < fields.size(); ++i)
            {
                const juce::Identifier from (fields.getAllKeys()[i]), to (fields.getAllValues()[i]);
                if (! node.hasProperty (from)) continue;
                if (! node.hasProperty (to)) node.setProperty (to, node[from], nullptr);
                node.removeProperty (from, nullptr);
                changed = true;
            }
        for (auto child : node) walk (child);
    };
    walk (tree);

    const auto mode = tree[IDs::colourMode].toString();
    const juce::String newMode = mode == "family" ? "composition.energy" : mode == "level" ? "composition.level"
                               : mode == "type" ? "schaeffer.type" : juce::String();
    if (newMode.isNotEmpty()) { tree.setProperty (IDs::colourMode, newMode, nullptr); changed = true; }
    return changed;
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

    migrateFrameworkFields (tree);     // a 4a project: its framework fields get their 4b keys
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
