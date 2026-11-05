// ProjectFolder.cpp

#include "ProjectFolder.h"
#include "Project.h"

juce::File ProjectFolder::defaultParent()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Projet");
}

juce::File ProjectFolder::unusedFolder (const juce::File& parent, const juce::String& base)
{
    auto f = parent.getChildFile (base);
    for (int n = 2; f.exists(); ++n)
        f = parent.getChildFile (base + " " + juce::String (n));
    return f;
}

void ProjectFolder::ensureSubfolders() const
{
    for (auto* sub : { "Sounds", "Boards", "Exports", "Backups" })
        getFolder().getChildFile (sub).createDirectory();
}

juce::Result ProjectFolder::create (const juce::File& folder, Project& project, double sampleRate)
{
    if (folder.existsAsFile())
        return juce::Result::fail (folder.getFullPathName() + " is a file, not a folder.");

    if (folder.isDirectory() && folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) > 0)
        return juce::Result::fail ("The folder " + folder.getFileName() + " already exists and is not empty.");

    if (! folder.createDirectory())
        return juce::Result::fail ("Could not create the folder " + folder.getFullPathName() + ".");

    projectFile = folder.getChildFile (folder.getFileName() + extension);
    ensureSubfolders();
    project.resetToDefault (folder.getFileName(), sampleRate);
    return save (project);
}

juce::Result ProjectFolder::open (const juce::File& fileOrFolder, Project& project)
{
    auto file = fileOrFolder;

    if (file.isDirectory())
    {
        auto direct = file.getChildFile (file.getFileName() + extension);
        if (direct.existsAsFile())
            file = direct;
        else
        {
            auto found = file.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
            if (found.isEmpty())
                return juce::Result::fail ("There is no " + juce::String (extension) + " file in " + file.getFileName() + ".");
            file = found.getFirst();
        }
    }

    if (! file.existsAsFile())
        return juce::Result::fail (file.getFullPathName() + " does not exist.");

    juce::String error;
    if (! project.fromJson (file.loadFileAsString(), error))
        return juce::Result::fail (error);

    projectFile = file;
    ensureSubfolders();
    return juce::Result::ok();
}

void ProjectFolder::writeBackup() const
{
    if (! projectFile.existsAsFile()) return;

    auto backups = getBackupsFolder();
    backups.createDirectory();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S");
    auto target = backups.getChildFile (projectFile.getFileNameWithoutExtension() + " " + stamp + extension);
    for (int n = 2; target.exists(); ++n)
        target = backups.getChildFile (projectFile.getFileNameWithoutExtension() + " " + stamp + "-" + juce::String (n) + extension);
    projectFile.copyFileTo (target);

    // Keep the most recent ones.
    auto all = backups.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (all.begin(), all.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });
    for (int i = backupsKept; i < all.size(); ++i)
        all.getReference (i).deleteFile();
}

juce::Result ProjectFolder::save (Project& project)
{
    if (! isValid())
        return juce::Result::fail ("The project has no folder yet.");

    ensureSubfolders();
    writeBackup();

    juce::TemporaryFile temp (projectFile);
    if (! temp.getFile().replaceWithText (project.toJson()))
        return juce::Result::fail ("Could not write " + projectFile.getFullPathName() + ".");
    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not replace " + projectFile.getFullPathName() + ".");

    return juce::Result::ok();
}

juce::File ProjectFolder::resolve (const juce::String& relativePath) const
{
    if (relativePath.isEmpty()) return {};
    if (juce::File::isAbsolutePath (relativePath)) return juce::File (relativePath);
    return getFolder().getChildFile (relativePath);
}

juce::String ProjectFolder::relativePathOf (const juce::File& f) const
{
    return f.getRelativePathFrom (getFolder()).replaceCharacter ('\\', '/');
}

juce::File ProjectFolder::newSoundFile (const juce::String& name, const juce::String& extensionWithDot) const
{
    auto base = juce::File::createLegalFileName (name).trim();
    if (base.isEmpty()) base = "sound";
    auto f = getSoundsFolder().getChildFile (base + extensionWithDot);
    for (int n = 2; f.exists(); ++n)
        f = getSoundsFolder().getChildFile (base + " " + juce::String (n) + extensionWithDot);
    return f;
}
