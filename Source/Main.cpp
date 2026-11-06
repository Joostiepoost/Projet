// Main.cpp — the application object and its main window (copied from Effet).
// The contents live in MainComponent.

#include <JuceHeader.h>
#include "MainComponent.h"

class ProjetApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        openFiles (commandLine);
    }

    // Files given on the command line or sent by the system (a .projet opens,
    // sound files are imported at the playhead).
    void anotherInstanceStarted (const juce::String& commandLine) override   { openFiles (commandLine); }

    void openFiles (const juce::String& commandLine)
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        juce::StringArray files;
        for (auto a : args)
        {
            a = a.unquoted();
            if (juce::File::isAbsolutePath (a) && juce::File (a).existsAsFile()) files.add (a);
        }
        if (files.isEmpty() || mainWindow == nullptr) return;
        if (auto* mc = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            mc->openFiles (files);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        // Unsaved changes: ask first (Save / Don't save / Cancel).
        if (auto* mc = mainWindow != nullptr ? dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) : nullptr)
        {
            if (quitting) return;
            quitting = true;
            mc->askToSaveThen ([] { juce::JUCEApplication::getInstance()->quit(); });
            juce::MessageManager::callAsync ([this] { quitting = false; });
            return;
        }
        quit();
    }

    bool quitting = false;

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1000, 560, 8000, 5000);
            centreWithSize (1400, 860);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ProjetApplication)
