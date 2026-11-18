// MainComponent.cpp

#include "MainComponent.h"
#include "Engine/Arrangement.h"
#include "UI/TimeFormat.h"
#include "Engine/ReaperExport.h"

namespace
{
    const char* audioPatterns = "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.caf;*.aac";
}

MainComponent::MainComponent()
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    juce::PropertiesFile::Options opts;
    opts.applicationName = "Projet";
    opts.filenameSuffix = "settings";
    opts.folderName = "Projet";
    opts.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters (opts);
    auto* settings = properties.getUserSettings();
    recentProjects.setMaxNumberOfItems (12);
    recentProjects.restoreFromString (settings->getValue ("recentProjects"));

    addAndMakeVisible (transport);
    addAndMakeVisible (timeline);
    addChildComponent (magnifier);
    addChildComponent (magnifierBar);
    addChildComponent (board);
    addChildComponent (boardBar);
    wireView (timeline);
    wireView (magnifier);
    addAndMakeVisible (inspector);
    addAndMakeVisible (panelEdge);
    addChildComponent (importPanel);
    project.setFramework (&framework);

    // The panel on the right: shown or hidden, and how wide (stage 4c).
    panelShown = settings->getBoolValue ("panelShown", true);
    panelWidth = settings->getIntValue ("panelWidth", 320);
    panelEdge.getWidth_ = [this] { return panelWidth; };
    panelEdge.onWidth = [this] (int w)
    {
        panelWidth = juce::jlimit (minPanelWidth, maxPanelWidth(), w);
        resized();
    };
    panelEdge.onDone = [this] { properties.getUserSettings()->setValue ("panelWidth", panelWidth); };
    inspector.onHidePanel = [this] { setPanelShown (false); };
    transport.onTogglePanel = [this] { setPanelShown (! panelShown); };

    // Levels (stage 5b): the outline on the left, the level bar above the timeline.
    addChildComponent (levelBar);
    addChildComponent (outline);
    outlineShown = settings->getBoolValue ("outlineShown", true);
    project.onFocusChanged = [this] { focusChanged(); };
    levelBar.onGoTo = [this] (juce::ValueTree c) { goTo (c); };
    levelBar.onSelectAround = [this] (juce::ValueTree box) { goTo (Project::containerOf (box)); timeline.selectBox (box); };
    outline.onSelect = [this] (juce::ValueTree box)
    {
        if (! box.isValid()) { goTo ({}); return; }
        showBox (box);
    };

    // The magnifier (stage 6): its bar, and the divider it makes.
    magnifierShare = juce::jlimit (0.2, 0.8, settings->getDoubleValue ("magnifierShare", 0.5));
    magnifierBar.onClose = [this] { closeMagnifier(); };
    magnifierBar.onGoInside = [this] { goTo (magnifier.getLevel()); };
    magnifierBar.onDragStart = [this] { shareAtDragStart = magnifierShare; };
    magnifierBar.onDrag = [this] (int dy)
    {
        const int h = juce::jmax (1, viewsArea.getHeight());
        magnifierShare = juce::jlimit (0.15, 0.85, shareAtDragStart - (double) dy / h);
        resized();
    };
    magnifierBar.onDragEnd = [this] { properties.getUserSettings()->setValue ("magnifierShare", magnifierShare); };

    // The board (stage 9): its bar is the divider too.
    boardShare = juce::jlimit (0.2, 0.85, settings->getDoubleValue ("boardShare", 0.55));
    boardFilled = settings->getBoolValue ("boardFilled", false);
    boardBar.setHeading ("BOARD");
    boardBar.setCloseTip ("Close the board");
    boardBar.onClose = [this] { closeBoard(); };
    boardBar.onGoInside = [this] { setBoardFilled (! boardFilled); };
    boardBar.onDragStart = [this] { boardShareAtDragStart = boardShare; };
    boardBar.onDrag = [this] (int dy)
    {
        if (boardFilled) return;
        const int h = juce::jmax (1, viewsArea.getHeight());
        boardShare = juce::jlimit (0.15, 0.9, boardShareAtDragStart - (double) dy / h);
        resized();
    };
    boardBar.onDragEnd = [this] { properties.getUserSettings()->setValue ("boardShare", boardShare); };
    board.onActivate = [this] { activateBoard(); };
    board.onSelectionChanged = [this]
    {
        if (boardActive) selectionChanged();
        commandManager.commandStatusChanged();
    };
    board.onStatus = [this] (const juce::String& m, bool warning) { transport.setStatus (m, warning); };
    // 9b: sound cards heard, links followed, sounds from files, cards onto tracks.
    board.onPlayCard = [this] (juce::ValueTree card) { playCard (card); };
    board.previewingCard = [this] { return previewer.getTag(); };
    board.previewSeconds = [this] { return previewer.getPositionSeconds(); };
    board.onGoToBox = [this] (juce::ValueTree box) { showBox (box); };
    board.onImportSounds = [this] (const juce::StringArray& files, juce::Point<float> p) { importSoundCards (files, p); };
    board.onChooseSoundFiles = [this] (juce::Point<float> p)
    {
        chooser = std::make_unique<juce::FileChooser> ("Sounds for the board", juce::File::getSpecialLocation (juce::File::userMusicDirectory), audioPatterns, true);
        juce::Component::SafePointer<MainComponent> safeThis (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [safeThis, p] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;
            juce::StringArray paths;
            for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
            if (! paths.isEmpty()) safeThis->importSoundCards (paths, p);
        });
    };
    board.canDragOut = [this] { return ! boardFilled; };
    timeline.onDropGesture = magnifier.onDropGesture = [this] (const juce::String& boxId, const juce::String& gestureId, juce::ValueTree track, double start, double length)
    {
        dropGesture (boxId, gestureId, track, start, length);
    };
    timeline.onDropSoundCard = magnifier.onDropSoundCard = [this] (const juce::String& boxId, const juce::String& cardId, juce::ValueTree track, double start)
    {
        dropSoundCard (boxId, cardId, track, start);
    };
    {
        auto& bp = inspector.getBoardPanel();
        bp.onShowOnBox = [this] { board.showOnBox(); };
        bp.onClearShown = [this] { board.clearShownPart(); };
        bp.onFit = [this] { board.zoomToFit(); };
        bp.onAddImages = [this] { board.chooseImages(); };
        bp.onFront = [this] { board.bringToFront(); };
        bp.onBack = [this] { board.sendToBack(); };
        bp.onDelete = [this] { board.deleteSelection(); };
        bp.onClose = [this] { closeBoard(); };
        bp.onFill = [this] { setBoardFilled (! boardFilled); };
        bp.isFilled = [this] { return boardFilled; };
        bp.onPlayCard = [this] (juce::ValueTree card) { playCard (card); };
        bp.previewingCard = [this] { return previewer.getTag(); };
        bp.onGoToBox = [this] (juce::ValueTree box) { showBox (box); };
        bp.onRelink = [this] (juce::ValueTree link) { board.relink (link); };
        bp.soundSeconds = [this] (const juce::ValueTree& card)
        {
            const auto id = card[IDs::soundId].toString();
            if (auto s = pool.get (id)) return s->seconds();
            return project.soundSeconds (id);
        };
    }
    inspector.onOpenBoard = [this] (juce::ValueTree box) { openBoard (box); };
    outline.onDiveIn = [this] (juce::ValueTree c) { goTo (c); };
    outline.onHide = [this] { setOutlineShown (false); };
    transport.onToggleOutline = [this] { setOutlineShown (! outlineShown); };
    addChildComponent (startPanel);

    startPanel.onNew = [this] { newProjectDialog(); };
    startPanel.onOpen = [this] { openProjectDialog(); };
    startPanel.onOpenRecent = [this] (const juce::File& f) { openProject (f); };

    transport.onPlayStop = [this] { playStop(); };
    transport.onToStart = [this] { locate (0.0); };
    transport.onToggleLoop = [this] { loopAction (0); };
    transport.onZoomIn = [this] { if (boardActive) board.zoomBy (1.25f); else view().zoomBy (1.5, playheadInLevel()); };
    transport.onZoomOut = [this] { if (boardActive) board.zoomBy (0.8f); else view().zoomBy (1.0 / 1.5, playheadInLevel()); };
    transport.onZoomFit = [this] { if (boardActive) board.zoomToFit(); else view().zoomToFit(); };
    transport.onImport = [this] { importDialog(); };
    transport.onToggleAudio = [this] { setAudioOn (! audioOn); };

    inspector.onExpandNotes = [this] { openNotesWindow(); };
    inspector.onAddSoundTakes = [this] (juce::ValueTree box) { addSoundTakes (box); };
    inspector.onToggleShowTakes = [this] { commandManager.invokeDirectly (cmdShowTakes, false); };
    inspector.onDrawFactor = [this] (const juce::String& key, const juce::String& id)
    {
        timeline.setFactorDrawing (key, id);
        magnifier.setFactorDrawing (key, id);
        if (id.isNotEmpty())
            if (auto* c = framework.findCategory (key))
                if (auto* f = framework.findFactor (*c, id))
                    transport.setStatus ("Drawing " + framework.text (f->name) + " over the selected box: click to add points, drag them, double-click to remove, "
                                         "Alt-drag to draw freely; the pen in the Framework tab stops it.");
        if (id.isEmpty()) transport.setStatus ({});
    };
    inspector.getRulerPanel().getPlayhead = [this] { return playheadInLevel(); };
    inspector.getRulerPanel().setProperties (properties.getUserSettings());
    inspector.getRulerPanel().onStatus = [this] (const juce::String& m, bool warning) { transport.setStatus (m, warning); };
    inspector.getRulerPanel().setFramework (&framework);
    inspector.getRulerPanel().onAddLane = [this] (int laneIndex) { return view().addLane (laneIndex); };
    inspector.extraInfo = [this]() -> std::pair<juce::String, juce::String>
    {
        auto node = view().getSelectedNode();
        return node.isValid() ? view().describeNode (node) : std::pair<juce::String, juce::String>();
    };
    inspector.getDisplayPanel().setProperties (properties.getUserSettings());
    inspector.getDisplayPanel().setView = [this] (const juce::Identifier& id, const juce::String& v) { setViewSetting (id, v); };
    inspector.getPackagesPanel().onAction = [this] (const juce::String& a, const juce::String& id) { packagesAction (a, id); };
    inspector.onShowFrameworks = [this] { showPackages(); };
    inspector.onReveal = [this] (const juce::File& f)
    {
        if (f.exists()) f.revealToUser();
        else transport.setStatus (f.getFullPathName() + " is no longer there.", true);
    };

    updater.callback = [this] { modelChanged(); };
    project.onChanged = [this]
    {
        if (! suppressDirty) dirty = true;
        updater.triggerAsyncUpdate();
    };
    pool.onLoaded = [this] (const juce::String&, const juce::String& error)
    {
        if (error.isNotEmpty()) transport.setStatus (error, true);
        rebuildArrangement();
        repaintViews();
        inspector.refresh();
    };

    // Audio: 2 outputs, no inputs. The device and rate are remembered.
    std::unique_ptr<juce::XmlElement> audioState (settings->getXmlValue ("audioState"));
    const auto err = deviceManager.initialise (0, 2, audioState.get(), true);
    if (err.isNotEmpty()) transport.setStatus ("Audio: " + err, true);
    deviceManager.addAudioCallback (&engine);
    deviceManager.addAudioCallback (&previewer);     // mixed with the engine's (9b)
    deviceManager.addChangeListener (this);
    // Off when it was off at the last quit.
    if (! settings->getBoolValue ("audioOn", true)) setAudioOn (false, false);

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setApplicationCommandManagerToWatch (&commandManager);
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #endif

    // The framework library, the first time: from the 4a framework file, else Projet's packages.
    {
        juce::String lang, naming;
        Framework::Report report;
        const auto made = Framework::ensureLibrary (lang, naming, report);
        if (lang.isNotEmpty() && settings->getValue ("frameworkLanguage").isEmpty()) settings->setValue ("frameworkLanguage", lang);
        if (naming == "first" || naming == "second") settings->setValue ("frameworkNaming", "composition.role=" + naming);
        if (made.failed()) transport.setStatus (made.getErrorMessage(), true);
        else if (! report.errors.isEmpty()) transport.setStatus (report.errors.joinIntoString (" "), true);
        else libraryNotice = report.notes.joinIntoString (" ");
    }

    openLastOrShowStart();

    setWantsKeyboardFocus (true);
    setSize (1400, 860);
    startTimerHz (audioOn ? 30 : 10);
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    stopTimer();
    deviceManager.removeAudioCallback (&previewer);
    deviceManager.removeAudioCallback (&engine);
    deviceManager.removeChangeListener (this);
    properties.saveIfNeeded();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
}

void MainComponent::setPanelShown (bool shown)
{
    if (shown == panelShown && inspector.isVisible() == shown) return;
    panelShown = shown;
    properties.getUserSettings()->setValue ("panelShown", shown);
    resized();
    commandManager.commandStatusChanged();          // the View menu says Show or Hide
}

void MainComponent::setOutlineShown (bool shown)
{
    outlineShown = shown;
    properties.getUserSettings()->setValue ("outlineShown", shown);
    resized();
    if (shown) outline.showFocus();
    commandManager.commandStatusChanged();
}

