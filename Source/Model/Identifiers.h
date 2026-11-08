// Identifiers.h — the names used in Projet's model (a juce::ValueTree).
//
// PROJET
//   SOUNDS
//     SOUND        a placeholder copy in the project's Sounds folder
//   TRACKS
//     TRACK        a track, or a folder (isFolder). The list is flat: a folder's
//                  tracks follow it directly and name it in folderId (stage 3a)
//       BOX        a box on the timeline: "sound" or "text" (stage 2); every box
//                  has notes (Markdown) that survive a change of kind. A box can
//                  be bound to a frame (propFrame) with a proportional start and
//                  length (propStart, propLength: typed expressions, stage 3a)
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
    PROJET_ID (frameLength)           // the piece as a proportion frame, seconds (0 = its current length)
    PROJET_ID (spanStart)             // a span dragged on the proportion ruler (seconds)
    PROJET_ID (spanEnd)
    PROJET_ID (rulerFrame)            // what the proportion ruler divides: "piece", "span" or a box id
    PROJET_ID (denominators)          // the ruler's divisions, e.g. "2,3,4,5,6,8" (decimals allowed)
    PROJET_ID (autoDivisions)         // finer divisions appear as you zoom in

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
    PROJET_ID (isFolder)
    PROJET_ID (folderId)              // the folder a track is in ("" at the top)
    PROJET_ID (folded)                // a folder's tracks are hidden

    // Box
    PROJET_ID (kind)                  // "sound", "text"; "container", "board", ... later
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
    PROJET_ID (notes)                 // Markdown, kept whatever the kind
    PROJET_ID (propFrame)             // "" (none), "piece", "span" or a box id
    PROJET_ID (propStart)             // expression, e.g. "3/11"
    PROJET_ID (propLength)
    PROJET_ID (propLock)              // follow the frame when it changes (default on)

    #undef PROJET_ID
}
