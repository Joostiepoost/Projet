// ProjectFolder.h — one folder per piece.
//
//   <name>/
//     <name>.projet   the piece (JSON)
//     Sounds/         placeholder copies of every imported sound
//     Boards/         images placed on boards (later stages)
//     Exports/        Reaper exports (later stages)
//     Backups/        the previous versions of <name>.projet, one per save
//
// Sounds are referred to by paths relative to the folder, so the folder can
// be moved or renamed as a whole.

#pragma once

#include <JuceHeader.h>

class Project;

class ProjectFolder
{
public:
    static constexpr const char* extension = ".projet";
    static constexpr int backupsKept = 30;

    ProjectFolder() = default;

    bool isValid() const                            { return projectFile != juce::File(); }
    juce::File getFolder() const                    { return projectFile.getParentDirectory(); }
    juce::File getProjectFile() const               { return projectFile; }
    juce::File getSoundsFolder() const              { return getFolder().getChildFile ("Sounds"); }
    juce::File getBackupsFolder() const             { return getFolder().getChildFile ("Backups"); }

    /** Makes `folder` (which must not exist yet, or be empty) with its subfolders,
        resets the project to an empty one named after the folder, and saves it. */
    juce::Result create (const juce::File& folder, Project& project, double sampleRate);

    /** Opens a .projet file (or a folder containing one) into `project`. */
    juce::Result open (const juce::File& fileOrFolder, Project& project);

    /** Writes the project; the previous file goes to Backups first. */
    juce::Result save (Project& project);

    /** Absolute file for a path stored in the project. */
    juce::File resolve (const juce::String& relativePath) const;
    /** Path to store in the project for a file inside the folder. */
    juce::String relativePathOf (const juce::File& f) const;

    /** A new, unused file name in Sounds for a sound called `name`. */
    juce::File newSoundFile (const juce::String& name, const juce::String& extensionWithDot) const;

    /** ~/Music/Projet, where new projects go by default. */
    static juce::File defaultParent();
    /** "Untitled", "Untitled 2", ... : a folder name not yet used in `parent`. */
    static juce::File unusedFolder (const juce::File& parent, const juce::String& base);

private:
    void ensureSubfolders() const;
    void writeBackup() const;

    juce::File projectFile;
};
