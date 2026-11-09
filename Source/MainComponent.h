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
#include "Model/Framework.h"
#include "Engine/SoundData.h"
#include "Engine/Importer.h"
#include "Engine/PlaybackEngine.h"
#include "UI/Theme.h"
#include "UI/TimelineView.h"
#include "UI/Inspector.h"
#include "UI/TransportBar.h"
#include "UI/ImportPanel.h"
#include "UI/StartPanel.h"
#include "UI/NotesWindow.h"

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
        cmdAddTrack, cmdAudioSettings, cmdShortcuts,
        cmdNewTextBox, cmdSplit, cmdCopy, cmdCut, cmdPaste, cmdSelectAll, cmdNotesWindow
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
    //== Framework packages (stage 4b) =========================================
    /** Reads the project's packages (made the first time from its 4a
        framework file, else copied from the library). */
    void loadFramework (bool announce);
    /** Puts any glyph-script problem in the status bar (never a dialog). */
    void showGlyphProblems();
    /** A view setting of the project (colour mode, display, language): it
        does not mark the project as changed. */
    void setViewSetting (const juce::Identifier&, const juce::String&);
    void setFrameworkLanguage (const juce::String&);
    void showPackages();
    /** Gives the Frameworks panel what is in the project, the library and Projet. */
    void refreshPackages();
    void packagesAction (const juce::String& action, const juce::String& id);
    void addPackageFolder();
    juce::StringArray namingKeys;           // the categories of the Framework menu's name submenus
    static constexpr int highlightBase = 40000;

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

    //== Editing ================================================================
    void addTextBox (juce::ValueTree track, double start);
    void editNotes (juce::ValueTree box);
    void openNotesWindow();
    void splitAtPlayhead (juce::Array<juce::ValueTree> boxes);
    void copySelection();
    void deleteSelection (const juce::String& undoName);
    void paste();
    void selectionChanged();

    struct ClipItem { juce::ValueTree box; int trackOffset = 0; double timeOffset = 0.0; };
    std::vector<ClipItem> clipboard;

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
    Framework framework;            // the open project's framework packages (stage 4a, 4b)
    TimelineView timeline { project, pool, framework };
    Inspector inspector { project, pool, folder, framework };
    ImportPanel importPanel;
    std::unique_ptr<NotesWindow> notesWindow;
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