void MainComponent::goTo (juce::ValueTree container)
{
    inspector.commitNotes();
    closeMagnifier();
    if (container.isValid() && (! Project::isContainer (container) || ! project.isInPiece (container))) container = {};
    if (container != timeline.getLevel())
        timeline.setLevel (container);
    if (project.getFocus() == container) levelsChanged();   // the focus was already there (the magnifier's)
    else project.setFocus (container);
}

void MainComponent::focusChanged()
{
    // A pane clicked: the focus follows it quietly, the views stay as they are.
    if (quietFocus)
    {
        inspector.refresh();
        selectionChanged();
        commandManager.commandStatusChanged();
        return;
    }
    // Going to a level, or the level worked on has gone (undo, delete).
    if (magnifierShown && project.getFocus() != magnifier.getLevel()) closeMagnifier();
    if (! magnifierShown && timeline.getLevel() != project.getFocus()) timeline.setLevel (project.getFocus());
    levelsChanged();
}

void MainComponent::levelsChanged()
{
    outline.refresh();
    outline.setLevels (timeline.getLevel(), magnifierShown ? magnifier.getLevel() : juce::ValueTree());
    outline.showFocus();
    levelBar.setLevel (timeline.getLevel());
    resized();
    selectionChanged();
    commandManager.commandStatusChanged();
    const auto level = timeline.getLevel();
    if (level.isValid())
        transport.setStatus ("Inside " + level[IDs::name].toString() + ": double-click a container to go further in, Cmd-Up to go up, Shift-Cmd-Down to magnify one below.");
    else
        transport.setStatus ({});
}

//== The magnifier (stage 6) ===================================================

void MainComponent::wireView (TimelineView& v)
{
    v.onActivate = [this, &v] { activate (v); };
    v.onDiveIn = [this] (juce::ValueTree c) { goTo (c); };
    v.onOpenBoard = [this] (juce::ValueTree box) { openBoard (box); };
    v.onMagnify = [this, &v] (juce::ValueTree c) { magnify (v, c); };
    v.onLocate = [this] (double t) { locate (t); };
    v.onLoopAction = [this] (int a) { loopAction (a); };
    v.onSelectionChanged = [this, &v]
    {
        if (&v == active) selectionChanged();
        // The magnifier follows the containers selected above (decision 1).
        if (&v == &timeline && magnifierShown)
        {
            auto b = timeline.getSelectedBox();
            if (Project::isContainer (b) && b != magnifier.getLevel() && timeline.getSelectedBoxes().size() == 1)
                openMagnifier (b);
        }
    };
    v.onAddTextBox = [this] (juce::ValueTree track, double t) { addTextBox (track, t); };
    v.onEditNotes = [this] (juce::ValueTree box) { editNotes (box); };
    v.onOpenNotesWindow = [this, &v] (juce::ValueTree box) { v.selectBox (box); openNotesWindow(); };
    v.onSplitAtPlayhead = [this, &v] (juce::ValueTree box)
    {
        auto boxes = v.getSelectedBoxes();
        if (boxes.isEmpty()) boxes.add (box);
        splitAtPlayhead (boxes);
    };
    v.onStatus = [this] (const juce::String& m, bool warning) { transport.setStatus (m, warning); };
    v.onShowRulerPanel = [this] (const juce::String& layerId) { setPanelShown (true); inspector.showRuler (layerId); };
    v.onHighlightChanged = [this, &v]
    {
        const auto& h = v.getHighlight();
        (&v == &timeline ? magnifier : timeline).setHighlight (h);       // one highlight for both views
        transport.setStatus (h.isActive() ? "Highlighting " + h.describe (framework) + " (click the lane again, or View > Highlight > Show all, to clear)" : juce::String());
    };
    v.onFilesDropped = [this] (const juce::StringArray& files, juce::ValueTree track, double t) { importFiles (files, track, t); };
    v.onAddSoundTakes = [this] (juce::ValueTree box) { addSoundTakes (box); };
    v.onToggleShowTakes = [this] { commandManager.invokeDirectly (cmdShowTakes, false); };
    v.onFilesDroppedAsTakes = [this] (const juce::StringArray& files, juce::ValueTree box) { importFiles (files, {}, 0.0, box[IDs::id].toString()); };
    v.onRevealOriginal = [this] (juce::ValueTree box)
    {
        auto s = project.findSound (box[IDs::soundId].toString());
        juce::File f (s[IDs::originalPath].toString());
        if (f.exists()) f.revealToUser();
        else transport.setStatus ("The original of this sound is no longer at " + f.getFullPathName(), true);
    };
}

void MainComponent::activate (TimelineView& v)
{
    if (&v == &magnifier && ! magnifierShown) return;
    const bool fromBoard = boardActive;
    if (fromBoard)
    {
        boardActive = false;
        board.setActivePane (false);
    }
    const bool changed = &v != active || fromBoard;
    if (changed) inspector.commitNotes();
    active = &v;
    timeline.setActivePane ((magnifierShown || boardShown) && active == &timeline);
    magnifier.setActivePane (magnifierShown && active == &magnifier);
    if (project.getFocus() != v.getLevel())
    {
        const juce::ScopedValueSetter<bool> quiet (quietFocus, true);
        project.setFocus (v.getLevel());
    }
    else if (changed)
    {
        inspector.refresh();
        selectionChanged();
        commandManager.commandStatusChanged();
    }
}

void MainComponent::openMagnifier (juce::ValueTree c)
{
    if (! Project::isContainer (c) || ! project.isInPiece (c) || Project::containerOf (c) != timeline.getLevel()) return;
    inspector.commitNotes();
    closeBoard();                                  // one of the two below at a time (stage 9)
    project.prepareLevel (c);                      // its own rulers, on a first visit
    if (active == &magnifier) activate (timeline);
    magnifierShown = true;
    resized();                                     // laid out first, so it can be fitted
    magnifier.setHighlight (timeline.getHighlight());
    magnifier.setLevel (c);
    magnifier.zoomToFit();                          // fitted when it opens (decision 4)
    magnifier.setPlayhead (engine.getPositionSeconds());
    timeline.setMagnified (c);
    timeline.setActivePane (true);
    magnifier.setActivePane (false);
    outline.setLevels (timeline.getLevel(), c);
    updateMagnifierBar();
    commandManager.commandStatusChanged();
}

void MainComponent::closeMagnifier()
{
    if (! magnifierShown) return;
    if (active == &magnifier) activate (timeline);
    magnifierShown = false;
    magnifier.selectBox ({});
    magnifier.setLevel ({});
    timeline.setMagnified ({});
    timeline.setActivePane (false);
    outline.setLevels (timeline.getLevel(), {});
    resized();
    commandManager.commandStatusChanged();
}

void MainComponent::magnify (TimelineView& from, juce::ValueTree c)
{
    if (! Project::isContainer (c)) return;
    if (&from == &magnifier && magnifierShown && Project::containerOf (c) == magnifier.getLevel())
    {
        // A level down: the upper view goes inside the magnified container.
        const auto outer = magnifier.getLevel();
        goTo (outer);
        timeline.selectBox (c);
        openMagnifier (c);
        return;
    }
    if (magnifierShown && c == magnifier.getLevel()) { closeMagnifier(); return; }     // toggles
    openMagnifier (c);
}

void MainComponent::updateMagnifierBar()
{
    if (! magnifierShown) return;
    const auto c = magnifier.getLevel();
    if (! c.isValid()) return;
    juce::String name = c[IDs::name].toString();
    if (name.isEmpty()) name = "Container";
    if (Project::numTakes (c) > 1) name << juce::String::fromUTF8 (" \xc2\xb7 take ") << Project::activeTakeName (c);   // stage 7
    // Its place in the piece, and how much larger it is drawn than above.
    double from = 0.0, to = 0.0, upperPps = 1.0;
    {
        const Project::ScopedFocus upper (project, timeline.getLevel());
        from = project.levelToPiece (Project::boxStart (c));
        to = project.levelToPiece (Project::boxEnd (c));
    }
    upperPps = timeline.getPixelsPerSecond();
    const double k = Project::containerScale (c);
    const double times = magnifier.getPixelsPerSecond() / juce::jmax (1.0e-9, k * upperPps);
    const auto dot = juce::String::fromUTF8 ("   \xc2\xb7   ");
    int depth = 0;
    for (auto p = c; p.isValid() && depth < 64; p = Project::containerOf (p)) ++depth;
    juce::String about;
    about << "depth " << depth << dot
          << TimeFormat::format (from, 1) << juce::String::fromUTF8 (" \xe2\x80\x93 ") << TimeFormat::format (to, 1) << " in the piece" << dot
          << juce::String::fromUTF8 ("\xc3\x97") << juce::String (times, times < 10.0 ? 1 : 0) << " larger" << dot
          << (Project::isStretch (c) ? "stretch " + juce::String::fromUTF8 ("\xc3\x97") + juce::String (k, 2) : juce::String ("window"));
    magnifierBar.setInfo (name, about);
}

//== The board (stage 9) =======================================================

void MainComponent::openBoard (juce::ValueTree box)
{
    if (! Project::isBoard (box) || ! project.isInPiece (box)) return;
    inspector.commitNotes();
    closeMagnifier();
    const bool wasShown = boardShown;
    boardShown = true;
    resized();                                     // laid out first, so it can be fitted
    board.setBox (box);
    timeline.setOpenBoard (box);
    activateBoard();
    board.grabKeyboardFocus();
    updateBoardBar();
    if (! wasShown) transport.setStatus ("The board of " + box[IDs::name].toString() + ": double-click to write a card; the tools are on the left. "
                                         + "Click the timeline to work there again.");
    commandManager.commandStatusChanged();
}

void MainComponent::closeBoard()
{
    if (! boardShown) return;
    board.commitEdit();
    inspector.getBoardPanel().commitNotes();
    if (boardActive) activate (timeline);          // the panel and commands go back to the timeline
    boardShown = false;
    boardActive = false;
    board.setActivePane (false);
    board.setBox ({});
    timeline.setOpenBoard ({});
    timeline.setActivePane (magnifierShown && active == &timeline);
    resized();
    commandManager.commandStatusChanged();
}

void MainComponent::activateBoard()
{
    if (! boardShown) return;
    if (! boardActive) inspector.commitNotes();
    boardActive = true;
    timeline.setActivePane (false);
    magnifier.setActivePane (false);
    board.setActivePane (true);
    selectionChanged();
    commandManager.commandStatusChanged();
}

void MainComponent::setBoardFilled (bool filled)
{
    boardFilled = filled;
    properties.getUserSettings()->setValue ("boardFilled", filled);
    resized();
    if (filled) activateBoard();                    // the timeline is hidden: the board is worked on
    updateBoardBar();
    inspector.refresh();
}

void MainComponent::updateBoardBar()
{
    if (! boardShown) return;
    const auto box = board.getBox();
    juce::String name = box[IDs::name].toString();
    if (name.isEmpty()) name = "Board";
    if (Project::numTakes (box) > 1) name << juce::String::fromUTF8 (" \xc2\xb7 take ") << Project::activeTakeName (box);
    const auto b = Project::boardOf (box);
    const auto dot = juce::String::fromUTF8 ("   \xc2\xb7   ");
    juce::String about = Board::summary (b);
    about << dot << (Board::shownPart (b).isEmpty() ? "the box shows its name" : "the box shows a part of it");
    double from = 0.0;
    {
        const Project::ScopedFocus around (project, Project::containerOf (box));
        from = project.levelToPiece (Project::boxStart (box));
    }
    about << dot << "at " << TimeFormat::format (from, 1) << " in the piece";
    boardBar.setInfo (name, about);
    boardBar.setButton (boardFilled ? "Share" : "Fill", boardFilled ? "Give the timeline its room back above the board" : "Let the board fill the area (the timeline hides until Share)");
}

//== Sound cards and links (9b) ================================================

void MainComponent::playCard (juce::ValueTree card)
{
    if (! card.hasType (IDs::SOUNDCARD)) return;
    const auto id = card[IDs::id].toString();
    if (previewer.isPlaying() && previewer.getTag() == id) { previewer.stop(); board.repaint(); inspector.refresh(); return; }
    auto sound = pool.get (card[IDs::soundId].toString());
    if (sound == nullptr) { transport.setStatus ("The sound is still loading (or missing).", true); return; }
    if (! audioOn) setAudioOn (true);
    previewer.play (sound, Board::cardIn (card), Board::cardOut (card, sound->seconds()),
                    juce::Decibels::decibelsToGain ((float) (double) card.getProperty (IDs::gainDb, 0.0)),
                    (double) card.getProperty (IDs::fadeIn, 0.0), (double) card.getProperty (IDs::fadeOut, 0.0), id);
    previewWasPlaying = false;     // the timer brings the button and the line up to date
}

void MainComponent::showBox (juce::ValueTree box)
{
    if (! box.isValid() || ! project.isInPiece (box)) return;
    if (boardShown && boardFilled) setBoardFilled (false);      // the timeline in view again
    // A box of either level shown is selected where it is (stage 6).
    if (auto* v = viewFor (box)) { activate (*v); v->selectBox (box); v->revealBox (box); return; }
    goTo (Project::containerOf (box));
    activate (timeline);
    timeline.selectBox (box);
    timeline.revealBox (box);
}

void MainComponent::importSoundCards (const juce::StringArray& files, juce::Point<float> boardPoint)
{
    if (! boardShown || ! Project::isBoard (board.getBox())) return;
    importFiles (files, {}, 0.0, {});
    if (! batches.empty())
    {
        batches.back()->boardBoxId = board.getBox()[IDs::id].toString();
        batches.back()->boardPoint = boardPoint;
    }
}

