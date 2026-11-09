// MainComponent.cpp

#include "MainComponent.h"
#include "Engine/Arrangement.h"
#include "UI/TimeFormat.h"

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
    addChildComponent (startPanel);

    startPanel.onNew = [this] { newProjectDialog(); };
    startPanel.onOpen = [this] { openProjectDialog(); };
    startPanel.onOpenRecent = [this] (const juce::File& f) { openProject (f); };

    transport.onPlayStop = [this] { playStop(); };
    transport.onToStart = [this] { locate (0.0); };
    transport.onZoomIn = [this] { timeline.zoomBy (1.5, engine.getPositionSeconds()); };
    transport.onZoomOut = [this] { timeline.zoomBy (1.0 / 1.5, engine.getPositionSeconds()); };
    transport.onZoomFit = [this] { timeline.zoomToFit(); };
    transport.onImport = [this] { importDialog(); };

    timeline.onLocate = [this] (double t) { locate (t); };
    timeline.onSelectionChanged = [this] { selectionChanged(); };
    timeline.onAddTextBox = [this] (juce::ValueTree track, double t) { addTextBox (track, t); };
    timeline.onEditNotes = [this] (juce::ValueTree box) { editNotes (box); };
    timeline.onOpenNotesWindow = [this] (juce::ValueTree box) { timeline.selectBox (box); openNotesWindow(); };
    timeline.onSplitAtPlayhead = [this] (juce::ValueTree box)
    {
        auto boxes = timeline.getSelectedBoxes();
        if (boxes.isEmpty()) boxes.add (box);
        splitAtPlayhead (boxes);
    };
    inspector.onExpandNotes = [this] { openNotesWindow(); };
    timeline.onStatus = [this] (const juce::String& m, bool warning) { transport.setStatus (m, warning); };
    timeline.onShowRulerPanel = [this] (const juce::String& layerId) { setPanelShown (true); inspector.showRuler (layerId); };
    inspector.getRulerPanel().getPlayhead = [this] { return engine.getPositionSeconds(); };
    inspector.getRulerPanel().setProperties (properties.getUserSettings());
    inspector.getRulerPanel().onStatus = [this] (const juce::String& m, bool warning) { transport.setStatus (m, warning); };
    inspector.getDisplayPanel().setProperties (properties.getUserSettings());
    inspector.getDisplayPanel().setView = [this] (const juce::Identifier& id, const juce::String& v) { setViewSetting (id, v); };
    inspector.getPackagesPanel().onAction = [this] (const juce::String& a, const juce::String& id) { packagesAction (a, id); };
    inspector.onShowFrameworks = [this] { showPackages(); };
    timeline.onFilesDropped = [this] (const juce::StringArray& files, juce::ValueTree track, double t) { importFiles (files, track, t); };
    timeline.onRevealOriginal = [this] (juce::ValueTree box)
    {
        auto s = project.findSound (box[IDs::soundId].toString());
        juce::File f (s[IDs::originalPath].toString());
        if (f.exists()) f.revealToUser();
        else transport.setStatus ("The original of this sound is no longer at " + f.getFullPathName(), true);
    };
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
        timeline.repaint();
        inspector.refresh();
    };

    // Audio: 2 outputs, no inputs. The device and rate are remembered.
    std::unique_ptr<juce::XmlElement> audioState (settings->getXmlValue ("audioState"));
    const auto err = deviceManager.initialise (0, 2, audioState.get(), true);
    if (err.isNotEmpty()) transport.setStatus ("Audio: " + err, true);
    deviceManager.addAudioCallback (&engine);
    deviceManager.addChangeListener (this);

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
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    stopTimer();
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

void MainComponent::resized()
{
    auto r = getLocalBounds();
    transport.setBounds (r.removeFromTop (48));
    transport.setPanelShown (panelShown);
    inspector.setVisible (panelShown);
    panelEdge.setVisible (panelShown);
    if (panelShown)
    {
        panelWidth = juce::jlimit (minPanelWidth, maxPanelWidth(), panelWidth);
        inspector.setBounds (r.removeFromRight (panelWidth));
        panelEdge.setBounds (r.removeFromRight (5));
    }
    timeline.setBounds (r);
    startPanel.setBounds (getLocalBounds());
    layoutImportPanel();
}

