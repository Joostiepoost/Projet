// Identifiers.h — the names used in Projet's model (a juce::ValueTree).
//
// PROJET
//   SOUNDS
//     SOUND        a placeholder copy in the project's Sounds folder
//   TRACKS
//     TRACK
//       BOX        a box on the timeline; stage 1 knows sound boxes only
//
// Times are in seconds (doubles); the engine turns them into samples at the
// project's sample rate.

#pragma once

#include <JuceHeader.h>

namespace IDs
{
    #define PROJET_ID(name) inline const juce::Identifier name { #name };

    // Types
    PROJET_ID (PROJET)
    PROJET_ID (SOUNDS)
    PROJET_ID (SOUND)
    PROJET_ID (TRACKS)
    PROJET_ID (TRACK)
    PROJET_ID (BOX)

    // Project
    PROJET_ID (formatVersion)
    PROJET_ID (sampleRate)
    PROJET_ID (placeholderFormat)     // "ogg" or "flac"
    PROJET_ID (vorbisQuality)         // 0..10 (JUCE quality index; q = index / 10)
    PROJET_ID (viewStart)             // seconds at the left edge of the timeline
    PROJET_ID (pixelsPerSecond)
    PROJET_ID (playhead)

    // Common
    PROJET_ID (id)
    PROJET_ID (name)
    PROJET_ID (colour)

    // Sound
    PROJET_ID (file)                  // relative to the project folder
    PROJET_ID (originalPath)          // the file that was imported
    PROJET_ID (originalHash)          // MD5 of the original file
    PROJET_ID (lengthSamples)         // at the project rate
    PROJET_ID (originalRate)
    PROJET_ID (originalChannels)
    PROJET_ID (format)

    // Track
    PROJET_ID (height)
    PROJET_ID (mute)
    PROJET_ID (solo)

    // Box
    PROJET_ID (kind)                  // "sound" (stage 1); "text", "container", ... later
    PROJET_ID (start)                 // seconds on the timeline
    PROJET_ID (length)                // seconds
    PROJET_ID (soundId)
    PROJET_ID (offset)                // seconds into the sound where the box begins
    PROJET_ID (gainDb)
    PROJET_ID (fadeIn)                // seconds
    PROJET_ID (fadeOut)               // seconds
    PROJET_ID (fadeInShape)           // 0 linear, 1 equal power, 2 S-curve
    PROJET_ID (fadeOutShape)
    PROJET_ID (opacity)               // 0..1, visual only

    #undef PROJET_ID
}