void MainComponent::dropGesture (const juce::String& boardBoxId, const juce::String& gestureId, juce::ValueTree track, double start, double length)
{
    auto boardBox = project.findBoxInPiece (boardBoxId);
    auto gesture = Board::findItem (Project::boardOf (boardBox), gestureId);
    if (! gesture.isValid() || ! track.isValid() || Project::isFolder (track)) return;
    juce::String name = "Gesture";
    if (auto cats = BoardPainter::glyphCategories (framework); ! cats.isEmpty())
        if (auto* v = framework.findValue (*cats[0], gesture[cats[0]->property()].toString())) name = framework.text (v->name);
    project.getUndoManager().beginNewTransaction ("Text box from a gesture");
    auto box = project.addTextBox (track, juce::jmax (0.0, start), juce::jmax (Project::minBoxLength, length), name);
    // Drawing first, sound later: its energy, second energy and their curves come along.
    for (int i = 0; i < gesture.getNumProperties(); ++i)
    {
        const auto id = gesture.getPropertyName (i);
        if (id.toString().startsWith ("fw.")) box.setProperty (id, gesture[id], project.um());
    }
    if (auto* v = viewFor (box)) { activate (*v); v->selectBox (box); }
    transport.setStatus ("A text box from the gesture " + name + ", " + Board::formatSeconds (length) + " long, waiting for its sound.");
}

void MainComponent::dropSoundCard (const juce::String& boardBoxId, const juce::String& cardId, juce::ValueTree track, double start)
{
    auto boardBox = project.findBoxInPiece (boardBoxId);
    auto card = Board::findItem (Project::boardOf (boardBox), cardId);
    if (! card.isValid() || ! track.isValid() || Project::isFolder (track) || ! project.findSound (card[IDs::soundId].toString()).isValid()) return;
    const double seconds = project.soundSeconds (card[IDs::soundId].toString());
    project.getUndoManager().beginNewTransaction ("Sound box from a sound card");
    auto box = project.addSoundBox (track, juce::jmax (0.0, start), card[IDs::soundId].toString());
    const double from = Board::cardIn (card), to = Board::cardOut (card, seconds);
    box.setProperty (IDs::offset, from, project.um());
    box.setProperty (IDs::length, juce::jmax (Project::minBoxLength, to - from), project.um());
    for (auto* id : { &IDs::gainDb, &IDs::fadeIn, &IDs::fadeOut })
        if (card.hasProperty (*id)) box.setProperty (*id, card[*id], project.um());
    if (card[IDs::label].toString().isNotEmpty()) box.setProperty (IDs::name, card[IDs::label], project.um());
    if (auto* v = viewFor (box)) { activate (*v); v->selectBox (box); }
    transport.setStatus ("A sound box from the sound card " + BoardPainter::soundCardName (card, &project) + ".");
}

bool MainComponent::boardTakes (juce::CommandID id) const
{
    if (! boardActive || ! boardShown) return false;
    switch (id)
    {
        case cmdDelete: case cmdDuplicate: case cmdCopy: case cmdCut: case cmdPaste: case cmdSelectAll: case cmdDeselect:
        case cmdZoomIn: case cmdZoomOut: case cmdZoomFit:
            return true;
        default:
            return false;
    }
}

TimelineView* MainComponent::viewFor (const juce::ValueTree& node)
{
    const auto level = Project::containerOf (node);
    if (magnifierShown && level == magnifier.getLevel()) return &magnifier;
    if (level == timeline.getLevel()) return &timeline;
    return nullptr;
}

void MainComponent::selectInView (const juce::ValueTree& box)
{
    if (auto* v = viewFor (box)) v->selectBox (box);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    transport.setBounds (r.removeFromTop (48));
    transport.setPanelShown (panelShown);
    transport.setOutlineShown (outlineShown);
    inspector.setVisible (panelShown);
    panelEdge.setVisible (panelShown);
    if (panelShown)
    {
        panelWidth = juce::jlimit (minPanelWidth, maxPanelWidth(), panelWidth);
        inspector.setBounds (r.removeFromRight (panelWidth));
        panelEdge.setBounds (r.removeFromRight (5));
    }
    const bool showOutline = outlineShown && hasProject();
    outline.setVisible (showOutline);
    if (showOutline) outline.setBounds (r.removeFromLeft (OutlinePanel::preferredWidth));
    levelBar.setVisible (hasProject() && timeline.getLevel().isValid());
    if (levelBar.isVisible()) levelBar.setBounds (r.removeFromTop (LevelBar::preferredHeight));
    viewsArea = r;
    // The board below (stage 9), or filling the area.
    const bool showBoard = boardShown && hasProject();
    board.setVisible (showBoard);
    boardBar.setVisible (showBoard);
    timeline.setVisible (! (showBoard && boardFilled));
    if (showBoard)
    {
        if (boardFilled)
        {
            boardBar.setBounds (r.removeFromTop (MagnifierBar::preferredHeight));
            board.setBounds (r);
        }
        else
        {
            const int below = juce::jlimit (120, juce::jmax (120, r.getHeight() - 100), (int) std::round (r.getHeight() * boardShare));
            board.setBounds (r.removeFromBottom (below));
            boardBar.setBounds (r.removeFromBottom (MagnifierBar::preferredHeight));
        }
    }
    // The magnifier below, half the height at first (decision 5); its bar is the divider.
    const bool showMagnifier = magnifierShown && hasProject();
    magnifier.setVisible (showMagnifier);
    magnifierBar.setVisible (showMagnifier);
    if (showMagnifier)
    {
        const int below = juce::jlimit (80, juce::jmax (80, r.getHeight() - 120), (int) std::round (r.getHeight() * magnifierShare));
        magnifier.setBounds (r.removeFromBottom (below));
        magnifierBar.setBounds (r.removeFromBottom (MagnifierBar::preferredHeight));
    }
    timeline.setBounds (r);
    startPanel.setBounds (getLocalBounds());
    layoutImportPanel();
}

void MainComponent::layoutImportPanel()
{
    const auto t = viewsArea.isEmpty() ? timeline.getBounds() : viewsArea;
    const int h = juce::jmin (t.getHeight() - 20, importPanel.preferredHeight());
    importPanel.setBounds (t.getRight() - 420 - 16, t.getBottom() - h - 16, 420, h);
}

//== Project ===================================================================

void MainComponent::openLastOrShowStart()
{
    const juce::File last (properties.getUserSettings()->getValue ("lastProject"));
    if (last.existsAsFile() && openProject (last))
        return;
    showStart();
}

void MainComponent::showStart()
{
    juce::Array<juce::File> files;
    for (int i = 0; i < recentProjects.getNumFiles(); ++i)
        files.add (recentProjects.getFile (i));
    startPanel.setRecent (files);
    startPanel.setVisible (true);
    startPanel.toFront (false);
    updateTitle();
}

bool MainComponent::openProject (const juce::File& file)
{
    ProjectFolder candidate;
    Project probe;   // read into a scratch project first, so a bad file changes nothing
    const auto check = candidate.open (file, probe);
    if (check.failed())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not open the project",
                                                check.getErrorMessage());
        return false;
    }

    engine.stop();
    folder = candidate;
    juce::String error;
    project.fromJson (folder.getProjectFile().loadFileAsString(), error);
    afterProjectLoaded();
    return true;
}

void MainComponent::afterProjectLoaded()
{
    loadFramework (false);
    startPanel.setVisible (false);
    pool.clear();
    magnifierShown = false;
    boardShown = boardActive = false;               // stage 9
    board.setActivePane (false);
    board.setBox ({});
    timeline.setOpenBoard ({});
    timeline.setProjectFolder (folder.getFolder());
    magnifier.setProjectFolder (folder.getFolder());
    active = &timeline;
    magnifier.resetLevels();
    timeline.setMagnified ({});
    timeline.setActivePane (false);
    timeline.selectBox ({});
    timeline.resetLevels();
    levelBar.setLevel ({});
    outline.setLevels ({}, {});
    inspector.stopDrawingFactor();          // stage 8
    timeline.setFactorDrawing ({}, {});
    magnifier.setFactorDrawing ({}, {});
    auto& s = project.getState();
    timeline.setView (s.getProperty (IDs::viewStart, 0.0), s.getProperty (IDs::pixelsPerSecond, 20.0));
    outline.refresh();
    resized();
    locate (s.getProperty (IDs::playhead, 0.0));
    syncSounds();
    rebuildArrangement();
    inspector.setSelection ({});
    if (notesWindow != nullptr) notesWindow->setBox ({});
    markClean();
    rememberProject();
    transport.setStatus ({});
    if (libraryNotice.isNotEmpty()) { transport.setStatus (libraryNotice); libraryNotice = {}; }
    showGlyphProblems();

    // A device at another rate than the project still plays correctly (through
    // the sinc kernel), but the timer reminds once.
    lastStatus = {};
}

void MainComponent::rememberProject()
{
    if (! folder.isValid()) return;
    auto* settings = properties.getUserSettings();
    settings->setValue ("lastProject", folder.getProjectFile().getFullPathName());
    recentProjects.addFile (folder.getProjectFile());
    settings->setValue ("recentProjects", recentProjects.toString());
    properties.saveIfNeeded();
}

void MainComponent::updateTitle()
{
    const auto name = hasProject() ? project.getName() : juce::String ("no project");
    transport.setProjectName (hasProject() ? name : juce::String(), dirty && hasProject());
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setName (hasProject() ? "Projet - " + name + (dirty ? " *" : "") : juce::String ("Projet"));
}

void MainComponent::markClean()
{
    dirty = false;
    updateTitle();
}

void MainComponent::saveProject()
{
    board.commitEdit();
    inspector.commitNotes();
    if (notesWindow != nullptr) notesWindow->setBox (notesWindow->isVisible() ? view().getSelectedBox() : juce::ValueTree());
    {
        const juce::ScopedValueSetter<bool> svs (suppressDirty, true);
        auto& s = project.getState();
        s.setProperty (IDs::viewStart, timeline.getPieceViewStart(), nullptr);
        s.setProperty (IDs::pixelsPerSecond, timeline.getPiecePixelsPerSecond(), nullptr);
        s.setProperty (IDs::playhead, engine.getPositionSeconds(), nullptr);
    }

    const auto result = folder.save (project);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save", result.getErrorMessage());
        return;
    }
    markClean();
    rememberProject();
    transport.setStatus ("Saved " + folder.getProjectFile().getFileName());
}

void MainComponent::askToSaveThen (std::function<void()> next)
{
    if (! dirty || ! hasProject()) { next(); return; }

    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::AlertWindow::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save changes?",
                                           "Do you want to save the changes to " + project.getName() + "?",
                                           "Save", "Don't save", "Cancel", this,
                                           juce::ModalCallbackFunction::create ([safeThis, next] (int result)
    {
        if (safeThis == nullptr) return;
        if (result == 1)      { safeThis->saveProject(); if (! safeThis->dirty) next(); }
        else if (result == 2) next();
    }));
}

void MainComponent::newProjectDialog()
{
    juce::File parent (properties.getUserSettings()->getValue ("lastParent"));
    if (! parent.isDirectory()) parent = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Name the new project (a folder of that name is made)",
                                                   ProjectFolder::unusedFolder (parent, "Untitled"), "", true);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        auto target = fc.getResult();
        if (target == juce::File()) return;
        target = target.withFileExtension ("");   // a folder, not a file

        auto* w = new juce::AlertWindow ("New project", "Sample rate of " + target.getFileName()
                                         + ". Every imported sound is converted to it; it cannot be changed later.",
                                         juce::MessageBoxIconType::NoIcon, safeThis.getComponent());
        w->addComboBox ("rate", { "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" });
        w->getComboBoxComponent ("rate")->setSelectedItemIndex (1);
        w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, w, target] (int r)
        {
            if (safeThis == nullptr || r != 1) return;
            static const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
            const double rate = rates[juce::jlimit (0, 3, w->getComboBoxComponent ("rate")->getSelectedItemIndex())];

            auto& self = *safeThis;
            ProjectFolder created;
            Project fresh;
            const auto result = created.create (target, fresh, rate);
            if (result.failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not create the project", result.getErrorMessage());
                return;
            }
            self.properties.getUserSettings()->setValue ("lastParent", target.getParentDirectory().getFullPathName());
            self.openProject (created.getProjectFile());
        }), true);
    });
}

void MainComponent::openProjectDialog()
{
    juce::File parent (properties.getUserSettings()->getValue ("lastParent"));
    if (! parent.isDirectory()) parent = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Open a project", parent, juce::String ("*") + ProjectFolder::extension, true);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || fc.getResult() == juce::File()) return;
        safeThis->openProject (fc.getResult());
    });
}

//== Sounds ====================================================================

void MainComponent::syncSounds()
{
    std::map<juce::String, juce::File> wanted;
    for (auto s : project.getSounds())
        wanted[s[IDs::id].toString()] = folder.resolve (s[IDs::file].toString());
    pool.sync (wanted);
}

void MainComponent::rebuildArrangement()
{
    engine.setArrangement (projet::Arrangement::build (project, pool));
}

