// Project.cpp

#include "Project.h"

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
    state.copyPropertiesAndChildrenFrom (createState (name, sampleRate), nullptr);
    addTrack ("Track 1");
    undoManager.clearUndoHistory();
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

juce::ValueTree Project::addTrack (const juce::String& name, int index)
{
    auto tracks = getTracks();
    juce::ValueTree t (IDs::TRACK);
    t.setProperty (IDs::id, newId(), nullptr);
    t.setProperty (IDs::name, name.isNotEmpty() ? name : "Track " + juce::String (tracks.getNumChildren() + 1), nullptr);
    t.setProperty (IDs::colour, tracks.getNumChildren() % 8, nullptr);
    t.setProperty (IDs::height, 72, nullptr);
    t.setProperty (IDs::mute, false, nullptr);
    t.setProperty (IDs::solo, false, nullptr);
    tracks.addChild (t, index, &undoManager);
    return t;
}

void Project::removeTrack (juce::ValueTree track)
{
    getTracks().removeChild (track, &undoManager);
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
    track.appendChild (b, &undoManager);
    return b;
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
    box.getParent().appendChild (copy, &undoManager);
    return copy;
}

void Project::moveBox (juce::ValueTree box, juce::ValueTree toTrack, double newStart)
{
    newStart = juce::jmax (0.0, newStart);
    auto from = box.getParent();

    if (toTrack.isValid() && from != toTrack)
    {
        from.removeChild (box, &undoManager);
        toTrack.appendChild (box, &undoManager);
    }

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
    const double offset = (double) box[IDs::offset];
    // The sound may not start before its first sample; the box keeps a minimum length.
    newStart = juce::jlimit (juce::jmax (0.0, boxStart (box) - offset), end - minBoxLength, newStart);
    const double delta = newStart - boxStart (box);

    box.setProperty (IDs::start, newStart, &undoManager);
    box.setProperty (IDs::offset, offset + delta, &undoManager);
    box.setProperty (IDs::length, end - newStart, &undoManager);
    setFades (box, box[IDs::fadeIn], box[IDs::fadeOut]);
}

void Project::trimEnd (juce::ValueTree box, double newEnd)
{
    const double available = soundSeconds (box[IDs::soundId]) - (double) box[IDs::offset];
    const double length = juce::jlimit (minBoxLength, juce::jmax (minBoxLength, available), newEnd - boxStart (box));
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

    state.copyPropertiesAndChildrenFrom (tree, nullptr);
    undoManager.clearUndoHistory();
    return true;
}
