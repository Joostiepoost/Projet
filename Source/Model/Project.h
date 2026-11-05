// Project.h — Projet's piece: sounds, tracks and boxes (a juce::ValueTree with
// undo). Saved as readable JSON in the project folder (see ProjectFolder.h).
//
// Boxes may overlap on a track (sounds blend into each other); opacity is
// visual only.

#pragma once

#include <JuceHeader.h>
#include "Identifiers.h"

enum class FadeShape { linear = 0, equalPower, sCurve };
constexpr int numFadeShapes = 3;
const char* fadeShapeName (FadeShape);

class Project : private juce::ValueTree::Listener
{
public:
    static constexpr int currentFormatVersion = 1;
    static constexpr double defaultSampleRate = 48000.0;
    static constexpr double minBoxLength = 0.005;

    Project();
    ~Project() override;

    juce::ValueTree& getState() noexcept            { return state; }
    juce::UndoManager& getUndoManager() noexcept    { return undoManager; }
    juce::UndoManager* um() noexcept                { return &undoManager; }

    /** A fresh project with one empty track. */
    void resetToDefault (const juce::String& name, double sampleRate = defaultSampleRate);

    juce::String getName() const                    { return state[IDs::name].toString(); }
    double getSampleRate() const                    { return (double) state.getProperty (IDs::sampleRate, defaultSampleRate); }
    juce::String getPlaceholderFormat() const       { return state.getProperty (IDs::placeholderFormat, "ogg").toString(); }
    int getVorbisQuality() const                    { return (int) state.getProperty (IDs::vorbisQuality, 7); }

    //== Sounds ===============================================================
    juce::ValueTree getSounds()                     { return state.getChildWithName (IDs::SOUNDS); }
    juce::ValueTree findSound (const juce::String& soundId) const;
    /** An earlier import of the same file (same contents) in the same placeholder format. */
    juce::ValueTree findSoundByHash (const juce::String& hash, const juce::String& format) const;
    /** Adds a SOUND record (not undoable: the file exists on disk either way). */
    juce::ValueTree addSound (const juce::ValueTree& soundRecord);
    double soundSeconds (const juce::String& soundId) const;

    //== Tracks ===============================================================
    juce::ValueTree getTracks()                     { return state.getChildWithName (IDs::TRACKS); }
    int getNumTracks()                              { return getTracks().getNumChildren(); }
    juce::ValueTree getTrack (int index)            { return getTracks().getChild (index); }
    juce::ValueTree addTrack (const juce::String& name = {}, int index = -1);
    void removeTrack (juce::ValueTree track);

    //== Boxes ================================================================
    /** A sound box for the whole sound, at `start` seconds. */
    juce::ValueTree addSoundBox (juce::ValueTree track, double start, const juce::String& soundId);
    void removeBox (juce::ValueTree box);
    juce::ValueTree duplicateBox (juce::ValueTree box);
    void moveBox (juce::ValueTree box, juce::ValueTree toTrack, double newStart);
    juce::ValueTree findBox (const juce::String& boxId);
    juce::ValueTree trackOf (const juce::ValueTree& box) const     { return box.getParent(); }

    static double boxStart (const juce::ValueTree& b)              { return (double) b[IDs::start]; }
    static double boxLength (const juce::ValueTree& b)             { return (double) b[IDs::length]; }
    static double boxEnd (const juce::ValueTree& b)                { return boxStart (b) + boxLength (b); }

    /** Moves the box's left edge to `newStart` (the sound stays in place under it). */
    void trimStart (juce::ValueTree box, double newStart);
    /** Moves the box's right edge to `newEnd`. */
    void trimEnd (juce::ValueTree box, double newEnd);
    /** Fades are clamped so fade in + fade out never exceed the box. */
    void setFades (juce::ValueTree box, double fadeIn, double fadeOut);

    /** End of the last box (0 if there are none). */
    double getContentEnd();

    /** Called after any change to the model (also undo/redo). */
    std::function<void()> onChanged;

    //== JSON =================================================================
    static juce::var toVar (const juce::ValueTree&);
    static juce::ValueTree fromVar (const juce::var&);
    juce::String toJson() const;
    /** Replaces the state; returns false (state unchanged) if the text is not a Projet file. */
    bool fromJson (const juce::String&, juce::String& error);

    static juce::String newId()                     { return juce::Uuid().toString(); }

private:
    static juce::ValueTree createState (const juce::String& name, double rate);
    void changed()                                  { if (onChanged) onChanged(); }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { changed(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { changed(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { changed(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { changed(); }
    void valueTreeRedirected (juce::ValueTree&) override                                 { changed(); }

    juce::ValueTree state;
    juce::UndoManager undoManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};