void MainComponent::modelChanged()
{
    project.checkFocus();                // the container worked on may have gone (undo, delete)
    // The magnified container, or the one shown above, may have gone too (stage 6).
    // ... or stopped being a container (another take, stage 7).
    auto gone = [this] (const juce::ValueTree& c) { return c.isValid() && (! project.isInPiece (c) || ! Project::isContainer (c)); };
    if (magnifierShown && (gone (magnifier.getLevel()) || Project::containerOf (magnifier.getLevel()) != timeline.getLevel()))
        closeMagnifier();
    if (gone (timeline.getLevel())) goTo (gone (project.getFocus()) ? juce::ValueTree() : project.getFocus());
    // The open board's box may have gone, or stopped being a board (stage 9).
    if (boardShown && (! project.isInPiece (board.getBox()) || ! Project::isBoard (board.getBox()))) closeBoard();
    if (boardShown) { board.modelChanged(); updateBoardBar(); }
    // A container's take with an inside never shown yet gets its rulers.
    if (magnifierShown) project.prepareLevel (magnifier.getLevel());
    if (timeline.getLevel().isValid()) project.prepareLevel (timeline.getLevel());
    outline.refresh();
    levelBar.repaint();
    syncSounds();
    rebuildArrangement();
    syncLoop();
    inspector.refresh();
    if (notesWindow != nullptr) notesWindow->refresh();
    repaintViews();
    updateMagnifierBar();
    updateTitle();
}

void MainComponent::openFiles (const juce::StringArray& paths)
{
    juce::StringArray sounds;
    for (auto& p : paths)
    {
        const juce::File f (p);
        if (f.hasFileExtension (ProjectFolder::extension))
        {
            juce::Component::SafePointer<MainComponent> safeThis (this);
            askToSaveThen ([safeThis, f] { if (safeThis != nullptr) safeThis->openProject (f); });
            return;
        }
        sounds.add (p);
    }
    if (hasProject() && ! sounds.isEmpty())
        importFiles (sounds, view().getSelectedLaneTrack(), playheadInLevel());
}

void MainComponent::importDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Import sounds", juce::File::getSpecialLocation (juce::File::userMusicDirectory), audioPatterns, true);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        juce::StringArray paths;
        for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
        if (paths.isEmpty()) return;
        safeThis->importFiles (paths, safeThis->view().getSelectedLaneTrack(), safeThis->playheadInLevel());
    });
}

void MainComponent::addSoundTakes (juce::ValueTree box)
{
    if (! box.isValid()) return;
    chooser = std::make_unique<juce::FileChooser> ("Sounds as new takes of " + box[IDs::name].toString(),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), audioPatterns, true);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    const auto boxId = box[IDs::id].toString();
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safeThis, boxId] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        juce::StringArray paths;
        for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
        if (! paths.isEmpty()) safeThis->importFiles (paths, {}, 0.0, boxId);
    });
}

void MainComponent::importFiles (const juce::StringArray& files, juce::ValueTree track, double start, const juce::String& takeBoxId)
{
    if (! folder.isValid() || files.isEmpty()) return;

    auto batch = std::make_shared<ImportBatch>();
    batch->trackId = track[IDs::id].toString();
    batch->takeBoxId = takeBoxId;
    batch->cursor = start;
    batch->results.resize ((size_t) files.size());
    batches.push_back (batch);

    projet::ImportSettings settings;
    settings.targetRate = project.getSampleRate();
    settings.format = project.getPlaceholderFormat();
    settings.vorbisQuality = project.getVorbisQuality();
    const juce::String ext = settings.format == "flac" ? ".flac" : ".ogg";

    for (int i = 0; i < files.size(); ++i)
    {
        const juce::File source (files[i]);
        auto dest = folder.newSoundFile (source.getFileNameWithoutExtension(), ext);
        dest.create();   // reserve the name while the copy is being made
        juce::Component::SafePointer<MainComponent> safeThis (this);
        auto progress = importer.start (source, dest, settings, [safeThis, batch, i] (const projet::ImportResult& r)
        {
            if (safeThis == nullptr) return;
            batch->results[(size_t) i] = r;
            safeThis->placeReady (batch);
        });
        importPanel.add (progress);
    }

    importPanel.update();
    importPanel.setVisible (true);
    layoutImportPanel();
    transport.setStatus ("Importing " + juce::String (files.size()) + (files.size() == 1 ? " sound..." : " sounds..."));
}

void MainComponent::placeReady (std::shared_ptr<ImportBatch> batch)
{
    // Boxes go in the order the files were chosen, one after the other.
    while (batch->next < (int) batch->results.size() && batch->results[(size_t) batch->next].has_value())
    {
        const auto& r = *batch->results[(size_t) batch->next];
        ++batch->next;

        if (! r.ok)
        {
            r.placeholder.deleteFile();
            if (r.cancelled) ++batch->cancelled;
            else batch->errors.add (r.original.getFileName() + ": " + r.error);
            continue;
        }

        const auto format = r.placeholder.getFileExtension().trimCharactersAtStart (".").toLowerCase();
        juce::String soundId;
        auto existing = project.findSoundByHash (r.hash, format);
        if (existing.isValid() && folder.resolve (existing[IDs::file].toString()).existsAsFile())
        {
            r.placeholder.deleteFile();   // already imported: reuse that copy
            soundId = existing[IDs::id].toString();
        }
        else
        {
            juce::ValueTree s (IDs::SOUND);
            soundId = Project::newId();
            s.setProperty (IDs::id, soundId, nullptr);
            s.setProperty (IDs::name, r.original.getFileNameWithoutExtension(), nullptr);
            s.setProperty (IDs::file, folder.relativePathOf (r.placeholder), nullptr);
            s.setProperty (IDs::originalPath, r.original.getFullPathName(), nullptr);
            s.setProperty (IDs::originalHash, r.hash, nullptr);
            s.setProperty (IDs::lengthSamples, (juce::int64) r.lengthSamples, nullptr);
            s.setProperty (IDs::originalRate, r.originalRate, nullptr);
            s.setProperty (IDs::originalChannels, r.originalChannels, nullptr);
            s.setProperty (IDs::format, format, nullptr);
            project.addSound (s);
        }

        // Sound cards on a board (9b), one under the other.
        if (batch->boardBoxId.isNotEmpty())
            if (auto boardBox = project.findBoxInPiece (batch->boardBoxId); boardBox.isValid() && Project::isBoard (boardBox))
            {
                project.getUndoManager().beginNewTransaction ("Sound card " + r.original.getFileName());
                auto card = Board::makeSoundCard (soundId, { batch->boardPoint.x, batch->boardPoint.y, 240.0f, 92.0f }, (int) Board::palette()[2].getARGB());
                Project::boardOf (boardBox).appendChild (card, project.um());
                batch->boardPoint.y += 110.0f;
                if (boardShown && board.getBox() == boardBox) board.selectItems ({ card });
                continue;
            }

        // New takes of a box (stage 7): the first of them is the one that plays.
        if (batch->takeBoxId.isNotEmpty())
            if (auto takeBox = project.findBoxInPiece (batch->takeBoxId); takeBox.isValid())
            {
                project.getUndoManager().beginNewTransaction ("New take " + r.original.getFileName());
                const auto playing = Project::activeTakeOf (takeBox);
                project.addTake (takeBox, Project::NewTake::sound, soundId);
                if (batch->firstTakeMade && playing.isValid()) project.setActiveTake (takeBox, playing);
                batch->firstTakeMade = true;
                selectInView (takeBox);
                continue;
            }

        auto track = project.findTrack (batch->trackId);
        if (track.isValid() && Project::isFolder (track)) track = {};
        if (! track.isValid()) track = project.laneTracks().getFirst();
        if (! track.isValid()) track = project.addTrack();

        project.getUndoManager().beginNewTransaction ("Import " + r.original.getFileName());
        auto box = project.addSoundBox (track, batch->cursor, soundId);
        batch->cursor = Project::boxEnd (box);
        selectInView (box);
    }

    if (batch->next >= (int) batch->results.size())
    {
        batches.erase (std::remove (batches.begin(), batches.end(), batch), batches.end());
        const int imported = (int) batch->results.size() - batch->errors.size() - batch->cancelled;
        if (batch->errors.isEmpty() && imported == 0)
            transport.setStatus ("Import cancelled.");
        else if (batch->errors.isEmpty())
            transport.setStatus ("Imported " + juce::String (imported) + (imported == 1 ? " sound" : " sounds")
                                 + (batch->cancelled > 0 ? ", " + juce::String (batch->cancelled) + " cancelled." : "."));
        else
        {
            transport.setStatus (juce::String (batch->errors.size()) + " sounds could not be imported.", true);
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Some sounds could not be imported",
                                                    batch->errors.joinIntoString ("\n"));
        }
    }
}

//== Editing ===================================================================

void MainComponent::selectionChanged()
{
    if (boardActive && boardShown)
    {
        inspector.showBoardItems (board.getBox(), board.getSelectedItems());
        return;
    }
    inspector.setSelection (view().getSelectedBoxes());
    juce::StringArray ids;
    for (auto b : view().getSelectedBoxes()) ids.add (b[IDs::id].toString());
    outline.setSelected (ids);
    if (notesWindow != nullptr && notesWindow->isVisible())
        notesWindow->setBox (view().getSelectedBox());
}

void MainComponent::addTextBox (juce::ValueTree track, double start)
{
    if (! track.isValid() || Project::isFolder (track)) track = view().getSelectedLaneTrack();
    if (! track.isValid()) return;
    project.getUndoManager().beginNewTransaction ("New text box");
    // About 160 pixels long at the current zoom, at least 2 s.
    auto box = project.addTextBox (track, start, juce::jmax (2.0, 160.0 / view().getPixelsPerSecond()));
    view().selectBox (box);
    inspector.focusNotes();
}

void MainComponent::addContainer (juce::ValueTree track, double start)
{
    if (! track.isValid() || Project::isFolder (track)) track = view().getSelectedLaneTrack();
    if (! track.isValid()) return;
    project.getUndoManager().beginNewTransaction ("New container");
    // About 240 pixels long at the current zoom, at least 4 s.
    auto box = project.addContainer (track, start, juce::jmax (4.0, 240.0 / view().getPixelsPerSecond()));
    view().selectBox (box);
}

void MainComponent::addBoard (juce::ValueTree track, double start)
{
    if (! track.isValid() || Project::isFolder (track)) track = view().getSelectedLaneTrack();
    if (! track.isValid()) return;
    project.getUndoManager().beginNewTransaction ("New board");
    // About 200 pixels long at the current zoom, at least 4 s.
    auto box = project.addBoardBox (track, start, juce::jmax (4.0, 200.0 / view().getPixelsPerSecond()));
    view().selectBox (box);
    openBoard (box);
}

void MainComponent::editNotes (juce::ValueTree box)
{
    view().selectBox (box);
    inspector.focusNotes();
}

void MainComponent::openNotesWindow()
{
    inspector.commitNotes();
    if (notesWindow == nullptr)
        notesWindow = std::make_unique<NotesWindow> (project);
    notesWindow->setBox (view().getSelectedBox());
    notesWindow->setVisible (true);
    notesWindow->toFront (true);
}

void MainComponent::splitAtPlayhead (juce::Array<juce::ValueTree> boxes)
{
    const double t = playheadInLevel();
    if (boxes.isEmpty())
        for (auto b : project.allBoxes())
            if (Project::boxStart (b) < t && t < Project::boxEnd (b)) boxes.add (b);

    project.getUndoManager().beginNewTransaction ("Split");
    juce::Array<juce::ValueTree> rights;
    for (auto b : boxes)
    {
        auto right = project.splitBox (b, t);
        if (right.isValid()) rights.add (right);
    }
    if (rights.isEmpty()) transport.setStatus ("Nothing to split: no selected box is under the playhead.", true);
    else view().setSelection (rights);
}

void MainComponent::copySelection()
{
    auto boxes = view().getSelectedBoxes();
    if (boxes.isEmpty()) return;

    // Track offsets count only tracks that hold boxes (folders are skipped).
    auto lanes = project.laneTracks();
    double firstTime = 1.0e12;
    int firstTrack = 1 << 30;
    for (auto b : boxes)
    {
        firstTime = juce::jmin (firstTime, Project::boxStart (b));
        firstTrack = juce::jmin (firstTrack, lanes.indexOf (b.getParent()));
    }

    clipboard.clear();
    for (auto b : boxes)
        clipboard.push_back ({ b.createCopy(), lanes.indexOf (b.getParent()) - firstTrack, Project::boxStart (b) - firstTime });
    transport.setStatus ("Copied " + juce::String (boxes.size()) + (boxes.size() == 1 ? " box." : " boxes."));
}

void MainComponent::deleteSelection (const juce::String& undoName)
{
    auto boxes = view().getSelectedBoxes();
    if (boxes.isEmpty()) { view().deleteDrawnSelection(); return; }      // a node, span or accent (4d.2)
    project.getUndoManager().beginNewTransaction (undoName);
    for (auto b : boxes) project.removeBox (b);
    view().selectBox ({});
}

