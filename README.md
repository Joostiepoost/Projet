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

If the build prints many `ld: warning: linking module flags 'SDK Version'` lines, part of the build folder was compiled with an older Xcode/macOS SDK. They are harmless; a clean build removes them: `rm -rf build && ./build.sh`.

Other options:

- `./build.sh debug` — Debug build (slower, more checks)
- `./build.sh xcode` — generate and open an Xcode project, for debugging
- `./build.sh tests` — build and run the measurement program (see *Measurements* below)

## Status

| Stage | Content | State |
| --- | --- | --- |
| 1 | Engine and project folder: import as placeholder copies, tracks, sound boxes, playback with gain, fades, crossfades; measurements | done |
| 1.1 | Start window, import progress panel with Cancel, faster rate conversion, playhead trace fixed | done |
| 2 | Editing: text boxes, rich-text notes editor, type switching, multi-selection, split/copy/paste, undo everywhere | done |
| 3a | Proportions and folders: proportion frames, typed proportions, divisions that follow the zoom, snapping with a readout, track folders | done |
| 3b | Ruler layers and presets, named marks, ratio splits, series (golden, Fibonacci, harmonic, your own), ratio checking | built, to test |
| 4 | Framework, layers 1–3: energies and semiotic levels, shapes and fills, colour modes, track template, overview lanes | |
| 5 | Nesting: container boxes, window/stretch, dive-in, outline panel | |
| 6 | Magnified inline expansion | |
| 7 | Takes | |
| 8 | Relations, materials, origin | |
| 9 | Framework, layer 4: factor curves | |
| 10 | Strata view | |
| 11 | Board box | |
| 12 | Reaper export | |

## Using Projet (stages 1–3b)

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
- Projet reopens the last project. When there is none (first start, or the folder has moved), a **start window** offers *New project*, *Open project* and the recent projects; Projet never makes a folder you did not choose. (Stage 1 made `~/Music/Projet/Untitled` automatically; you can delete that folder.)
- The window and the bar at the top show `*` when there are unsaved changes; New, Open and Quit ask first.
- **Show project folder in Finder** (File menu).

### Importing sounds