void MainComponent::layoutImportPanel()
{
    const auto t = timeline.getBounds();
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
    timeline.selectBox ({});
    auto& s = project.getState();
    timeline.setView (s.getProperty (IDs::viewStart, 0.0), s.getProperty (IDs::pixelsPerSecond, 20.0));
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
    inspector.commitNotes();
    if (notesWindow != nullptr) notesWindow->setBox (notesWindow->isVisible() ? timeline.getSelectedBox() : juce::ValueTree());
    {
        const juce::ScopedValueSetter<bool> svs (suppressDirty, true);
        auto& s = project.getState();
        s.setProperty (IDs::viewStart, timeline.getViewStart(), nullptr);
        s.setProperty (IDs::pixelsPerSecond, timeline.getPixelsPerSecond(), nullptr);
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
    syncSounds();
    rebuildArrangement();
    inspector.refresh();
    if (notesWindow != nullptr) notesWindow->refresh();
    timeline.repaint();
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
        importFiles (sounds, timeline.getSelectedLaneTrack(), engine.getPositionSeconds());
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
        safeThis->importFiles (paths, safeThis->timeline.getSelectedLaneTrack(), safeThis->engine.getPositionSeconds());
    });
}

void MainComponent::importFiles (const juce::StringArray& files, juce::ValueTree track, double start)
{
    if (! folder.isValid() || files.isEmpty()) return;

    auto batch = std::make_shared<ImportBatch>();
    batch->trackId = track[IDs::id].toString();
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

        auto track = project.findTrack (batch->trackId);
        if (track.isValid() && Project::isFolder (track)) track = {};
        if (! track.isValid()) track = project.laneTracks().getFirst();
        if (! track.isValid()) track = project.addTrack();

        project.getUndoManager().beginNewTransaction ("Import " + r.original.getFileName());
        auto box = project.addSoundBox (track, batch->cursor, soundId);
        batch->cursor = Project::boxEnd (box);
        timeline.selectBox (box);
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
    inspector.setSelection (timeline.getSelectedBoxes());
    if (notesWindow != nullptr && notesWindow->isVisible())
        notesWindow->setBox (timeline.getSelectedBox());
}

void MainComponent::addTextBox (juce::ValueTree track, double start)
{
    if (! track.isValid() || Project::isFolder (track)) track = timeline.getSelectedLaneTrack();
    if (! track.isValid()) return;
    project.getUndoManager().beginNewTransaction ("New text box");
    // About 160 pixels long at the current zoom, at least 2 s.
    auto box = project.addTextBox (track, start, juce::jmax (2.0, 160.0 / timeline.getPixelsPerSecond()));
    timeline.selectBox (box);
    inspector.focusNotes();
}

void MainComponent::editNotes (juce::ValueTree box)
{
    timeline.selectBox (box);
    inspector.focusNotes();
}

void MainComponent::openNotesWindow()
{
    inspector.commitNotes();
    if (notesWindow == nullptr)
        notesWindow = std::make_unique<NotesWindow> (project);
    notesWindow->setBox (timeline.getSelectedBox());
    notesWindow->setVisible (true);
    notesWindow->toFront (true);
}

void MainComponent::splitAtPlayhead (juce::Array<juce::ValueTree> boxes)
{
    const double t = engine.getPositionSeconds();
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
    else timeline.setSelection (rights);
}

void MainComponent::copySelection()
{
    auto boxes = timeline.getSelectedBoxes();
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
    auto boxes = timeline.getSelectedBoxes();
    if (boxes.isEmpty()) return;
    project.getUndoManager().beginNewTransaction (undoName);
    for (auto b : boxes) project.removeBox (b);
    timeline.selectBox ({});
}

void MainComponent::paste()
{
    if (clipboard.empty()) return;

    // At the playhead, starting on the selected track; more tracks are added if needed.
    const double t = engine.getPositionSeconds();
    auto lanes = project.laneTracks();
    const int base = juce::jmax (0, lanes.indexOf (timeline.getSelectedLaneTrack()));

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
    timeline.setSelection (pasted);
}

//== Transport =================================================================

void MainComponent::playStop()
{
    if (engine.isPlaying()) engine.stop();
    else engine.play();
    transport.setPlaying (engine.isPlaying());
}