void MainComponent::paste()
{
    if (clipboard.empty()) return;

    // At the playhead, starting on the selected track; more tracks are added if needed.
    const double t = playheadInLevel();
    auto lanes = project.laneTracks();
    const int base = juce::jmax (0, lanes.indexOf (view().getSelectedLaneTrack()));

    project.getUndoManager().beginNewTransaction ("Paste");

    // New ids; a box copied together with its frame box follows the copy of that frame.
    std::map<juce::String, juce::String> newIds;
    std::vector<juce::ValueTree> copies;
    for (auto& item : clipboard)
    {
        auto copy = item.box.createCopy();
        const auto id = Project::newId();
        newIds[copy[IDs::id].toString()] = id;
        copy.setProperty (IDs::id, id, nullptr);
        Project::renewInnerIds (copy, item.box[IDs::id].toString());     // a container's inside too (5a), its own nodes (5c)
        copy.setProperty (IDs::start, t + item.timeOffset, nullptr);
        copies.push_back (copy);
    }

    juce::Array<juce::ValueTree> pasted;
    for (size_t i = 0; i < copies.size(); ++i)
    {
        auto copy = copies[i];
        auto it = newIds.find (Project::boundFrame (copy));
        if (it != newIds.end()) copy.setProperty (IDs::propFrame, it->second, nullptr);
        else project.refitProportions (copy);

        while (lanes.size() <= base + clipboard[i].trackOffset) lanes.add (project.addTrack());
        lanes[base + clipboard[i].trackOffset].appendChild (copy, project.um());
        pasted.add (copy);
    }
    view().setSelection (pasted);
}

//== Transport =================================================================

void MainComponent::playStop()
{
    // On the board, Space plays the selected sound card (9b), when the piece is not playing.
    if (boardActive && boardShown && ! engine.isPlaying())
    {
        auto card = board.singleSelected();
        if (card.hasType (IDs::SOUNDCARD) || previewer.isPlaying())
        {
            if (card.hasType (IDs::SOUNDCARD)) playCard (card);
            else previewer.stop();
            return;
        }
    }
    if (engine.isPlaying()) engine.stop();
    else
    {
        if (! audioOn) setAudioOn (true);        // Play turns the engine on
        engine.play();
    }
    transport.setPlaying (engine.isPlaying());
}

void MainComponent::setAudioOn (bool on, bool announce)
{
    if (on == audioOn) return;
    if (on)
    {
        audioOn = true;
        deviceManager.restartLastAudioDevice();
        startTimerHz (30);
        if (deviceManager.getCurrentAudioDevice() == nullptr)
            transport.setStatus ("The audio device could not be opened again (Audio > Audio settings).", true);
        else if (announce)
            transport.setStatus ("Audio engine on.");
    }
    else
    {
        if (engine.isPlaying()) engine.stop();
        audioOn = false;
        deviceManager.closeAudioDevice();
        startTimerHz (10);                       // nothing moves: a slower clock is enough
        transport.setPeaks (0.0f, 0.0f);
        if (announce) transport.setStatus ("Audio engine off: the audio device is closed. Play or the Audio button turns it on again.");
    }
    transport.setAudioOn (audioOn);
    transport.setPlaying (engine.isPlaying());
    properties.getUserSettings()->setValue ("audioOn", audioOn);
    commandManager.commandStatusChanged();
}

//== The Reaper export (stage 10a) =============================================

void MainComponent::exportToReaper()
{
    if (! hasProject()) return;
    juce::Component::SafePointer<MainComponent> safeThis (this);
    const auto loop = project.getLoop();
    auto* w = new juce::AlertWindow ("Export to Reaper",
                                     "A Reaper project (.rpp) in the project's Reaper folder, with the sounds where they are. "
                                     "Containers become folder tracks; text and boards become empty items with their notes.",
                                     juce::MessageBoxIconType::NoIcon, this);
    juce::StringArray what { "The whole piece" };
    if (! loop.isEmpty()) what.add ("The loop: " + TimeFormat::format (loop.getStart(), 3) + " to " + TimeFormat::format (loop.getEnd(), 3) + ", moved to 0:00");
    w->addComboBox ("what", what, "What");
    w->addComboBox ("marks", { "Containers as regions, marks and nodes as markers", "Containers as regions only", "No regions or markers" }, "Regions and markers");
    w->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, w, loop] (int r)
    {
        if (safeThis == nullptr || r != 1) return;
        auto& self = *safeThis;
        projet::ReaperExport::Options options;
        if (w->getComboBoxComponent ("what")->getSelectedItemIndex() == 1) options.span = loop;
        const int marks = w->getComboBoxComponent ("marks")->getSelectedItemIndex();
        options.regions = marks <= 1;
        options.markers = marks == 0;

        // A new file each time: an export worked on in Reaper is never overwritten.
        auto dir = self.folder.getFolder().getChildFile ("Reaper");
        dir.createDirectory();
        const auto name = juce::File::createLegalFileName (self.folder.getProjectFile().getFileNameWithoutExtension()
                                                           + (options.span.isEmpty() ? juce::String() : juce::String (" loop"))
                                                           + juce::Time::getCurrentTime().formatted (" %Y-%m-%d %H%M"));
        auto file = dir.getChildFile (name + ".rpp");
        for (int n = 2; file.exists(); ++n) file = dir.getChildFile (name + " " + juce::String (n) + ".rpp");
        options.rppFile = file;
        options.trackColour = [] (int index) { return Theme::trackColour (index); };

        const auto result = projet::ReaperExport::write (self.project, self.framework, self.folder, options);
        if (! file.replaceWithText (result.text, false, false, "\n"))
        {
            self.transport.setStatus ("Could not write " + file.getFullPathName() + ".", true);
            return;
        }
        juce::String message;
        message << result.tracks << (result.tracks == 1 ? " track, " : " tracks, ") << result.items << (result.items == 1 ? " sound item, " : " sound items, ")
                << result.emptyItems << (result.emptyItems == 1 ? " empty item" : " empty items");
        if (result.regions > 0) message << ", " << result.regions << (result.regions == 1 ? " region" : " regions");
        if (result.markers > 0) message << ", " << result.markers << (result.markers == 1 ? " marker" : " markers");
        message << ".";
        if (! result.missing.isEmpty())
            message << "\n\n" << result.missing.size() << (result.missing.size() == 1 ? " sound file is" : " sound files are") << " missing: "
                    << result.missing.joinIntoString (", ") << ".";
        self.transport.setStatus ("Exported to Reaper: " + file.getFileName());
        auto* done = new juce::AlertWindow ("Exported to Reaper", file.getFileName() + "\n\n" + message, juce::MessageBoxIconType::NoIcon, safeThis.getComponent());
        done->addButton ("Open in Reaper", 1, juce::KeyPress (juce::KeyPress::returnKey));
        done->addButton ("Show in Finder", 2);
        done->addButton ("OK", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        done->enterModalState (true, juce::ModalCallbackFunction::create ([file] (int choice)
        {
            if (choice == 1) file.startAsProcess();
            else if (choice == 2) file.revealToUser();
        }), true);
    }), true);
}

//== The loop region (stage 10a) ===============================================

void MainComponent::syncLoop()
{
    const auto loop = project.getLoop();
    engine.setLoop (project.isLoopOn(), loop.getStart(), loop.getEnd());
    transport.setLoop (project.isLoopOn(), project.hasLoop());
    commandManager.commandStatusChanged();
}

void MainComponent::loopAction (int action)
{
    if (! hasProject()) return;
    if (action == 2)
    {
        project.getUndoManager().beginNewTransaction ("Remove the loop");
        project.setLoop ({});
        transport.setStatus ("The loop is removed.");
        return;
    }
    if (action == 1 || ! project.hasLoop())
    {
        auto& v = view();
        const Project::ScopedFocus levelScope (project, v.getLevel());
        const auto boxes = v.getSelectedBoxes();
        if (boxes.isEmpty())
        {
            transport.setStatus (action == 1 ? "Select the boxes to loop first." : "No loop yet: drag along the seconds ruler to draw one, or select boxes and press Cmd-L.");
            return;
        }
        double a = 1.0e12, b = 0.0;
        for (auto& box : boxes)
        {
            a = juce::jmin (a, project.levelToPiece (Project::boxStart (box)));
            b = juce::jmax (b, project.levelToPiece (Project::boxEnd (box)));
        }
        project.getUndoManager().beginNewTransaction ("Loop the selected boxes");
        project.setLoop ({ a, b });
        project.setLoopOn (true);
        transport.setStatus ("Looping " + TimeFormat::format (a, 3) + " to " + TimeFormat::format (b, 3) + ".");
        return;
    }
    project.setLoopOn (! project.isLoopOn());
    transport.setStatus (project.isLoopOn() ? "Loop on." : "Loop off.");
}

void MainComponent::locate (double seconds)
{
    engine.locate (seconds);
    timeline.setPlayhead (seconds);
    if (magnifierShown) magnifier.setPlayhead (seconds);
    transport.setTime (seconds);
}

void MainComponent::timerCallback()
{
    engine.purgeRetired();
    previewer.purge();
    // A sound card heard on the board: its play line, and its button (9b).
    if (previewer.isPlaying() || previewWasPlaying)
    {
        if (boardShown) board.repaint();
        if (previewer.isPlaying() != previewWasPlaying && inspector.isShowingBoard()) inspector.refresh();
        previewWasPlaying = previewer.isPlaying();
    }
    if (importPanel.isVisible())
    {
        const bool show = importPanel.update();
        importPanel.setVisible (show);
        if (show) layoutImportPanel();
    }
    const double t = engine.getPositionSeconds();
    timeline.setPlayhead (t);
    if (magnifierShown)
    {
        magnifier.setPlayhead (t);
        updateMagnifierBar();
    }
    transport.setTime (t);
    transport.setPlaying (engine.isPlaying());
    if (audioOn) transport.setPeaks (engine.getPeak (0), engine.getPeak (1));
    if (levelBar.isVisible())
    {
        levelBar.setPlayhead (t);
        levelBar.setView (timeline.getViewStart(), timeline.getViewEnd());
    }
    if (engine.isPlaying())
    {
        timeline.followPlayhead (t);
        // The magnifier follows while the playhead is inside its container.
        if (magnifierShown)
        {
            const auto c = magnifier.getLevel();
            const Project::ScopedFocus upper (project, timeline.getLevel());
            const double at = project.pieceToLevel (t);
            if (at >= Project::boxStart (c) && at < Project::boxEnd (c)) magnifier.followPlayhead (t);
        }
    }

    // A reminder when the device runs at another rate than the project.
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const double rate = device->getCurrentSampleRate();
        juce::String note;
        if (std::abs (rate - project.getSampleRate()) > 1.0)
            note = "The audio device runs at " + juce::String (rate / 1000.0, 1) + " kHz, the project at "
                 + juce::String (project.getSampleRate() / 1000.0, 1) + " kHz: playback is converted (Audio > Audio settings to match).";
        if (note != lastStatus && importer.getNumActive() == 0)
        {
            lastStatus = note;
            if (note.isNotEmpty()) transport.setStatus (note, true);
        }
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Closing the device to save CPU is not a new setup to remember.
    if (! audioOn) return;
    if (auto xml = deviceManager.createStateXml())
    {
        properties.getUserSettings()->setValue ("audioState", xml.get());
        properties.saveIfNeeded();
    }
    lastStatus = "-";   // re-check the rate
}

void MainComponent::showAudioSettings()
{
    if (! audioOn) setAudioOn (true);        // choosing a device means using it

    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 2, 2, false, false, true, false);
    selector->setSize (520, 420);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio settings";
    o.dialogBackgroundColour = Theme::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