- **Import sounds (Cmd-I)** or the *Import...* button: the sounds go on the selected track, one after the other, from the playhead.
- Or **drop sound files from the Finder** onto a track (below the last track makes a new one), at the place where you drop them.
- Accepted: WAV, AIFF, FLAC, Ogg, MP3, AAC/M4A, CAF.
- Each sound becomes a **placeholder copy** in `Sounds/`: stereo (mono is doubled), at the project rate (converted with Effet's sinc kernel when needed), encoded as **Ogg Vorbis q0.7** (default, about 1.6 MB per minute) or **FLAC 24-bit** (lossless, about 14 MB per minute). Choose in the panel on the right with nothing selected; it applies to new imports.
- The original is never changed; its path is kept (box panel: *Show original in Finder*), so it can be swapped for the production file later.
- Importing the same file again reuses the copy that is already there.
- Imports run in the background, two files at a time, and a **progress panel** appears at the bottom right of the timeline: one row per file with its step (reading, converting the sample rate, encoding) and a bar. **Cancel** stops the files still running and removes their unfinished copies. The panel closes by itself shortly after the last file.
- Rate conversion uses a precomputed filter and all cores (the same quality as before, about 35 times faster for 96 kHz sources).
- From Terminal, `open -a Projet file.wav ...` imports sound files at the playhead, and a `.projet` file opens that project.

### Tracks and boxes

- **Ruler**: click or drag to move the playhead.
- **Boxes**: click to select; **Shift-click** adds to the selection, **Cmd-click** toggles one box; drag on an empty lane to select with a rectangle. Dragging a selected box moves the whole selection, also to other tracks. **Cmd-A** selects every box, **Esc** deselects. Boxes may overlap.
- **Edges**: drag to trim. The sound stays in place under the box (the left edge moves into or out of the sound).
- **Fades**: drag the small white squares at the top corners.
- Moving and trimming snap to box edges, the playhead and 0:00; hold **Shift** to move freely.
- **Right-click a box**: write notes, notes in a large window, split at playhead, duplicate, delete, change kind, show original sound in Finder. Cmd-D duplicates, Delete/Backspace deletes (both act on every selected box).
- **Split at playhead (Cmd-T)**: cuts the selected boxes at the playhead; with nothing selected, every box under the playhead. Short 5 ms fades are put on the cut so it does not click.
- **Copy, Cut, Paste (Cmd-C, Cmd-X, Cmd-V)**: paste puts the boxes at the playhead, starting on the selected track, keeping their spacing in time and between tracks; tracks are added when needed.
- **Track header**: **M** mutes, **S** solos (several tracks can be soloed). Double-click the name to rename. Right-click: rename, colour, add a track above or below, delete. **+ Track** at the bottom (or Shift-Cmd-N) adds one.
- **Wheel**: scroll up and down; Shift-wheel or a sideways swipe scrolls in time. **Cmd-wheel or pinch** zooms; Cmd-= / Cmd-- / Cmd-0 (the whole piece).
- **Undo / Redo** (Cmd-Z / Shift-Cmd-Z) for every change to tracks and boxes.

### Text boxes and notes

- **Double-click an empty lane** (or **Cmd-B**, at the playhead on the selected track) makes a **text box**: a post-it on the timeline. It is silent. Its notes are shown inside the box.
- **Every box has notes**, sound boxes too. **Double-click a box** to write them in the panel on the right.
- Notes are plain text with a few marks (Markdown), styled in the preview and in the box:
  - `**bold**`, `*italic*`, `# Heading` at the start of a line, `- item` for a list;
  - links: `[text](https://...)`, or just paste an address. Links are clickable in the preview.
  - The buttons **B**, **I**, **H**, **List** and **Link** add the marks for you (around the selected text, or on the selected lines).
- **Preview / Edit** switches between the styled notes and the text.
- **Expand** or **Cmd-E** opens the notes in a **large window**: text on the left, styled preview on the right, side by side. It follows the selected box.
- Typing is saved as one undo step per pause (*Edit notes*).
- **Kind** in the panel (or right-click > change kind): a text box can become a sound box (choose its sound in the panel) and back. The notes are always kept. Container, Board and Codebox come in later stages.

### Proportions (stage 3a)

Below the time ruler is the **proportion strip**: it divides a **frame** into fractions.

- **The frame** is the piece, a span, or a box. Click the strip's name (*Proportion: piece 12:00*) for the menu:
  - **The piece**: its set length, or its current length (*auto*) until you set one (*Set the piece length...*, or drag the frame's right edge on the strip, or type it in the project panel: `12:00`, or `auto`).
  - **A span**: drag on the strip; the span becomes the frame. Drag its edges to adjust; *Remove the span* in the menu.
  - **A box**: right-click a box > *Use as the ruler's frame* (the box gets a dashed outline).
- **Divisions**: every denominator up to 20 by default. Simpler fractions get taller, brighter ticks and are labelled first. With *Finer divisions appear as you zoom in* on, the finer denominators join only when their ticks are far enough apart, so zooming in reveals 1/7, 1/11, 1/17 and so on. Choose the denominators in *Divisions* (one by one, or a preset), and add your own, also decimal: **7.5** gives 7 full parts and a half part at the end.
- **Snapping**: moving and trimming boxes, and dragging spans, snap to the shown divisions and the frame's edges (as well as box edges and the playhead). **Shift** moves freely.
- **Readout**: while you move or trim a box, a label shows where the edge is: *start 3/11 of piece 1:05.455*, or *0.2731 ≈ 3/11* when it is only close.

**Giving a box proportions.** In the box panel, under *Proportions of a frame*, choose the frame (the piece, the span, or another box), or right-click > *Give it proportions of ...* (works on several boxes at once). Then type **Start** and **Length** as proportions of that frame:

| Type | Means |
| --- | --- |
| `3/11` | three elevenths |
| `2.5/11`, `3/7.5` | decimal numerator or denominator |
| `0.2727`, `27.3%` | decimal or percentage |
| `2/3 of 3/4` | a fraction of a fraction (= 1/2) |
| `phi` or `φ`, `1/phi`, `1-1/phi`, `golden` | the golden ratio (1.618); `golden` = 0.618 |
| `sqrt(2)/2`, `pi` | roots and constants |
| `3:8` | a ratio: the first part of 3 + 8 (= 3/11) |

- **Keep the proportions when the frame changes** (on by default): the box follows its frame. Move or stretch the frame (the box, the span, the piece length) and the box keeps 3/11. A box measured against a box measured against the piece follows the whole chain. Boxes with proportions have a small corner mark (filled while they follow).
- Moving or trimming such a box rewrites its proportions (snapped to a fraction when it lands on one).
- Off: the box stays where it is; the panel shows where it is now as proportions.
- The panel also shows the end, and, for a box measured against another box, where it sits in the piece.
- Copies keep their proportions at their new place; a box copied together with its frame box follows the copy.

### Ruler layers, marks, series and ratios (stage 3b)

**Layers.** The ruler is a stack of layers: one **time** layer and any number of **proportion** layers, each with its own frame, divisions and series. Click a layer's name for its menu. **Drag a layer's lower edge** (in the name column) to make it taller or thinner.
- The **ruler panel** (layer menu > *Ruler layers panel...*) lists the layers top to bottom: drag the dots to reorder, the eye shows or hides, **S M L** sizes, **x** deletes. Click a layer to see its details below: name, frame, snapping, divisions (1/2 to 1/20 and your own), series, and its marks. *Done* goes back.
- The proportion layer you last clicked is the **active** one (a small square before its name): its grid shows through the tracks, and *Use as the ruler's frame* and new box proportions refer to it.
- **Presets**: *Time and the piece (A)*, *Time only*, *Form work (B)* (golden section of the piece above the time, a span below). *Save...* keeps the present layers as your own preset, for all projects.

**Series** (layer menu > *Series*, or the panel): drawn in their own colour (green-blue), labelled, snapped to.
- **Golden section**: 1 level (0.382 and 0.618), 2 or 3 levels (each part cut again).
- **Fibonacci**: the parts of 5, 8, 13, 21, 34 or 55 (for 13: 1/13, 2/13, 3/13, 5/13, 8/13).
- **Harmonic**: 1/2, 1/3 ... up to 1/16.
- **Your own**: proportions separated by commas (1/7, 3/7, golden of 1/2).

**Marks** are named proportions of a frame. They follow their frame and are drawn on every layer with that frame, and as dashed lines through the tracks.
- **Double-click** a proportion layer for a new mark (it asks for a name, such as *a1*); **drag** a mark to move it (it snaps). Right-click a mark: rename, type its proportion, move it to the playhead, pin the selected boxes to it, delete.
- Layer menu: *New mark at the playhead*; *Split the frame by a ratio into marks...* (**2:3:5:8** gives marks at 1/9, 5/18 and 5/9; names start with what you choose).
- **Box proportions can name marks**: start `a1`, length `a2 - a1`, or `a1 + 1/12`. Move the mark and the box follows. A mark's own proportion can name other marks too.
- **Pinning**: drop a box edge on a mark (it snaps more strongly to marks) and a box with proportions is pinned there (the readout says *pinned to a1*); or right-click > *Pin the start/end to a mark*.
- Renaming a mark renames it in every proportion; deleting it leaves the boxes where they are.

**Ratio splits of boxes**: right-click a box > *Split by a ratio...*: cut it into parts (3:8, 1:1:1, 2:3:5:8 ...) or put marks in it.

**Ratio checking**: select two or more boxes; the panel shows them in time order with their **lengths as a ratio** (*3 : 8 : 5*, or *≈ 8 : 13* when only close), what fraction one is of the other, the gaps or overlaps between them, and where they sit in the active layer's frame.

### Folders (stage 3a)

- Right-click a track > **Put in a new folder**. Folders can hold folders.
- **Drag a track header** to move it: between tracks, or onto the middle of a folder to put it inside. A folder moves with everything in it. Right-click > **Move out of the folder**.
- The **triangle** folds a folder; its lane then shows a thin summary of all the boxes inside (it does that when open too).
- A folder gives its **colour** to everything inside (their own colours come back when they leave it). **M** and **S** on a folder mute or solo everything inside.
- Right-click a folder: fold, add a track inside, **ungroup** (the tracks stay, one level up), or delete the folder and its tracks.
- Boxes live on tracks, never on folder lines. Sounds dropped on a folder line go on a new track inside it.

### The panel on the right

- **Several boxes selected**: how many, and what you can do with them.
- **A text box selected**: name, kind, opacity, times, notes.
- **A sound box selected**: name, kind, sound, gain (−60 to +12 dB; double-click for 0), fade in and out with their shapes (linear, equal power, S-curve), **opacity** (visual only: overlapping boxes show through), start, end, length and offset into the sound, and the sound (copy, original, rates).
- **Nothing selected**: the project: sample rate, number of tracks, boxes and sounds, size of the copies, length, folder; the format for new imports and the Vorbis quality.

### Playback

- **Space** plays and stops; **Return** goes back to 0:00. The playhead follows during playback.
- Overlapping boxes simply add up; their fades make the crossfade. For a crossfade between unrelated sounds use **equal power** on both sides; for two copies of the same sound use **linear** or **S-curve**.
- **Audio > Audio settings (Cmd-,)**: output device and sample rate. Set the device to the project's rate: then every sample is played exactly as stored. At another rate Projet still plays correctly (through the sinc kernel), and the status line says so.
- The meters top right show the output peak.

**Worth testing in stage 3b**:
- Add, reorder, hide and size layers (in the panel and by dragging a layer's edge); try the presets; save your own.
- Give layers different frames (the piece, a span, a box) and series; zoom in and out.
- Make marks (double-click, ratio split), drag them, rename them; type `a1` and `a2 - a1` into box proportions; drop box edges on marks; delete a mark.
- Cut a box by a ratio; select several boxes and read the ratios.
- Undo marks and pins; save, quit and reopen.

**Worth testing in stage 3a**:
- Put tracks in folders, nest them, drag them in and out, fold them, mute and solo a folder, colour a folder.
- Set the piece length, drag a span, use a box as the frame; zoom in and out and watch the divisions.
- Give boxes proportions (3/11 of a box, the golden section of the piece, 3:8), then move and stretch the frame.
- A box measured against a box measured against the piece; unlock one and move the frame.
- Copy/paste and split boxes with proportions; undo all of it; save, quit and reopen.

**Worth testing in stage 2**:
- Make text boxes by double-clicking; write notes with headings, lists and links; click a link in the preview.
- The large notes window (Cmd-E) while selecting other boxes.
- Switch a text box to a sound box and back: the notes stay.
- Select several boxes (Shift, Cmd, rectangle) and move, duplicate, copy/paste and delete them.
- Split boxes with fades at the playhead and listen across the cut.
- Undo and redo all of it, then save, quit and reopen.

**Worth testing in stage 1**:
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
8. **Fast rate conversion** checked against the reference (SincReader at every position).

`Measurements/report-cloud-test-machine.md` is a run on the (slower) cloud machine where the code was written; your M4 should be roughly two to three times faster.