void MainComponent::locate (double seconds)
{
    engine.locate (seconds);
    timeline.setPlayhead (seconds);
    transport.setTime (seconds);
}

void MainComponent::timerCallback()
{
    engine.purgeRetired();
    if (importPanel.isVisible())
    {
        const bool show = importPanel.update();
        importPanel.setVisible (show);
        if (show) layoutImportPanel();
    }
    const double t = engine.getPositionSeconds();
    timeline.setPlayhead (t);
    transport.setTime (t);
    transport.setPlaying (engine.isPlaying());
    transport.setPeaks (engine.getPeak (0), engine.getPeak (1));
    if (engine.isPlaying()) timeline.followPlayhead (t);

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
    if (auto xml = deviceManager.createStateXml())
    {
        properties.getUserSettings()->setValue ("audioState", xml.get());
        properties.saveIfNeeded();
    }
    lastStatus = "-";   // re-check the rate
}

void MainComponent::showAudioSettings()
{
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
                  cmdAddTrack, cmdAudioSettings, cmdShortcuts,
                  cmdNewTextBox, cmdSplit, cmdCopy, cmdCut, cmdPaste, cmdSelectAll, cmdNotesWindow, cmdTogglePanel });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const bool hasBox = hasProject() && ! timeline.getSelectedBoxes().isEmpty();
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
            info.setActive (hasBox);
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

        case cmdAddTrack:      info.setInfo ("Add track", "Add a track below the selected one", "Track", 0); info.addDefaultKeypress ('n', cmd | shift); info.setActive (open); break;
        case cmdAudioSettings: info.setInfo ("Audio settings...", "Output device and sample rate", "Audio", 0); info.addDefaultKeypress (',', cmd); break;
        case cmdShortcuts:     info.setInfo ("Keyboard shortcuts", "List the shortcuts", "Help", 0); info.addDefaultKeypress ('/', cmd); break;
        case cmdNewTextBox:    info.setInfo ("New text box", "A text box on the selected track at the playhead", "Edit", 0); info.addDefaultKeypress ('b', cmd); info.setActive (open); break;
        case cmdSplit:         info.setInfo ("Split at playhead", "Cut the selected boxes at the playhead (every box under it if none is selected)", "Edit", 0); info.addDefaultKeypress ('t', cmd); info.setActive (open); break;
        case cmdCopy:          info.setInfo ("Copy", "Copy the selected boxes", "Edit", 0); info.addDefaultKeypress ('c', cmd); info.setActive (hasBox); break;
        case cmdCut:           info.setInfo ("Cut", "Cut the selected boxes", "Edit", 0); info.addDefaultKeypress ('x', cmd); info.setActive (hasBox); break;
        case cmdPaste:         info.setInfo ("Paste at playhead", "Paste on the selected track at the playhead", "Edit", 0); info.addDefaultKeypress ('v', cmd); info.setActive (open && ! clipboard.empty()); break;
        case cmdSelectAll:     info.setInfo ("Select all boxes", "Select every box", "Edit", 0); info.addDefaultKeypress ('a', cmd); info.setActive (open); break;
        case cmdNotesWindow:   info.setInfo ("Notes in a large window", "Open the selected box's notes in a large window", "Edit", 0); info.addDefaultKeypress ('e', cmd); info.setActive (open); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    if (! hasProject() && info.commandID != cmdNewProject && info.commandID != cmdOpenProject
        && info.commandID != cmdAudioSettings && info.commandID != cmdShortcuts && info.commandID != cmdTogglePanel)
        return true;

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
            auto boxes = timeline.getSelectedBoxes();
            if (boxes.isEmpty()) break;
            project.getUndoManager().beginNewTransaction ("Duplicate");
            juce::Array<juce::ValueTree> copies;
            for (auto b : boxes) copies.add (project.duplicateBox (b));
            timeline.setSelection (copies);
            break;
        }
        case cmdNewTextBox:   addTextBox (timeline.getSelectedLaneTrack(), engine.getPositionSeconds()); break;
        case cmdSplit:        splitAtPlayhead (timeline.getSelectedBoxes()); break;
        case cmdCopy:         copySelection(); break;
        case cmdCut:          copySelection(); deleteSelection ("Cut"); break;
        case cmdPaste:        paste(); break;
        case cmdSelectAll:    timeline.selectAll(); break;
        case cmdNotesWindow:  openNotesWindow(); break;
        case cmdDeselect:     timeline.selectBox ({}); break;

        case cmdPlayStop:     playStop(); break;
        case cmdToStart:      locate (0.0); break;
        case cmdZoomIn:       timeline.zoomBy (1.5, engine.getPositionSeconds()); break;
        case cmdZoomOut:      timeline.zoomBy (1.0 / 1.5, engine.getPositionSeconds()); break;
        case cmdZoomFit:      timeline.zoomToFit(); break;
        case cmdTogglePanel:  setPanelShown (! panelShown); break;

        case cmdAddTrack:
        {
            // After the selected track (and everything inside it), in the same folder.
            project.getUndoManager().beginNewTransaction ("Add track");
            auto sel = timeline.getSelectedTrack();
            if (sel.isValid()) project.addTrack ({}, project.blockEnd (sel), sel[IDs::folderId].toString());
            else project.addTrack();
            break;
        }
        case cmdAudioSettings: showAudioSettings(); break;
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
            add ({ 0, cmdSaveProject, cmdRevealFolder, 0, cmdImport });
            break;
        }
        case 1: add ({ cmdUndo, cmdRedo, 0, cmdCut, cmdCopy, cmdPaste, cmdDuplicate, cmdDelete, 0, cmdSplit, cmdNewTextBox, cmdNotesWindow, 0, cmdSelectAll, cmdDeselect }); break;
        case 2:
        {
            add ({ cmdZoomIn, cmdZoomOut, cmdZoomFit, 0, cmdTogglePanel });
            m.addSeparator();
            // Colour modes (stage 4a, 4b): 6000 = track colours, 6001 + category index
            const auto mode = project.getState().getProperty (IDs::colourMode, "own").toString();
            juce::PopupMenu colours;
            colours.addItem (6000, "Track colours", hasProject(), mode == "own");
            for (auto& c : framework.getCategories())
                if ((c.kind == Framework::Kind::choice || c.kind == Framework::Kind::tags) && framework.hasColours (c) && c.index < 98)
                {
                    bool ownColours = false;
                    for (auto& v : framework.valuesOf (c)) ownColours = ownColours || v.colours.colour != 0;
                    const auto& groupsName = framework.source (c).groupsName;
                    colours.addItem (6001 + c.index, framework.text (c.name) + (! ownColours && ! groupsName.isEmpty() ? " (" + framework.text (groupsName) + ")" : juce::String()),
                                     hasProject(), mode == c.key());
                }
            m.addSubMenu ("Colour boxes by", colours, hasProject());

            // Highlight: 6100 clears; values at highlightBase + index * 500 + 1 + i, groups at + 251 + i
            const auto& h = timeline.getHighlight();
            juce::PopupMenu hl;
            hl.addItem (6100, "Show all (clear)", h.isActive());
            for (auto& c : framework.getCategories())
            {
                if (c.valuesFrom.isNotEmpty() || (c.kind != Framework::Kind::choice && c.kind != Framework::Kind::tags)) continue;
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
        case 3: add ({ cmdPlayStop, cmdToStart }); break;
        case 4: add ({ cmdAddTrack }); break;
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
        case 6: add ({ cmdAudioSettings }); break;
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
        if (id == 6100) timeline.clearHighlight();
        else
        {
            const auto& c = framework.getCategories().getReference ((id - highlightBase) / 500);
            const int item = (id - highlightBase) % 500;
            if (item >= 251 && juce::isPositiveAndBelow (item - 251, framework.groupsOf (c).size()))
                timeline.toggleHighlight (c.key() + "#groups", framework.groupsOf (c)[item - 251].id);
            else if (item >= 1 && juce::isPositiveAndBelow (item - 1, framework.valuesOf (c).size()))
                timeline.toggleHighlight (c.key(), framework.valuesOf (c)[item - 1].id);
        }
        const auto& h = timeline.getHighlight();
        transport.setStatus (h.isActive() ? "Highlighting " + h.describe (framework) + " (View > Highlight > Show all to clear)" : juce::String());
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
    timeline.repaint();
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
