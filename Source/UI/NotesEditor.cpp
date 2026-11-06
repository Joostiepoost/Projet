// NotesEditor.cpp

#include "NotesEditor.h"
#include "Theme.h"

NotesEditor::NotesEditor (Project& p, bool splitView)
    : project (p), split (splitView)
{
    auto setupTool = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setTooltip (tip);
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    };
    setupTool (boldButton, "Bold: **text**");
    setupTool (italicButton, "Italic: *text*");
    setupTool (headingButton, "Heading: # at the start of the line");
    setupTool (bulletButton, "Bulleted list: - at the start of the line");
    setupTool (linkButton, "Link: [text](https://...) (a bare address works too)");
    setupTool (modeButton, "Switch between editing the notes and the styled preview (links are clickable there)");
    setupTool (expandButton, "Open the notes in a large window (Cmd-E)");

    boldButton.onClick = [this] { wrapSelection ("**", "**", "bold"); };
    italicButton.onClick = [this] { wrapSelection ("*", "*", "italic"); };
    headingButton.onClick = [this] { prefixLines ("# "); };
    bulletButton.onClick = [this] { prefixLines ("- "); };
    linkButton.onClick = [this] { wrapSelection ("[", "](https://)", "text"); };
    modeButton.onClick = [this] { setWriting (! writing); if (writing) focusEditor(); };
    expandButton.onClick = [this] { commit(); if (onExpand) onExpand(); };

    editor.setMultiLine (true, true);
    editor.setReturnKeyStartsNewLine (true);
    editor.setTabKeyUsedAsCharacter (false);
    editor.setFont (Theme::font (13.5f));
    editor.setScrollbarsShown (true);
    editor.setTextToShowWhenEmpty ("Write the idea of this moment. **bold**, *italic*, # heading, - list, links...", Theme::textMuted);
    editor.onTextChange = [this]
    {
        startTimer (700);
        if (split) { view.setMarkdown (editor.getText()); resized(); }
    };
    editor.onFocusLost = [this] { commit(); };
    editor.onEscapeKey = [this] { commit(); if (! split) setWriting (false); };
    addAndMakeVisible (editor);

    view.onDoubleClick = [this] { setWriting (true); focusEditor(); };
    viewport.setViewedComponent (&view, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    expandButton.setVisible (! split);
    modeButton.setVisible (! split);
    setWriting (split);
}

NotesEditor::~NotesEditor()
{
    commit();
}

void NotesEditor::setBox (const juce::ValueTree& b)
{
    if (b == box) { refresh(); return; }
    commit();
    box = b;
    editor.setText (box.isValid() ? box[IDs::notes].toString() : juce::String(), false);
    view.setMarkdown (editor.getText());
    if (! split) setWriting (editor.getText().trim().isEmpty() && box.isValid() && Project::kindOf (box) == "text");
    resized();
}

void NotesEditor::refresh()
{
    if (! box.isValid()) return;
    const auto notes = box[IDs::notes].toString();
    if (! editor.hasKeyboardFocus (true) && ! isTimerRunning() && editor.getText() != notes)
        editor.setText (notes, false);
    view.setMarkdown (editor.getText());
    resized();
}

void NotesEditor::commit()
{
    stopTimer();
    if (! box.isValid() || ! box.getParent().isValid()) return;
    const auto text = editor.getText();
    if (text == box[IDs::notes].toString()) return;
    project.getUndoManager().beginNewTransaction ("Edit notes");
    box.setProperty (IDs::notes, text, project.um());
}

void NotesEditor::setWriting (bool w)
{
    if (! w) commit();
    writing = split || w;
    if (! writing) view.setMarkdown (editor.getText());
    editor.setVisible (writing);
    viewport.setVisible (! writing || split);
    for (auto* b : { &boldButton, &italicButton, &headingButton, &bulletButton, &linkButton })
        b->setEnabled (writing);
    updateModeButtons();
    resized();
}

void NotesEditor::focusEditor()
{
    if (editor.isShowing()) editor.grabKeyboardFocus();
}

void NotesEditor::updateModeButtons()
{
    // The button names the mode it switches to.
    modeButton.setButtonText (writing ? "Preview" : "Edit");
}

void NotesEditor::wrapSelection (const juce::String& before, const juce::String& after, const juce::String& placeholder)
{
    if (! writing) return;
    const auto range = editor.getHighlightedRegion();
    const auto selected = editor.getHighlightedText();
    const auto inner = selected.isNotEmpty() ? selected : placeholder;
    editor.insertTextAtCaret (before + inner + after);

    // Select the placeholder (or, for a link, the address) so it can be typed over.
    const int start = range.getStart() + before.length();
    if (after.startsWith ("]("))
        editor.setHighlightedRegion ({ start + inner.length() + 2, start + inner.length() + after.length() - 1 });
    else if (selected.isEmpty())
        editor.setHighlightedRegion ({ start, start + inner.length() });
    focusEditor();
}

void NotesEditor::prefixLines (const juce::String& prefix)
{
    if (! writing) return;
    auto text = editor.getText();
    auto range = editor.getHighlightedRegion();
    int lineStart = text.substring (0, range.getStart()).lastIndexOfChar ('\n') + 1;
    int lineEnd = text.indexOfChar (range.getEnd(), '\n');
    if (lineEnd < 0) lineEnd = text.length();

    auto lines = juce::StringArray::fromLines (text.substring (lineStart, lineEnd));
    const bool allHave = std::all_of (lines.begin(), lines.end(), [&] (const juce::String& l) { return l.startsWith (prefix); });
    for (auto& l : lines)
        l = allHave ? l.substring (prefix.length()) : (l.startsWith (prefix) ? l : prefix + l);
    const auto replaced = lines.joinIntoString ("\n");

    editor.setHighlightedRegion ({ lineStart, lineEnd });
    editor.insertTextAtCaret (replaced);
    editor.setHighlightedRegion ({ lineStart, lineStart + replaced.length() });
    focusEditor();
}

void NotesEditor::paint (juce::Graphics& g)
{
    if (! writing || split)
    {
        auto r = viewport.getBounds().toFloat().expanded (1.0f);
        g.setColour (Theme::panel2);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
}

void NotesEditor::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (26);
    r.removeFromTop (6);

    for (auto* b : { &boldButton, &italicButton, &headingButton })
    {
        b->setBounds (bar.removeFromLeft (26));
        bar.removeFromLeft (3);
    }
    bulletButton.setBounds (bar.removeFromLeft (40));
    bar.removeFromLeft (3);
    linkButton.setBounds (bar.removeFromLeft (40));

    if (! split)
    {
        expandButton.setBounds (bar.removeFromRight (56));
        bar.removeFromRight (4);
        modeButton.setBounds (bar.removeFromRight (58));
    }

    juce::Rectangle<int> editArea, viewArea;
    if (split)
    {
        editArea = r.removeFromLeft (r.getWidth() / 2 - 6);
        r.removeFromLeft (12);
        viewArea = r;
    }
    else
        editArea = viewArea = r;

    editor.setBounds (editArea);
    viewport.setBounds (viewArea.reduced (6, 6));
    view.setSize (viewport.getMaximumVisibleWidth() > 0 ? viewport.getMaximumVisibleWidth() : viewport.getWidth(), 10);
    view.setSize (view.getWidth(), juce::jmax (viewport.getHeight(), view.getIdealHeight()));
}
