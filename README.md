# Projet

A sketch timeline for acousmatic pieces: lay out a piece's conceptual skeleton in boxes on tracks, nest boxes inside boxes, fill them with notes and placeholder sounds, and later hand the structure to Reaper. Sibling of [Trajet](https://github.com/Joostiepoost/trajet) and Effet.

macOS, Apple Silicon only. Built with [JUCE](https://juce.com) 9 and CMake. The full design is in the Projet proposal doc.

## Building

Requirements: Xcode (opened once, licence accepted), Homebrew, CMake (`brew install cmake`) — the same as for Trajet and Effet.

In Terminal, from this folder:

```
chmod +x build.sh   # only the first time
./build.sh
```

The first build downloads JUCE and takes a few minutes; later builds are much faster. The app opens when the build finishes, and lives at `build/Projet_artefacts/Release/Projet.app`.

To skip the JUCE download, point the build at the copy Trajet already downloaded:

```
cmake -B build -DPROJET_JUCE_PATH=../trajet/build/_deps/juce-src
./build.sh
```

(Adjust the path to where your Trajet folder is. Only needed once; the setting is remembered.)

Other options:

- `./build.sh debug` — Debug build (slower, more checks)
- `./build.sh xcode` — generate and open an Xcode project, for debugging
- `./build.sh tests` — build and run the measurement program (see *Measurements* below)

## Status

| Stage | Content | State |
| --- | --- | --- |
| 1 | Engine and project folder: import as placeholder copies, tracks, sound boxes, playback with gain, fades, crossfades; measurements | testing |
| 2 | Editing: text boxes, rich-text notes editor, type switching, split/copy, undo everywhere | |
| 3 | Rulers and tracks: proportion ruler, fraction snapping, track folders | |
| 4 | Framework, layers 1–3: energies and semiotic levels, shapes and fills, colour modes, track template, overview lanes | |
| 5 | Nesting: container boxes, window/stretch, dive-in, outline panel | |
| 6 | Magnified inline expansion | |
| 7 | Takes | |
| 8 | Relations, materials, origin | |
| 9 | Framework, layer 4: factor curves | |
| 10 | Strata view | |
| 11 | Board box | |
| 12 | Reaper export | |

## Using stage 1

### The project folder

Every piece is a folder:

```
Étude II/
  Étude II.projet    the piece (readable JSON)
  Sounds/            placeholder copies of every imported sound
  Boards/            (later stages)
  Exports/           (later stages)
  Backups/           the previous version of Étude II.projet, one per save (the last 30 are kept)
```

- **File > New project (Cmd-N)**: name the project; a folder of that name is made where you choose. Then pick the **sample rate** (44.1, 48, 88.2 or 96 kHz; default 48). Every imported sound is converted to it, so it cannot be changed later.
- **Open project (Cmd-O)**, **Open recent**, **Save (Cmd-S)**. Sounds are stored with paths relative to the folder, so the folder can be moved or renamed as a whole.
- The first time Projet starts it makes `~/Music/Projet/Untitled`; after that it reopens the last project.
- The window and the bar at the top show `*` when there are unsaved changes; New, Open and Quit ask first.
- **Show project folder in Finder** (File menu).

### Importing sounds

- **Import sounds (Cmd-I)** or the *Import...* button: the sounds go on the selected track, one after the other, from the playhead.
- Or **drop sound files from the Finder** onto a track (below the last track makes a new one), at the place where you drop them.
- Accepted: WAV, AIFF, FLAC, Ogg, MP3, AAC/M4A, CAF.
- Each sound becomes a **placeholder copy** in `Sounds/`: stereo (mono is doubled), at the project rate (converted with Effet's sinc kernel when needed), encoded as **Ogg Vorbis q0.7** (default, about 1.6 MB per minute) or **FLAC 24-bit** (lossless, about 14 MB per minute). Choose in the panel on the right with nothing selected; it applies to new imports.
- The original is never changed; its path is kept (box panel: *Show original in Finder*), so it can be swapped for the production file later.
- Importing the same file again reuses the copy that is already there.
- Imports run in the background, several at a time; the status line shows progress.

### Tracks and boxes

- **Ruler**: click or drag to move the playhead.
- **Boxes**: click to select; drag to move, also to another track. Boxes may overlap.
- **Edges**: drag to trim. The sound stays in place under the box (the left edge moves into or out of the sound).
- **Fades**: drag the small white squares at the top corners.
- Moving and trimming snap to box edges, the playhead and 0:00; hold **Shift** to move freely.
- **Right-click a box**: Duplicate, Delete, Show original sound in Finder. Cmd-D duplicates, Delete/Backspace deletes.
- **Track header**: **M** mutes, **S** solos (several tracks can be soloed). Double-click the name to rename. Right-click: rename, colour, add a track above or below, delete. **+ Track** at the bottom (or Shift-Cmd-N) adds one.
- **Wheel**: scroll up and down; Shift-wheel or a sideways swipe scrolls in time. **Cmd-wheel or pinch** zooms; Cmd-= / Cmd-- / Cmd-0 (the whole piece).
- **Undo / Redo** (Cmd-Z / Shift-Cmd-Z) for every change to tracks and boxes.

### The panel on the right

- **A box selected**: name, gain (−60 to +12 dB; double-click for 0), fade in and out with their shapes (linear, equal power, S-curve), **opacity** (visual only: overlapping boxes show through), start, end, length and offset into the sound, and the sound (copy, original, rates).
- **Nothing selected**: the project: sample rate, number of tracks, boxes and sounds, size of the copies, length, folder; the format for new imports and the Vorbis quality.

### Playback

- **Space** plays and stops; **Return** goes back to 0:00. The playhead follows during playback.
- Overlapping boxes simply add up; their fades make the crossfade. For a crossfade between unrelated sounds use **equal power** on both sides; for two copies of the same sound use **linear** or **S-curve**.
- **Audio > Audio settings (Cmd-,)**: output device and sample rate. Set the device to the project's rate: then every sample is played exactly as stored. At another rate Projet still plays correctly (through the sinc kernel), and the status line says so.
- The meters top right show the output peak.

**Worth testing**:
- Import a few sounds at different rates (44.1, 48, 96 kHz) and formats (WAV, AIFF, MP3): placement, length, sound.
- Overlap two boxes with fades and listen to the crossfade with each fade shape.
- Trim from the left and right, move between tracks, undo everything.
- Save, quit, reopen; move the project folder and open it again.
- The device at another rate than the project.
- A long piece with many boxes: scrolling, zooming and the playhead.

**Known limits of stage 1**: all sounds of the project are held in memory as 32-bit float (about 23 MB per stereo minute at 48 kHz); disk streaming can come later if pieces get very large. Sounds that are no longer used stay in `Sounds/` (a clean-up command comes later).

## Measurements

`./build.sh tests` (add `--quick` for a short run) builds and runs `ProjetTests`, which writes `Measurements/report.md`:

1. **Import**: clicks at known times must land on the same place in the placeholder copy (Ogg and FLAC, sources at 44.1, 48 and 96 kHz), with the right length and level.
2. **Playback timing**: boxes must sound on exactly the sample they start on, at any block size.
3. **Locating** inside a box.
4. **Fades and crossfades**: the gain of each shape, and the summed level and power of overlapping boxes.
5. **Device at another rate** than the project.
6. **CPU**.
7. **Import speed and size**, Ogg versus FLAC.

`Measurements/report-cloud-test-machine.md` is a run on the (slower) cloud machine where the code was written; your M4 should be roughly two to three times faster.