//== Commands ==================================================================

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ cmdNewProject, cmdOpenProject, cmdSaveProject, cmdRevealFolder, cmdImport,
                  cmdUndo, cmdRedo, cmdDelete, cmdDuplicate, cmdDeselect,
                  cmdPlayStop, cmdToStart, cmdZoomIn, cmdZoomOut, cmdZoomFit,
                  cmdAddTrack, cmdAudioSettings, cmdAudioEngine, cmdShortcuts,
                  cmdNewTextBox, cmdSplit, cmdCopy, cmdCut, cmdPaste, cmdSelectAll, cmdNotesWindow, cmdTogglePanel,
                  cmdNewContainer, cmdGroup, cmdUngroup, cmdSwap, cmdDiveIn, cmdLevelUp, cmdToggleOutline, cmdMagnify,
                  cmdNextTake, cmdPreviousTake, cmdNewTake, cmdShowTakes, cmdNewBoard,
                  cmdLoopToggle, cmdLoopSelection, cmdExportReaper });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const bool hasBox = hasProject() && ! view().getSelectedBoxes().isEmpty();
    const bool open = hasProject();

    switch (id)
    {
        case cmdNewProject:    info.setInfo ("New project...", "Make a new project folder", "File", 0); info.addDefaultKeypress ('n', cmd); break;
        case cmdOpenProject:   info.setInfo ("Open project...", "Open a project", "File", 0); info.addDefaultKeypress ('o', cmd); break;
        case cmdSaveProject:   info.setInfo ("Save", "Save the project", "File", 0); info.addDefaultKeypress ('s', cmd); info.setActive (open); break;
        case cmdRevealFolder:  info.setInfo ("Show project folder in Finder", "Reveal the project folder", "File", 0); info.setActive (open); break;
        case cmdImport:        info.setInfo ("Import sounds...", "Import sound files as placeholder copies", "File", 0); info.addDefaultKeypress ('i', cmd); info.setActive (open); break;

        case cmdUndo:
            info.setInfo ("Undo " + project.getUndoManager().getUndoDescription(), "Undo", "Edit", 0);
            info.setActive (project.getUndoManager().canUndo());
            info.addDefaultKeypress ('z', cmd);
            break;
        case cmdRedo:
            info.setInfo ("Redo " + project.getUndoManager().getRedoDescription(), "Redo", "Edit", 0);
            info.setActive (project.getUndoManager().canRedo());
            info.addDefaultKeypress ('z', cmd | shift);
            break;
        case cmdDelete:
            info.setInfo ("Delete", "Delete the selected boxes", "Edit", 0);
            info.setActive (hasBox || view().hasDrawnSelection());
            info.addDefaultKeypress (KP::backspaceKey, 0);
            info.addDefaultKeypress (KP::deleteKey, 0);
            break;
        case cmdDuplicate:
            info.setInfo ("Duplicate", "Duplicate the selected boxes, each after itself", "Edit", 0);
            info.setActive (hasBox);
            info.addDefaultKeypress ('d', cmd);
            break;
        case cmdDeselect:      info.setInfo ("Select nothing", "Clear the selection", "Edit", 0); info.addDefaultKeypress (KP::escapeKey, 0); break;

        case cmdPlayStop:      info.setInfo ("Play / stop", "Start or stop playback", "Transport", 0); info.addDefaultKeypress (KP::spaceKey, 0); info.setActive (open); break;
        case cmdToStart:       info.setInfo ("Back to the start", "Move the playhead to 0:00", "Transport", 0); info.addDefaultKeypress (KP::returnKey, 0); info.addDefaultKeypress (KP::homeKey, 0); break;

        case cmdZoomIn:        info.setInfo ("Zoom in", "Zoom in", "View", 0); info.addDefaultKeypress ('=', cmd); break;
        case cmdZoomOut:       info.setInfo ("Zoom out", "Zoom out", "View", 0); info.addDefaultKeypress ('-', cmd); break;
        case cmdZoomFit:       info.setInfo ("Show the whole piece", "Zoom to fit", "View", 0); info.addDefaultKeypress ('0', cmd); break;
        case cmdTogglePanel:
            info.setInfo (panelShown ? "Hide panel" : "Show panel", "Hide or show the panel on the right", "View", 0);
            info.addDefaultKeypress ('i', cmd | juce::ModifierKeys::altModifier);
            break;
        case cmdToggleOutline:
            info.setInfo (outlineShown ? "Hide outline" : "Show outline", "Hide or show the outline on the left", "View", 0);
            info.addDefaultKeypress ('o', cmd | juce::ModifierKeys::altModifier);
            break;
        case cmdDiveIn:
            info.setInfo ("Go inside", "Show the selected container's own timeline", "View", 0);
            info.addDefaultKeypress (KP::downKey, cmd);
            info.setActive (hasProject() && Project::isContainer (view().getSelectedBox()));
            break;
        case cmdLevelUp:
            info.setInfo ("Up one level", "Show the level around this container", "View", 0);
            info.addDefaultKeypress (KP::upKey, cmd);
            info.setActive (hasProject() && timeline.getLevel().isValid());
            break;
        case cmdMagnify:
        {
            const auto sel = view().getSelectedBox();
            const bool canOpen = hasProject() && Project::isContainer (sel) && (active == &timeline || magnifierShown);
            info.setInfo (magnifierShown && (! canOpen || sel == magnifier.getLevel()) ? "Close the magnifier" : "Magnify below",
                          "Show the selected container in a second timeline below, or close it", "View", 0);
            info.addDefaultKeypress (KP::downKey, cmd | shift);
            info.setActive (canOpen || magnifierShown);
            break;
        }

        case cmdAddTrack:      info.setInfo ("Add track", "Add a track below the selected one", "Track", 0); info.addDefaultKeypress ('n', cmd | shift); info.setActive (open); break;
        case cmdAudioSettings: info.setInfo ("Audio settings...", "Output device and sample rate", "Audio", 0); info.addDefaultKeypress (',', cmd); break;
        case cmdAudioEngine:   info.setInfo ("Audio engine on", "Turn the audio engine on or off (off closes the device, saving CPU and battery)", "Audio", 0); info.setTicked (audioOn); break;
        case cmdShortcuts:     info.setInfo ("Keyboard shortcuts", "List the shortcuts", "Help", 0); info.addDefaultKeypress ('/', cmd); break;
        case cmdNewTextBox:    info.setInfo ("New text box", "A text box on the selected track at the playhead", "Edit", 0); info.addDefaultKeypress ('b', cmd); info.setActive (open); break;
        case cmdSplit:         info.setInfo ("Split at playhead", "Cut the selected boxes at the playhead (every box under it if none is selected)", "Edit", 0); info.addDefaultKeypress ('t', cmd); info.setActive (open); break;
        case cmdCopy:          info.setInfo ("Copy", "Copy the selected boxes", "Edit", 0); info.addDefaultKeypress ('c', cmd); info.setActive (hasBox); break;
        case cmdCut:           info.setInfo ("Cut", "Cut the selected boxes", "Edit", 0); info.addDefaultKeypress ('x', cmd); info.setActive (hasBox); break;
        case cmdPaste:         info.setInfo ("Paste at playhead", "Paste on the selected track at the playhead", "Edit", 0); info.addDefaultKeypress ('v', cmd); info.setActive (open && ! clipboard.empty()); break;
        case cmdSelectAll:     info.setInfo ("Select all boxes", "Select every box", "Edit", 0); info.addDefaultKeypress ('a', cmd); info.setActive (open); break;
        case cmdNotesWindow:   info.setInfo ("Notes in a large window", "Open the selected box's notes in a large window", "Edit", 0); info.addDefaultKeypress ('e', cmd); info.setActive (open); break;
        case cmdNewContainer:  info.setInfo ("New container", "An empty container on the selected track at the playhead", "Edit", 0); info.addDefaultKeypress ('b', cmd | shift); info.setActive (open); break;
        case cmdGroup:         info.setInfo ("Group into a container", "Put the selected boxes in a new container that spans them", "Edit", 0); info.addDefaultKeypress ('g', cmd); info.setActive (hasBox); break;
        case cmdUngroup:
        {
            bool any = false;
            if (hasProject()) for (auto b : view().getSelectedBoxes()) any = any || Project::isContainer (b);
            info.setInfo ("Ungroup", "Put the selected containers' boxes back on the tracks", "Edit", 0);
            info.addDefaultKeypress ('g', cmd | shift);
            info.setActive (any);
            break;
        }
        case cmdNextTake:
        case cmdPreviousTake:
        {
            bool any = false;
            if (hasProject()) for (auto b : view().getSelectedBoxes()) any = any || Project::numTakes (b) > 1;
            const bool next = id == cmdNextTake;
            info.setInfo (next ? "Next take" : "Previous take", next ? "The selected boxes play their next take" : "The selected boxes play their previous take", "Edit", 0);
            info.addDefaultKeypress (next ? ']' : '[', cmd);
            info.addDefaultKeypress (next ? KP::rightKey : KP::leftKey, cmd | juce::ModifierKeys::altModifier);
            info.setActive (any);
            break;
        }
        case cmdNewTake:
            info.setInfo ("New take (a copy)", "A new take of the selected box: a copy of the active one, to vary", "Edit", 0);
            info.setActive (hasProject() && view().getSelectedBoxes().size() == 1);
            break;
        case cmdShowTakes:
            info.setInfo ("Show takes in boxes", "Draw every take of a box stacked inside it (click one to make it active)", "View", 0);
            info.setTicked (hasProject() && (bool) project.getState().getProperty (IDs::showTakes, false));
            info.setActive (hasProject());
            break;
        case cmdSwap:          info.setInfo ("Swap the two boxes", "The two selected boxes change places, each keeping its length", "Edit", 0); info.setActive (hasProject() && view().getSelectedBoxes().size() == 2); break;
        case cmdNewBoard:      info.setInfo ("New board", "An empty board box on the selected track at the playhead, opened below", "Edit", 0); info.addDefaultKeypress ('b', cmd | juce::ModifierKeys::altModifier); info.setActive (open); break;
        case cmdLoopToggle:    info.setInfo ("Loop on", "Loop the loop region (draw it by dragging along the seconds ruler)", "Transport", 0); info.addDefaultKeypress ('l', 0);
                               info.setActive (open); info.setTicked (open && project.isLoopOn()); break;
        case cmdLoopSelection: info.setInfo ("Loop the selected boxes", "The loop region becomes the span of the selected boxes, and loops", "Transport", 0); info.addDefaultKeypress ('l', cmd); info.setActive (open); break;
        case cmdExportReaper:  info.setInfo ("Export to Reaper...", "Write the piece as a Reaper project (.rpp) in the project's Reaper folder", "File", 0); info.addDefaultKeypress ('r', cmd | shift); info.setActive (open); break;
        default: break;
    }
    // While the board is worked on, the editing commands are its own (stage 9).
    if (boardTakes (id))
    {
        const bool any = board.hasSelection();
        switch (id)
        {
            case cmdDelete:    info.shortName = "Delete"; info.setActive (any); break;
            case cmdDuplicate: info.setActive (any); break;
            case cmdCopy:      info.shortName = "Copy"; info.setActive (any); break;
            case cmdCut:       info.shortName = "Cut"; info.setActive (any); break;
            case cmdPaste:     info.shortName = "Paste"; info.setActive (BoardView::canPaste()); break;
            case cmdSelectAll: info.shortName = "Select all on the board"; info.setActive (true); break;
            case cmdZoomFit:   info.shortName = "Fit the board"; info.setActive (true); break;
            default: info.setActive (true); break;
        }
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    if (! hasProject() && info.commandID != cmdNewProject && info.commandID != cmdOpenProject
        && info.commandID != cmdAudioSettings && info.commandID != cmdAudioEngine && info.commandID != cmdShortcuts && info.commandID != cmdTogglePanel && info.commandID != cmdToggleOutline)
        return true;

    if (boardTakes (info.commandID))
    {
        switch (info.commandID)
        {
            case cmdDelete:    board.deleteSelection(); break;
            case cmdDuplicate: board.duplicateSelection(); break;
            case cmdCopy:      board.copySelection(); break;
            case cmdCut:       board.cutSelection(); break;
            case cmdPaste:     board.paste(); break;
            case cmdSelectAll: board.selectAll(); break;
            case cmdDeselect:  board.deselect(); break;
            case cmdZoomIn:    board.zoomBy (1.25f); break;
            case cmdZoomOut:   board.zoomBy (0.8f); break;
            case cmdZoomFit:   board.zoomToFit(); break;
            default: break;
        }
        return true;
    }

    switch (info.commandID)
    {
        case cmdNewProject:   askToSaveThen ([this] { newProjectDialog(); }); break;
        case cmdOpenProject:  askToSaveThen ([this] { openProjectDialog(); }); break;
        case cmdSaveProject:  saveProject(); break;
        case cmdRevealFolder: folder.getProjectFile().revealToUser(); break;
        case cmdImport:       importDialog(); break;

        case cmdUndo:         project.getUndoManager().undo(); break;
        case cmdRedo:         project.getUndoManager().redo(); break;
        case cmdDelete:       deleteSelection ("Delete"); break;
        case cmdDuplicate:
        {
            auto boxes = view().getSelectedBoxes();
            if (boxes.isEmpty()) break;
            project.getUndoManager().beginNewTransaction ("Duplicate");
            juce::Array<juce::ValueTree> copies;
            for (auto b : boxes) copies.add (project.duplicateBox (b));
            view().setSelection (copies);
            break;
        }
        case cmdNewTextBox:   addTextBox (view().getSelectedLaneTrack(), playheadInLevel()); break;
        case cmdSplit:        splitAtPlayhead (view().getSelectedBoxes()); break;
        case cmdCopy:         copySelection(); break;
        case cmdCut:          copySelection(); deleteSelection ("Cut"); break;
        case cmdPaste:        paste(); break;
        case cmdSelectAll:    view().selectAll(); break;
        case cmdNotesWindow:  openNotesWindow(); break;
        case cmdNewContainer: addContainer (view().getSelectedLaneTrack(), playheadInLevel()); break;
        case cmdNewBoard:     addBoard (view().getSelectedLaneTrack(), playheadInLevel()); break;
        case cmdLoopToggle:   loopAction (0); break;
        case cmdLoopSelection: loopAction (1); break;
        case cmdExportReaper: exportToReaper(); break;
        case cmdDiveIn:
        {
            auto box = view().getSelectedBox();
            if (Project::isContainer (box)) goTo (box);
            break;
        }
        case cmdLevelUp:      if (timeline.getLevel().isValid()) goTo (Project::containerOf (timeline.getLevel())); break;
        case cmdMagnify:
        {
            const auto sel = view().getSelectedBox();
            if (Project::isContainer (sel)) magnify (view(), sel);
            else closeMagnifier();
            break;
        }
        case cmdToggleOutline: setOutlineShown (! outlineShown); break;
        case cmdGroup:
        {
            auto boxes = view().getSelectedBoxes();
            if (boxes.isEmpty()) break;
            project.getUndoManager().beginNewTransaction ("Group into a container");
            auto c = project.groupIntoContainer (boxes);
            if (c.isValid()) view().selectBox (c);
            break;
        }
        case cmdUngroup:
        {
            juce::Array<juce::ValueTree> moved;
            project.getUndoManager().beginNewTransaction ("Ungroup");
            bool otherTakes = false;
            for (auto b : view().getSelectedBoxes())
                if (Project::isContainer (b))
                {
                    otherTakes = otherTakes || Project::numTakes (b) > 1;
                    moved.addArray (project.ungroupContainer (b));
                }
            view().setSelection (moved);
            if (otherTakes) transport.setStatus ("Ungrouped the active take's boxes; the container's other takes went with it (Cmd-Z brings them back).");
            break;
        }
        case cmdSwap:
        {
            auto boxes = view().getSelectedBoxes();
            if (boxes.size() != 2) break;
            project.getUndoManager().beginNewTransaction ("Swap");
            project.swapBoxes (boxes[0], boxes[1]);
            break;
        }
        case cmdDeselect:     view().selectBox ({}); break;
        case cmdNextTake:
        case cmdPreviousTake:
        {
            auto boxes = view().getSelectedBoxes();
            project.getUndoManager().beginNewTransaction (info.commandID == cmdNextTake ? "Next take" : "Previous take");
            for (auto b : boxes) project.stepTake (b, info.commandID == cmdNextTake ? 1 : -1);
            juce::StringArray names;
            for (auto b : boxes) if (Project::numTakes (b) > 1) names.add (b[IDs::name].toString() + ": take " + Project::activeTakeName (b));
            transport.setStatus (names.joinIntoString (", "));
            break;
        }
        case cmdNewTake:
        {
            auto boxes = view().getSelectedBoxes();
            if (boxes.size() != 1) break;
            project.getUndoManager().beginNewTransaction ("New take");
            project.addTake (boxes[0], Project::NewTake::copy);
            break;
        }
        case cmdShowTakes:
            setViewSetting (IDs::showTakes, (bool) project.getState().getProperty (IDs::showTakes, false) ? juce::String() : juce::String ("1"));
            commandManager.commandStatusChanged();
            break;

        case cmdPlayStop:     playStop(); break;
        case cmdToStart:      locate (0.0); break;
        case cmdZoomIn:       view().zoomBy (1.5, playheadInLevel()); break;
        case cmdZoomOut:      view().zoomBy (1.0 / 1.5, playheadInLevel()); break;
        case cmdZoomFit:      view().zoomToFit(); break;
        case cmdTogglePanel:  setPanelShown (! panelShown); break;

        case cmdAddTrack:
        {
            // After the selected track (and everything inside it), in the same folder.
            project.getUndoManager().beginNewTransaction ("Add track");
            auto sel = view().getSelectedTrack();
            if (sel.isValid()) project.addTrack ({}, project.blockEnd (sel), sel[IDs::folderId].toString());
            else project.addTrack();
            break;
        }
        case cmdAudioSettings: showAudioSettings(); break;
        case cmdAudioEngine:   setAudioOn (! audioOn); break;
        case cmdShortcuts:     showShortcuts(); break;
        default: return false;
    }
    return true;
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View", "Transport", "Track", "Framework", "Audio", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (std::initializer_list<int> ids)
    {
        for (int id : ids)
            if (id == 0) m.addSeparator(); else m.addCommandItem (&commandManager, id);
    };

    switch (menuIndex)
    {
        case 0:
        {
            add ({ cmdNewProject, cmdOpenProject });
            juce::PopupMenu recent;
            recentProjects.createPopupMenuItems (recent, 5000, true, true);
            m.addSubMenu ("Open recent", recent, recent.getNumItems() > 0);
            add ({ 0, cmdSaveProject, cmdRevealFolder, 0, cmdImport, 0, cmdExportReaper });
            break;
        }
        case 1: add ({ cmdUndo, cmdRedo, 0, cmdCut, cmdCopy, cmdPaste, cmdDuplicate, cmdDelete, 0, cmdSplit, cmdNewTextBox, cmdNotesWindow,
                       0, cmdNewContainer, cmdGroup, cmdUngroup, cmdSwap, 0, cmdNewBoard, 0, cmdNewTake, cmdNextTake, cmdPreviousTake, 0, cmdSelectAll, cmdDeselect }); break;
        case 2:
        {
            add ({ cmdZoomIn, cmdZoomOut, cmdZoomFit, 0, cmdDiveIn, cmdLevelUp, cmdMagnify, 0, cmdShowTakes, 0, cmdTogglePanel, cmdToggleOutline });
            m.addSeparator();
            // Colour modes (stage 4a, 4b): 6000 = track colours, 6001 + category index
            const auto mode = project.getState().getProperty (IDs::colourMode, "own").toString();
            juce::PopupMenu colours;
            colours.addItem (6000, "Track colours", hasProject(), mode == "own");
            for (auto& c : framework.getCategories())
                if ((c.kind == Framework::Kind::choice || c.kind == Framework::Kind::tags) && framework.hasColours (c) && c.index < 98 && c.forBoxes())
                {
                    bool ownColours = false;
                    for (auto& v : framework.valuesOf (c)) ownColours = ownColours || v.colours.colour != 0;
                    const auto& groupsName = framework.source (c).groupsName;
                    colours.addItem (6001 + c.index, framework.text (c.name) + (! ownColours && ! groupsName.isEmpty() ? " (" + framework.text (groupsName) + ")" : juce::String()),
                                     hasProject(), mode == c.key());
                }
            m.addSubMenu ("Colour boxes by", colours, hasProject());

            // Highlight: 6100 clears; values at highlightBase + index * 500 + 1 + i, groups at + 251 + i
            const auto& h = view().getHighlight();
            juce::PopupMenu hl;
            hl.addItem (6100, "Show all (clear)", h.isActive());
            for (auto& c : framework.getCategories())
            {
                if (c.valuesFrom.isNotEmpty() || ! c.forBoxes() || (c.kind != Framework::Kind::choice && c.kind != Framework::Kind::tags)) continue;
                const auto& values = framework.valuesOf (c);
                const auto& groups = framework.groupsOf (c);
                if (values.isEmpty() || values.size() > 249 || groups.size() > 248) continue;
                const int base = highlightBase + c.index * 500;
                const auto ticked = h.idsFor (c.key()), tickedGroups = h.idsFor (c.key() + "#groups");
                if (! groups.isEmpty())
                {
                    juce::PopupMenu gm;
                    for (int i = 0; i < groups.size(); ++i)
                        gm.addItem (base + 251 + i, framework.text (groups[i].name), true, tickedGroups.contains (groups[i].id));
                    hl.addSubMenu (framework.text (c.name) + ": " + framework.text (c.groupsName), gm, true, juce::Image(), ! tickedGroups.isEmpty());
                }
                auto valueItem = [&] (juce::PopupMenu& menu, int i)
                {
                    auto text = framework.valueName (c, values[i].id);
                    if (values[i].label.isNotEmpty() && ! text.startsWith (values[i].label)) text = values[i].label + "   " + text;
                    menu.addItem (base + 1 + i, text, true, ticked.contains (values[i].id));
                };
                if (groups.isEmpty() && values.size() <= 6)
                {
                    hl.addSectionHeader (framework.text (c.name));
                    for (int i = 0; i < values.size(); ++i) valueItem (hl, i);
                }
                else
                {
                    juce::PopupMenu vm;
                    juce::Array<int> listed;
                    for (auto& g : groups)
                    {
                        bool heading = false;
                        for (int i = 0; i < values.size(); ++i)
                            if (values[i].groups.size() > 0 && values[i].groups[0] == g.id)
                            {
                                if (! heading) { vm.addSectionHeader (framework.text (g.name)); heading = true; }
                                valueItem (vm, i);
                                listed.add (i);
                            }
                    }
                    for (int i = 0; i < values.size(); ++i) if (! listed.contains (i)) valueItem (vm, i);
                    hl.addSubMenu (framework.text (c.name), vm, true, juce::Image(), ! ticked.isEmpty());
                }
            }
            m.addSubMenu ("Highlight", hl, hasProject(), juce::Image(), h.isActive());
            m.addItem (6099, "Display...", hasProject());
            break;
        }
        case 3: add ({ cmdPlayStop, cmdToStart, 0, cmdLoopToggle, cmdLoopSelection }); break;
        case 4:
        {
            add ({ cmdAddTrack });
            // One folder per value (stage 4d: the semiotic track template): ids 6300+
            juce::PopupMenu from;
            for (auto& c : framework.getCategories())
                if (c.folders && ! c.foldersOnly && c.kind == Framework::Kind::choice && c.index < 90 && ! framework.valuesOf (c).isEmpty())
                    from.addItem (6300 + c.index, framework.text (c.name), hasProject());
            m.addSubMenu ("Folders from", from, hasProject() && from.getNumItems() > 0);
            break;
        }
        case 5:
        {
            // Framework packages (stage 4b): ids 6200+
            m.addItem (6200, "Frameworks...", hasProject());
            m.addItem (6201, "Reload frameworks", hasProject());
            m.addSeparator();
            juce::PopupMenu lang;
            lang.addItem (6210, juce::String::fromUTF8 ("Fran\xc3\xa7" "ais"), hasProject(), framework.getLanguage() == "fr");
            lang.addItem (6211, "English", hasProject(), framework.getLanguage() == "en");
            m.addSubMenu ("Names in", lang, hasProject());
            namingKeys.clear();
            for (auto& c : framework.getCategories())
                if (c.valuesFrom.isEmpty() && c.hasTwoNames && namingKeys.size() < 20)
                {
                    const int base = 6220 + namingKeys.size() * 3;
                    namingKeys.add (c.key());
                    const auto naming = framework.getNaming (c.key());
                    const auto first = c.firstNames.isEmpty() ? juce::String ("First names") : framework.text (c.firstNames);
                    const auto second = c.secondNames.isEmpty() ? juce::String ("Second names") : framework.text (c.secondNames);
                    juce::PopupMenu n;
                    n.addItem (base, first + " and " + second, hasProject(), naming == "both");
                    n.addItem (base + 1, first, hasProject(), naming == "first");
                    n.addItem (base + 2, second, hasProject(), naming == "second");
                    m.addSubMenu (framework.text (c.name) + ": names", n, hasProject());
                }
            m.addSeparator();
            m.addItem (6202, "Show the project's frameworks folder", hasProject());
            m.addItem (6203, "Show the library folder");
            break;
        }
        case 6: add ({ cmdAudioEngine, cmdAudioSettings }); break;
        case 7: add ({ cmdShortcuts }); break;
        default: break;
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    if (id >= 6000 && id < 6099)
    {
        const int index = id - 6001;
        setViewSetting (IDs::colourMode, juce::isPositiveAndBelow (index, framework.getCategories().size())
                                         ? framework.getCategories()[index].key() : juce::String ("own"));
        return;
    }
    if (id == 6099) { setPanelShown (true); inspector.showDisplay(); return; }
    if (id == 6100 || (id >= highlightBase && id < highlightBase + 500 * framework.getCategories().size()))
    {
        if (id == 6100) view().clearHighlight();
        else
        {
            const auto& c = framework.getCategories().getReference ((id - highlightBase) / 500);
            const int item = (id - highlightBase) % 500;
            if (item >= 251 && juce::isPositiveAndBelow (item - 251, framework.groupsOf (c).size()))
                view().toggleHighlight (c.key() + "#groups", framework.groupsOf (c)[item - 251].id);
            else if (item >= 1 && juce::isPositiveAndBelow (item - 1, framework.valuesOf (c).size()))
                view().toggleHighlight (c.key(), framework.valuesOf (c)[item - 1].id);
        }
        const auto& h = view().getHighlight();
        (active == &timeline ? magnifier : timeline).setHighlight (h);
        transport.setStatus (h.isActive() ? "Highlighting " + h.describe (framework) + " (View > Highlight > Show all to clear)" : juce::String());
        return;
    }
    if (id >= 6300 && id < 6390 && juce::isPositiveAndBelow (id - 6300, framework.getCategories().size()))
    {
        const auto& c = framework.getCategories().getReference (id - 6300);
        project.getUndoManager().beginNewTransaction ("Folders from " + framework.text (c.name));
        const auto made = project.addFoldersFor (c);
        transport.setStatus (juce::String (made.size()) + " folders added, one per " + framework.text (c.name).toLowerCase()
                             + ": boxes on their tracks take the folder's value unless they have their own.");
        return;
    }
    if (id == 6200) { showPackages(); return; }
    if (id == 6201) { loadFramework (true); refreshPackages(); return; }
    if (id == 6202) { Framework::projectFolder (folder.getFolder()).revealToUser(); return; }
    if (id == 6203) { Framework::libraryFolder().revealToUser(); return; }
    if (id == 6210 || id == 6211) { setFrameworkLanguage (id == 6211 ? "en" : "fr"); return; }
    if (id >= 6220 && id < 6280)
    {
        const auto key = namingKeys[(id - 6220) / 3];
        if (key.isEmpty()) return;
        static const char* namings[] = { "both", "first", "second" };
        framework.setNaming (key, namings[(id - 6220) % 3]);
        setViewSetting (IDs::fwNaming, framework.namingsToString());
        properties.getUserSettings()->setValue ("frameworkNaming", framework.namingsToString());
        inspector.frameworkChanged();
        return;
    }
    if (id >= 5000 && id < 5100)
    {
        const auto file = recentProjects.getFile (id - 5000);
        juce::Component::SafePointer<MainComponent> safeThis (this);
        askToSaveThen ([safeThis, file] { if (safeThis != nullptr) safeThis->openProject (file); });
    }
}

//== Framework packages (stage 4b) =============================================

void MainComponent::setViewSetting (const juce::Identifier& id, const juce::String& value)
{
    {
        const juce::ScopedValueSetter<bool> svs (suppressDirty, true);   // a view setting
        if (value.isEmpty()) project.getState().removeProperty (id, nullptr);
        else project.getState().setProperty (id, value, nullptr);
    }
    repaintViews();
    inspector.frameworkChanged();
}

void MainComponent::setFrameworkLanguage (const juce::String& lang)
{
    framework.setLanguage (lang);
    setViewSetting (IDs::fwLanguage, framework.getLanguage());
    properties.getUserSettings()->setValue ("frameworkLanguage", framework.getLanguage());   // for new projects
    if (inspector.isShowingPackages()) refreshPackages();
}

void MainComponent::loadFramework (bool announce)
{
    auto* settings = properties.getUserSettings();
    framework.clear();
    if (! folder.isValid()) return;

    // The first time: from the project's 4a framework file, else the library.
    juce::String lang, naming;
    Framework::Report report;
    const auto made = Framework::ensureProject (folder.getFolder(), lang, naming, report);
    if (made.failed()) report.errors.add (made.getErrorMessage());
    {
        const juce::ScopedValueSetter<bool> svs (suppressDirty, true);
        auto& state = project.getState();
        if (lang.isNotEmpty()) state.setProperty (IDs::fwLanguage, lang, nullptr);
        if (naming == "first" || naming == "second") state.setProperty (IDs::fwNaming, "composition.role=" + naming, nullptr);
        if (! state.hasProperty (IDs::fwLanguage))
            state.setProperty (IDs::fwLanguage, settings->getValue ("frameworkLanguage", "fr"), nullptr);
        if (! state.hasProperty (IDs::fwNaming) && settings->getValue ("frameworkNaming").isNotEmpty())
            state.setProperty (IDs::fwNaming, settings->getValue ("frameworkNaming"), nullptr);
    }
    framework.setLanguage (project.getState()[IDs::fwLanguage].toString());
    framework.namingsFromString (project.getState()[IDs::fwNaming].toString());

    framework.loadFolder (Framework::projectFolder (folder.getFolder()), report);
    if (! report.errors.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "A framework package could not be read",
                                                report.errors.joinIntoString ("\n\n") + "\n\nThe other packages are in use. Fix the file, then "
                                                "Framework > Reload frameworks.\n\n" + Framework::projectFolder (folder.getFolder()).getFullPathName());
    else if (! report.notes.isEmpty() && (announce || made.wasOk()))
        transport.setStatus (report.notes.joinIntoString (" "), false);
    if (announce && report.errors.isEmpty())
        transport.setStatus ("Frameworks: " + juce::String (framework.getPackages().size()) + " packages, "
                             + juce::String (framework.getCategories().size()) + " categories"
                             + (report.notes.isEmpty() ? juce::String() : " (" + juce::String (report.notes.size()) + " notes: Framework > Frameworks...)"));

    timeline.frameworkReloaded();
    magnifier.frameworkReloaded();
    board.frameworkReloaded();
    showGlyphProblems();
    inspector.frameworkChanged();
    inspector.refresh();
}

void MainComponent::showGlyphProblems()
{
    if (! timeline.glyphProblems().isEmpty())
        transport.setStatus ("A glyph script could not be read: " + timeline.glyphProblems().joinIntoString ("; ")
                             + ". Its package draws with Projet's shapes meanwhile.", true);
}

void MainComponent::showPackages()
{
    setPanelShown (true);
    if (! hasProject()) return;
    inspector.showPackages();
    refreshPackages();
}

void MainComponent::refreshPackages()
{
    if (! inspector.isShowingPackages() || ! folder.isValid()) return;
    const auto projectDir = Framework::projectFolder (folder.getFolder());
    const auto libraryDir = Framework::libraryFolder();

    juce::StringArray ids;
    for (auto& f : Framework::packageFolders (projectDir)) ids.addIfNotAlreadyThere (f.getFileName());
    for (auto& f : Framework::packageFolders (libraryDir)) ids.addIfNotAlreadyThere (f.getFileName());
    for (auto& b : Framework::builtins()) ids.addIfNotAlreadyThere (b.id);

    juce::Array<PackagesPanel::Entry> entries;
    for (auto& id : ids)
    {
        PackagesPanel::Entry e;
        e.id = id;
        const auto pf = projectDir.getChildFile (id), lf = libraryDir.getChildFile (id);
        e.inProject = pf.getChildFile ("framework.json").existsAsFile();
        e.inLibrary = lf.getChildFile ("framework.json").existsAsFile();
        e.projectVersion = Framework::folderVersion (pf);
        e.libraryVersion = Framework::folderVersion (lf);
        juce::String builtinJson, builtinHelp;
        bool builtinGlyphs = false;
        for (auto& b : Framework::builtins())
            if (b.id == id) { e.builtin = true; e.builtinVersion = b.version; builtinJson = b.json; builtinHelp = b.help; builtinGlyphs = b.glyphs.isNotEmpty(); }
        e.hasScript = e.inProject ? pf.getChildFile ("glyphs.js").existsAsFile()
                    : e.inLibrary ? lf.getChildFile ("glyphs.js").existsAsFile() : builtinGlyphs;

        // Its name, description and categories: as this project reads it, else as the library's or Projet's file says.
        Framework reader;
        reader.setLanguage (framework.getLanguage());
        juce::String error;
        const auto json = e.inProject ? pf.getChildFile ("framework.json").loadFileAsString()
                        : e.inLibrary ? lf.getChildFile ("framework.json").loadFileAsString() : builtinJson;
        if (reader.addPackage (json, id, error) && ! reader.getPackages().isEmpty())
        {
            const auto& p = reader.getPackages().getReference (0);
            e.order = p.order;
            e.name = reader.text (p.name);
            e.about = reader.text (p.about);
            for (auto& key : p.categories)
                if (auto* c = reader.findCategory (key))
                    e.categories.add (reader.text (c->name));
            if (e.inProject)
            {
                if (auto* loaded = framework.findPackage (p.id))
                    for (auto& w : loaded->warnings) e.problems.add (w);
                for (auto& gp : timeline.glyphProblems())
                    if (gp.startsWith (p.id + ":"))
                        e.problems.add ("Its glyphs.js could not be read (" + gp.fromFirstOccurrenceOf (":", false, false).trim()
                                        + "); Projet's shapes are drawn meanwhile.");
            }
        }
        else
        {
            e.problems.add ("Could not be read: " + error);
            e.unreadable = true;
        }

        const auto helpFile = (e.inProject ? pf : lf).getChildFile ("help.md");
        e.help = helpFile.existsAsFile() ? helpFile.loadFileAsString() : builtinHelp;
        entries.add (e);
    }
    // In the project first, then the library, then Projet's.
    std::stable_sort (entries.begin(), entries.end(), [] (const PackagesPanel::Entry& a, const PackagesPanel::Entry& b)
    {
        auto rank = [] (const PackagesPanel::Entry& e) { return e.inProject ? 0 : e.inLibrary ? 1 : 2; };
        if (rank (a) != rank (b)) return rank (a) < rank (b);
        return a.order != b.order ? a.order < b.order : a.id < b.id;
    });
    inspector.getPackagesPanel().setEntries (entries, framework.getLanguage());
}

void MainComponent::packagesAction (const juce::String& action, const juce::String& id)
{
    if (! folder.isValid()) return;
    const auto projectDir = Framework::projectFolder (folder.getFolder());
    const auto libraryDir = Framework::libraryFolder();
    juce::Component::SafePointer<MainComponent> safe (this);
    auto done = [safe] (const juce::Result& r, const juce::String& message)
    {
        if (safe == nullptr) return;
        safe->loadFramework (false);
        safe->refreshPackages();
        safe->transport.setStatus (r.failed() ? r.getErrorMessage() : message, r.failed());
    };
    auto confirm = [this] (const juce::String& title, const juce::String& text, const juce::String& button, std::function<void()> then)
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, title, text, button, "Cancel", this,
                                            juce::ModalCallbackFunction::create ([then] (int r) { if (r == 1) then(); }));
    };

    if (action == "reload")           { loadFramework (true); refreshPackages(); }
    else if (action == "showProject") projectDir.revealToUser();
    else if (action == "showLibrary") libraryDir.revealToUser();
    else if (action == "language")    setFrameworkLanguage (id);
    else if (action == "add")         addPackageFolder();
    else if (action == "show")
    {
        const auto pf = projectDir.getChildFile (id);
        (pf.isDirectory() ? pf : libraryDir.getChildFile (id)).revealToUser();
    }
    else if (action == "use" || action == "update")
        done (Framework::copyPackage (libraryDir.getChildFile (id), projectDir),
              action == "use" ? id + " is used in this project." : id + ": this project now has the library's version.");
    else if (action == "useBuiltin")
    {
        auto r = Framework::writeBuiltin (libraryDir, id);
        if (r.wasOk()) r = Framework::copyPackage (libraryDir.getChildFile (id), projectDir);
        done (r, id + " is in the library and used in this project.");
    }
    else if (action == "remove")
        confirm ("Remove " + id + " from this project?",
                 "This project stops using the package: its copy goes to the trash. The values the boxes have for its categories stay "
                 "(they come back if you use the package again).", "Remove", [safe, projectDir, id, done]
        {
            if (safe == nullptr) return;
            auto dir = projectDir.getChildFile (id);
            const bool ok = dir.moveToTrash() || dir.deleteRecursively();
            done (ok ? juce::Result::ok() : juce::Result::fail ("Could not remove " + dir.getFullPathName()), id + " removed from this project.");
        });
    else if (action == "toLibrary")
    {
        auto copy = [projectDir, libraryDir, id, done]
        {
            done (Framework::copyPackage (projectDir.getChildFile (id), libraryDir), id + ": the library has this project's version.");
        };
        if (libraryDir.getChildFile (id).isDirectory())
            confirm ("Replace the library's " + id + "?",
                     "The library's copy (v" + juce::String (Framework::folderVersion (libraryDir.getChildFile (id))) + ") is replaced by this project's (v"
                     + juce::String (Framework::folderVersion (projectDir.getChildFile (id))) + "). New projects get it; other projects keep their own copies.",
                     "Replace", copy);
        else copy();
    }
    else if (action == "install")
    {
        auto install = [libraryDir, id, done] { done (Framework::writeBuiltin (libraryDir, id), id + ": the library has Projet's version."); };
        if (libraryDir.getChildFile (id).isDirectory())
            confirm ("Replace the library's " + id + " with Projet's?",
                     "Changes you made to the library's copy are lost (projects keep their own copies). This project is not changed: "
                     "take the library's version here afterwards if you want it.", "Replace", install);
        else install();
    }
}

