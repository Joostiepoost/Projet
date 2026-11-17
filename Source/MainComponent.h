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
#include "UI/LevelBar.h"
#include "UI/OutlinePanel.h"
#include "UI/MagnifierBar.h"
#include "UI/BoardView.h"
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
        cmdNewTextBox, cmdSplit, cmdCopy, cmdCut, cmdPaste, cmdSelectAll, cmdNotesWindow,
        cmdTogglePanel,
        cmdNewContainer, cmdGroup, cmdUngroup, cmdSwap,           // stage 5a
        cmdAudioEngine,
        cmdDiveIn, cmdLevelUp, cmdToggleOutline,                 // stage 5b
        cmdMagnify,                                              // stage 6
        cmdNextTake, cmdPreviousTake, cmdNewTake, cmdShowTakes,  // stage 7
        cmdNewBoard                                              // stage 9
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

    //== The panel on the right (stage 4c) ======================================
    /** Shown or hidden (remembered as an app setting). */
    void setPanelShown (bool);
    bool panelShown = true;
    int panelWidth = 320;
    static constexpr int minPanelWidth = 260;
    int maxPanelWidth() const   { return juce::jmax (minPanelWidth, juce::jmin (720, getWidth() - 480)); }
    /** The panel's left edge: drag to make it wider or narrower. */
    struct EdgeDragger : juce::Component
    {
        std::function<int()> getWidth_;
        std::function<void (int)> onWidth;
        std::function<void()> onDone;
        int startWidth = 0;
        EdgeDragger()                                           { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); setRepaintsOnMouseActivity (true); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (isMouseOverOrDragging() ? Theme::accent.withAlpha (0.6f) : Theme::line);
            g.fillRect (getWidth() / 2, 0, isMouseOverOrDragging() ? 2 : 1, getHeight());
        }
        void mouseDown (const juce::MouseEvent&) override      { startWidth = getWidth_ ? getWidth_() : 0; }
        void mouseDrag (const juce::MouseEvent& e) override     { if (onWidth) onWidth (startWidth - e.getDistanceFromDragStartX()); }
        void mouseUp (const juce::MouseEvent&) override         { if (onDone) onDone(); }
    } panelEdge;

    //== Sounds =================================================================
    void importDialog();
    void importFiles (const juce::StringArray& files, juce::ValueTree track, double start, const juce::String& takeBoxId = {});
    /** Sound files chosen to become new takes of a box (stage 7). */
    void addSoundTakes (juce::ValueTree box);
    struct ImportBatch
    {
        juce::String trackId;
        juce::String takeBoxId;            // stage 7: the sounds become takes of this box
        bool firstTakeMade = false;
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
    void addContainer (juce::ValueTree track, double start);
    void addBoard (juce::ValueTree track, double start);
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
    /** The audio engine on or off (off: the device is closed, saving CPU and battery). */
    bool audioOn = true;
    void setAudioOn (bool on, bool announce = true);

    juce::ApplicationProperties properties;
    juce::RecentlyOpenedFilesList recentProjects;
    juce::ApplicationCommandManager commandManager;

    TransportBar transport;
    Framework framework;            // the open project's framework packages (stage 4a, 4b)
    TimelineView timeline { project, pool, framework };
    Inspector inspector { project, pool, folder, framework };
    LevelBar levelBar { project };
    OutlinePanel outline { project };
    bool outlineShown = true;
    void setOutlineShown (bool);
    /** Shows a level (invalid: the piece). */
    void goTo (juce::ValueTree container);
    void focusChanged();
    /** The playhead in the time of the level worked on (5b). */
    double playheadInLevel() const                  { return project.pieceToLevel (engine.getPositionSeconds()); }

    //== The magnifier (stage 6) ================================================
    // A container of the level shown, magnified in a second timeline below.
    // The project's focus is the level of the view last clicked (active):
    // the panel, the commands and the playhead commands work there.
    TimelineView magnifier { project, pool, framework };
    MagnifierBar magnifierBar;
    TimelineView* active = &timeline;
    TimelineView& view()                            { return *active; }
    bool magnifierShown = false, quietFocus = false;
    double magnifierShare = 0.5, shareAtDragStart = 0.5;    // the magnifier's part of the height
    juce::Rectangle<int> viewsArea;
    void wireView (TimelineView&);
    /** The view becomes the one worked on: the focus moves quietly to its level. */
    void activate (TimelineView&);
    /** Magnify a container of the level shown (fitted). */
    void openMagnifier (juce::ValueTree container);
    void closeMagnifier();
    /** From either view: the upper one's containers are magnified; one inside
        the magnified container goes a level down (the upper view goes inside
        the magnified one, and magnifies it). */
    void magnify (TimelineView& from, juce::ValueTree container);
    void updateMagnifierBar();
    /** The view showing the level a box or track lies in, if any. */
    TimelineView* viewFor (const juce::ValueTree& node);
    /** Selects a box in the view showing its level. */
    void selectInView (const juce::ValueTree& box);
    void repaintViews()                             { timeline.repaint(); magnifier.repaint(); }
    void levelsChanged();
    //== The board (stage 9) ====================================================
    // A board box's board, open below the timeline where the magnifier opens
    // (one of the two at a time), or filling the area. Clicked, it is the one
    // worked on: the panel shows what is selected on it, and the editing
    // commands (delete, copy, paste, select all, zoom) go to it.
    BoardView board { project, framework, folder };
    MagnifierBar boardBar;
    bool boardShown = false, boardFilled = false, boardActive = false;
    double boardShare = 0.55, boardShareAtDragStart = 0.55;
    void openBoard (juce::ValueTree box);
    void closeBoard();
    void activateBoard();
    void updateBoardBar();
    void setBoardFilled (bool);
    /** The editing commands the board takes while it is worked on. */
    bool boardTakes (juce::CommandID) const;
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
    juce::String libraryNotice;         // what the library gained at start (shown once a project is open)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
