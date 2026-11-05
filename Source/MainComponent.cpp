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

    transport.onPlayStop = [this] { playStop(); };
    transport.onToStart = [this] { locate (0.0); };
    transport.onZoomIn = [this] { timeline.zoomBy (1.5, engine.getPositionSeconds()); };
    transport.onZoomOut = [this] { timeline.zoomBy (1.0 / 1.5, engine.getPositionSeconds()); };
    transport.onZoomFit = [this] { timeline.zoomToFit(); };
    transport.onImport = [this] { importDialog(); };

    timeline.onLocate = [this] (double t) { locate (t); };
    timeline.onSelectionChanged = [this] { inspector.setBox (timeline.getSelectedBox()); };
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

    openLastOrDefault();

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
}

//== Project ===================================================================

void MainComponent::openLastOrDefault()
{
    const juce::File last (properties.getUserSettings()->getValue ("lastProject"));
    if (last.existsAsFile() && openProject (last))
        return;

    // No project yet: a fresh one in ~/Music/Projet.
    auto parent = ProjectFolder::defaultParent();
    parent.createDirectory();
    auto f = ProjectFolder::unusedFolder (parent, "Untitled");
    const auto result = folder.create (f, project, Project::defaultSampleRate);
    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Projet",
                                                "Could not create a project folder: " + result.getErrorMessage());
    afterProjectLoaded();
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
    pool.clear();
    timeline.selectBox ({});
    auto& s = project.getState();
    timeline.setView (s.getProperty (IDs::viewStart, 0.0), s.getProperty (IDs::pixelsPerSecond, 20.0));
    locate (s.getProperty (IDs::playhead, 0.0));
    syncSounds();
    rebuildArrangement();
    inspector.setBox ({});
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
    transport.setProjectName (project.getName(), dirty);
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setName ("Projet - " + project.getName() + (dirty ? " *" : ""));
}

void MainComponent::markClean()
{
    dirty = false;
    updateTitle();
}

void MainComponent::saveProject()
{
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
    if (! dirty) { next(); return; }

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
    chooser = std::make_unique<juce::FileChooser> ("Name the new project (a folder of that name is made)",
                                                   ProjectFolder::unusedFolder (ProjectFolder::defaultParent(), "Untitled"), "", true);
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
            self.openProject (created.getProjectFile());
        }), true);
    });
}