void MainComponent::addPackageFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a framework package (a folder with a framework.json)",
                                                   juce::File (juce::String (properties.getUserSettings()->getValue ("lastPackageParent"))), "", true);
    juce::Component::SafePointer<MainComponent> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        auto dir = fc.getResult();
        if (dir == juce::File()) return;
        if (dir.getFileName() == "framework.json") dir = dir.getParentDirectory();
        safe->properties.getUserSettings()->setValue ("lastPackageParent", dir.getParentDirectory().getFullPathName());
        const auto file = dir.getChildFile ("framework.json");
        Framework probe;
        juce::String error;
        if (! file.existsAsFile() || ! probe.addPackage (file.loadFileAsString(), dir.getFileName(), error))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Not a framework package",
                                                    file.existsAsFile() ? "Its framework.json could not be read: " + error
                                                                        : dir.getFullPathName() + " has no framework.json.");
            return;
        }
        // Copied under its id, into the library and this project.
        const auto id = probe.getPackages()[0].id;
        const auto staging = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ProjetPackage").getNonexistentSibling();
        auto r = juce::Result::ok();
        if (! dir.copyDirectoryTo (staging.getChildFile (id))) r = juce::Result::fail ("Could not copy " + dir.getFullPathName());
        if (r.wasOk()) r = Framework::copyPackage (staging.getChildFile (id), Framework::libraryFolder());
        if (r.wasOk()) r = Framework::copyPackage (staging.getChildFile (id), Framework::projectFolder (safe->folder.getFolder()));
        staging.deleteRecursively();
        safe->loadFramework (false);
        safe->refreshPackages();
        safe->transport.setStatus (r.failed() ? r.getErrorMessage() : id + " is in the library and used in this project.", r.failed());
    });
}

