// MainComponent.h — everything inside Projet's main window.
//
// Stage 1 / 1.1: the project folder (new, open, save, backups), importing sounds
// as compact placeholder copies, a flat timeline with tracks and sound boxes,
// and sample-accurate playback with gain, fades and crossfades. With no
// project open (first start) the start panel covers everything.

#pragma once

#include <JuceHeader.h>
#include <optional>

#include "Model/Project.h"
#include "Model/ProjectFolder.h"
#include "Engine/SoundData.h"
#include "Engine/Importer.h"
#include "Engine/PlaybackEngine.h"
#include "UI/Theme.h"
#include "UI/TimelineView.h"
#include "UI/Inspector.h"
#include "UI/TransportBar.h"
#include "UI/ImportPanel.h"
#include "UI/StartPanel.h"

class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum CommandIDs
    {
        cmdNewProject = 0x3000, cmdOpenProject, cmdSaveProject, cmdRevealFolder, cmdImport,
        cmdUndo, cmdRedo, cmdDelete, cmdDuplicate, cmdDeselect,
        cmdPlayStop, cmdToStart, cmdZoomIn, cmdZoomOut, cmdZoomFit,
        cmdAddTrack, cmdAudioSettings, cmdShortcuts
    };

    /** A .projet file opens; sound files are imported on the selected track at the playhead. */
    void openFiles (const juce::StringArray& paths);

    /** Asks to save unsaved changes (Save / Don't save / Cancel), then runs `next` unless cancelled. */
    void askToSaveThen (std::function<void()> next);

    juce::ApplicationCommandTarget* getNextCommandTarget() override   { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int) override;

private:
    void timerCallback() override;
    void modelChanged();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    //== Project ================================================================
    void openLastOrShowStart();
    void showStart();
    void layoutImportPanel();
    bool hasProject() const      { return folder.isValid(); }
    bool openProject (const juce::File&);
    void newProjectDialog();
    void openProjectDialog();
    void saveProject();
    void afterProjectLoaded();
    void rememberProject();
    void updateTitle();
    void markClean();

    //== Sounds =================================================================
    void importDialog();
    void importFiles (const juce::StringArray& files, juce::ValueTree track, double start);
    struct ImportBatch
    {
        juce::String trackId;
        double cursor = 0.0;
        int next = 0;
        std::vector<std::optional<projet::ImportResult>> results;
        juce::StringArray errors;
        int cancelled = 0;
    };
    void placeReady (std::shared_ptr<ImportBatch>);
    void syncSounds();
    void rebuildArrangement();

    //== Transport ==============================================================
    void playStop();
    void locate (double seconds);
    void showAudioSettings();
    void showShortcuts();

    Theme::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    Project project;
    ProjectFolder folder;
    projet::SoundPool pool;
    projet::Importer importer;
    projet::PlaybackEngine engine;
    juce::AudioDeviceManager deviceManager;

    juce::ApplicationProperties properties;
    juce::RecentlyOpenedFilesList recentProjects;
    juce::ApplicationCommandManager commandManager;

    TransportBar transport;
    TimelineView timeline { project, pool };
    Inspector inspector { project, pool, folder };
    ImportPanel importPanel;
    StartPanel startPanel;

    // MenuBarModel is already an AsyncUpdater, so model changes are coalesced here.
    struct Updater : juce::AsyncUpdater
    {
        std::function<void()> callback;
        void handleAsyncUpdate() override { if (callback) callback(); }
    } updater;

    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::shared_ptr<ImportBatch>> batches;
    bool dirty = false, suppressDirty = false;
    juce::String lastStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