void MainComponent::openProjectDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a project", ProjectFolder::defaultParent(), juce::String ("*") + ProjectFolder::extension, true);
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
    timeline.repaint();
    updateTitle();
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
        importer.start (source, dest, settings, [safeThis, batch, i] (const projet::ImportResult& r)
        {
            if (safeThis == nullptr) return;
            batch->results[(size_t) i] = r;
            safeThis->placeReady (batch);
        });
    }

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
            batch->errors.add (r.original.getFileName() + ": " + r.error);
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
        if (batch->errors.isEmpty())
            transport.setStatus ("Imported " + juce::String (batch->results.size()) + (batch->results.size() == 1 ? " sound." : " sounds."));
        else
        {
            transport.setStatus (juce::String (batch->errors.size()) + " sounds could not be imported.", true);
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Some sounds could not be imported",
                                                    batch->errors.joinIntoString ("\n"));
        }
    }
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
                  cmdAddTrack, cmdAudioSettings, cmdShortcuts });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    const auto cmd = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const bool hasBox = timeline.getSelectedBox().isValid();

    switch (id)
    {
        case cmdNewProject:    info.setInfo ("New project...", "Make a new project folder", "File", 0); info.addDefaultKeypress ('n', cmd); break;
        case cmdOpenProject:   info.setInfo ("Open project...", "Open a project", "File", 0); info.addDefaultKeypress ('o', cmd); break;
        case cmdSaveProject:   info.setInfo ("Save", "Save the project", "File", 0); info.addDefaultKeypress ('s', cmd); break;
        case cmdRevealFolder:  info.setInfo ("Show project folder in Finder", "Reveal the project folder", "File", 0); break;
        case cmdImport:        info.setInfo ("Import sounds...", "Import sound files as placeholder copies", "File", 0); info.addDefaultKeypress ('i', cmd); break;

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
            info.setInfo ("Delete box", "Delete the selected box", "Edit", 0);
            info.setActive (hasBox);
            info.addDefaultKeypress (KP::backspaceKey, 0);
            info.addDefaultKeypress (KP::deleteKey, 0);
            break;
        case cmdDuplicate:
            info.setInfo ("Duplicate box", "Duplicate the selected box after itself", "Edit", 0);
            info.setActive (hasBox);
            info.addDefaultKeypress ('d', cmd);
            break;
        case cmdDeselect:      info.setInfo ("Select nothing", "Clear the selection", "Edit", 0); info.addDefaultKeypress (KP::escapeKey, 0); break;

        case cmdPlayStop:      info.setInfo ("Play / stop", "Start or stop playback", "Transport", 0); info.addDefaultKeypress (KP::spaceKey, 0); break;
        case cmdToStart:       info.setInfo ("Back to the start", "Move the playhead to 0:00", "Transport", 0); info.addDefaultKeypress (KP::returnKey, 0); info.addDefaultKeypress (KP::homeKey, 0); break;

        case cmdZoomIn:        info.setInfo ("Zoom in", "Zoom in", "View", 0); info.addDefaultKeypress ('=', cmd); break;
        case cmdZoomOut:       info.setInfo ("Zoom out", "Zoom out", "View", 0); info.addDefaultKeypress ('-', cmd); break;
        case cmdZoomFit:       info.setInfo ("Show the whole piece", "Zoom to fit", "View", 0); info.addDefaultKeypress ('0', cmd); break;

        case cmdAddTrack:      info.setInfo ("Add track", "Add a track below the selected one", "Track", 0); info.addDefaultKeypress ('n', cmd | shift); break;
        case cmdAudioSettings: info.setInfo ("Audio settings...", "Output device and sample rate", "Audio", 0); info.addDefaultKeypress (',', cmd); break;
        case cmdShortcuts:     info.setInfo ("Keyboard shortcuts", "List the shortcuts", "Help", 0); info.addDefaultKeypress ('/', cmd); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    auto box = timeline.getSelectedBox();

    switch (info.commandID)
    {
        case cmdNewProject:   askToSaveThen ([this] { newProjectDialog(); }); break;
        case cmdOpenProject:  askToSaveThen ([this] { openProjectDialog(); }); break;
        case cmdSaveProject:  saveProject(); break;
        case cmdRevealFolder: folder.getProjectFile().revealToUser(); break;
        case cmdImport:       importDialog(); break;

        case cmdUndo:         project.getUndoManager().undo(); break;
        case cmdRedo:         project.getUndoManager().redo(); break;
        case cmdDelete:
            if (box.isValid()) { project.getUndoManager().beginNewTransaction ("Delete box"); project.removeBox (box); timeline.selectBox ({}); }
            break;
        case cmdDuplicate:
            if (box.isValid()) { project.getUndoManager().beginNewTransaction ("Duplicate box"); timeline.selectBox (project.duplicateBox (box)); }
            break;
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
        case 1: add ({ cmdUndo, cmdRedo, 0, cmdDuplicate, cmdDelete, 0, cmdDeselect }); break;
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
         << "   Box: click to select, drag to move (also to another track)\n"
         << "   Box edges: drag to trim; small squares at the top: drag to set the fades\n"
         << "   Shift while dragging: no snapping\n"
         << "   Wheel: scroll; Shift-wheel: sideways; Cmd-wheel or pinch: zoom\n"
         << "   Track header: M / S, double-click the name to rename, right-click for more\n"
         << "   Drop sound files from the Finder onto a track to import them there\n";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Keyboard shortcuts", text);
}