void MainComponent::showShortcuts()
{
    juce::String text;
    juce::Array<juce::CommandID> ids;
    getAllCommands (ids);
    juce::StringArray categories;
    for (auto id : ids)
        if (auto* c = commandManager.getCommandForID (id)) categories.addIfNotAlreadyThere (c->categoryName);

    for (auto& cat : categories)
    {
        text << cat << "\n";
        for (auto id : ids)
        {
            auto* c = commandManager.getCommandForID (id);
            if (c == nullptr || c->categoryName != cat) continue;
            juce::StringArray keys;
            for (auto& k : commandManager.getKeyMappings()->getKeyPressesAssignedToCommand (id))
                keys.add (k.getTextDescriptionWithIcons());
            text << "   " << c->shortName.upToFirstOccurrenceOf (" ...", false, false) << (keys.isEmpty() ? "" : "   " + keys.joinIntoString (", ")) << "\n";
        }
        text << "\n";
    }

    text << "Mouse\n"
         << "   Ruler: click or drag to move the playhead\n"
         << "   Box: click to select (Shift adds, Cmd toggles), drag to move all selected boxes\n"
         << "   Empty lane: drag a rectangle to select; double-click for a new text box\n"
         << "   Double-click a box: write its notes\n"
         << "   Proportion strip: drag a span; click its name for the frame and divisions\n"
         << "   Track header: drag to reorder or into a folder\n"
         << "   Box edges: drag to trim; small squares at the top: drag to set the fades\n"
         << "   Shift while dragging: no snapping\n"
         << "   Wheel: scroll; Shift-wheel: sideways; Cmd-wheel or pinch: zoom\n"
         << "   Track header: M / S, double-click the name to rename, right-click for more\n"
         << "   Drop sound files from the Finder onto a track to import them there\n";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Keyboard shortcuts", text);
}
