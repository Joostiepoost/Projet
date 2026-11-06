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
    addChildComponent (importPanel);
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

void MainComponent::resized()
{
    auto r = getLocalBounds();
    transport.setBounds (r.removeFromTop (48));
    inspector.setBounds (r.removeFromRight (320));
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
        importFiles (sounds, timeline.getSelectedTrack(), engine.getPositionSeconds());
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
        safeThis->importFiles (paths, safeThis->timeline.getSelectedTrack(), safeThis->engine.getPositionSeconds());
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

        auto track = project.getTracks().getChildWithProperty (IDs::id, batch->trackId);
        if (! track.isValid()) track = project.getTrack (0);
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
    if (! track.isValid()) track = timeline.getSelectedTrack();
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

    auto tracks = project.getTracks();
    double firstTime = 1.0e12;
    int firstTrack = 1 << 30;
    for (auto b : boxes)
    {
        firstTime = juce::jmin (firstTime, Project::boxStart (b));
        firstTrack = juce::jmin (firstTrack, tracks.indexOf (b.getParent()));
    }

    clipboard.clear();
    for (auto b : boxes)
        clipboard.push_back ({ b.createCopy(), tracks.indexOf (b.getParent()) - firstTrack, Project::boxStart (b) - firstTime });
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
    auto tracks = project.getTracks();
    const int base = juce::jmax (0, tracks.indexOf (timeline.getSelectedTrack()));

    project.getUndoManager().beginNewTransaction ("Paste");
    juce::Array<juce::ValueTree> pasted;
    for (auto& item : clipboard)
    {
        while (tracks.getNumChildren() <= base + item.trackOffset) project.addTrack();
        auto copy = item.box.createCopy();
        copy.setProperty (IDs::id, Project::newId(), nullptr);
        copy.setProperty (IDs::start, t + item.timeOffset, nullptr);
        tracks.getChild (base + item.trackOffset).appendChild (copy, project.um());
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
                  cmdNewTextBox, cmdSplit, cmdCopy, cmdCut, cmdPaste, cmdSelectAll, cmdNotesWindow });
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
        && info.commandID != cmdAudioSettings && info.commandID != cmdShortcuts)
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
        case cmdNewTextBox:   addTextBox (timeline.getSelectedTrack(), engine.getPositionSeconds()); break;
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

        case cmdAddTrack:
        {
            project.getUndoManager().beginNewTransaction ("Add track");
            const int index = project.getTracks().indexOf (timeline.getSelectedTrack());
            project.addTrack ({}, index < 0 ? -1 : index + 1);
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
    return { "File", "Edit", "View", "Transport", "Track", "Audio", "Help" };
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
        case 2: add ({ cmdZoomIn, cmdZoomOut, cmdZoomFit }); break;
        case 3: add ({ cmdPlayStop, cmdToStart }); break;
        case 4: add ({ cmdAddTrack }); break;
        case 5: add ({ cmdAudioSettings }); break;
        case 6: add ({ cmdShortcuts }); break;
        default: break;
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    if (id >= 5000 && id < 5100)
    {
        const auto file = recentProjects.getFile (id - 5000);
        juce::Component::SafePointer<MainComponent> safeThis (this);
        askToSaveThen ([safeThis, file] { if (safeThis != nullptr) safeThis->openProject (file); });
    }
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
         << "   Box edges: drag to trim; small squares at the top: drag to set the fades\n"
         << "   Shift while dragging: no snapping\n"
         << "   Wheel: scroll; Shift-wheel: sideways; Cmd-wheel or pinch: zoom\n"
         << "   Track header: M / S, double-click the name to rename, right-click for more\n"
         << "   Drop sound files from the Finder onto a track to import them there\n";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Keyboard shortcuts", text);
}
