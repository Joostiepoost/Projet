// ScoreExport.cpp

#include "ScoreExport.h"
#include "TimelineView.h"
#include "BoardPainter.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "../Model/Board.h"
#include "../Model/Glyphs.h"

namespace Score
{

namespace
{
    constexpr float margin = 28.0f, headerHeight = 22.0f, systemGap = 16.0f;
    constexpr float screenToPage = 0.72f;           // screen pixels to points: text at about 8 pt

    class Layout
    {
    public:
        Layout (Project& p, projet::SoundPool& sp, const Framework& f, const ProjectFolder& pf, const Options& o)
            : project (p), pool (sp), framework (f), folder (pf), options (o), view (p, sp, f)
        {
            pageWidth = o.a4 ? 841.89f : 1190.55f;
            pageHeight = o.a4 ? 595.28f : 841.89f;
            content = { margin, margin + headerHeight, pageWidth - 2.0f * margin, pageHeight - 2.0f * margin - headerHeight };
            widthPx = juce::roundToInt (content.getWidth() / screenToPage);
            view.setPrinting (true);
            view.setProjectFolder (pf.getFolder());     // images on board boxes
            if (options.paper) map = [] (juce::Colour c) { return Vector::paperColour (c); };
            glyphs.rebuild (framework);
        }

        Result run()
        {
            juce::Array<juce::ValueTree> boardsToPrint;
            printLevel (options.level, options.span, levelName (options.level), boardsToPrint, true);
            if (options.boards)
                for (auto& box : boardsToPrint) printBoard (box);
            finishPages();
            return std::move (result);
        }

    private:
        Project& project;
        projet::SoundPool& pool;
        const Framework& framework;
        const ProjectFolder& folder;
        const Options& options;
        TimelineView view;
        Glyphs::Set glyphs;
        Vector::Recorder::ColourMap map;
        float pageWidth = 0.0f, pageHeight = 0.0f;
        juce::Rectangle<float> content;
        int widthPx = 1000;
        Result result;
        std::vector<juce::String> headings;     // per page
        float y = 0.0f;
        bool pageOpen = false;

        juce::Colour background() const
        {
            return options.paper ? juce::Colours::white : Theme::background;
        }

        juce::String levelName (const juce::ValueTree& level) const
        {
            return level.isValid() ? level[IDs::name].toString() : juce::String ("The piece");
        }

        void newPage (const juce::String& heading)
        {
            Vector::Page page;
            page.width = pageWidth;
            page.height = pageHeight;
            page.background = background();
            result.pages.push_back (std::move (page));
            headings.push_back (heading);
            y = content.getY();
            pageOpen = true;
        }

        /** The level's tracks over `span` (its own time), in systems; then, if
            asked, each container in it opened out. */
        void printLevel (const juce::ValueTree& level, juce::Range<double> span, const juce::String& heading,
                         juce::Array<juce::ValueTree>& boardsToPrint, bool first)
        {
            if (level.isValid()) project.prepareLevel (level);     // its rulers, as when you go inside
            view.setLevel (level);
            {
                const Project::ScopedFocus atLevel (project, level);
                if (span.isEmpty())
                {
                    const auto frame = project.frameFor ("piece");
                    span = { frame.start, frame.end() };
                }
            }
            if (span.getLength() < 1.0e-3) return;
            view.setSize (widthPx, 4000);
            const int heightPx = juce::jmax (40, view.printHeight());
            view.setSize (widthPx, heightPx);

            const float scale = juce::jmin (screenToPage, content.getHeight() / (float) heightPx);
            const float systemHeight = (float) heightPx * scale;
            const double seconds = juce::jmax (0.5, options.secondsPerLine);
            const double pps = (double) (widthPx - TimelineView::headerWidth) / seconds;

            if (! first || ! pageOpen) newPage (heading);
            int number = 0;
            for (double t = span.getStart(); t < span.getEnd() - 1.0e-6; t += seconds)
            {
                if (y + systemHeight > content.getBottom() + 0.5f) newPage (heading);
                const double end = juce::jmin (t + seconds, span.getEnd());
                view.setView (t, pps);
                auto& page = result.pages.back();
                Vector::Recorder recorder (page, map, options.imageDpi);
                recorder.beginGroup (heading + " system " + juce::String (++number) + " " + TimeFormat::format (t, 1) + "-" + TimeFormat::format (end, 1));
                juce::Graphics g (recorder);
                g.addTransform (juce::AffineTransform::scale (scale).translated (content.getX(), y));
                // The last line stops where the span does; its time scale stays the same.
                const float xEnd = (float) TimelineView::headerWidth + (float) ((end - t) * pps);
                g.reduceClipRegion (0, 0, juce::roundToInt (std::ceil (xEnd)) + 1, heightPx);
                view.paint (g);
                y += systemHeight + systemGap;
                ++result.systems;
            }

            // What it holds: containers to open out, boards to print.
            juce::Array<juce::ValueTree> containers;
            {
                const Project::ScopedFocus atLevel (project, level);
                for (auto track : project.getTracks())
                    for (auto box : track)
                    {
                        if (! box.hasProperty (IDs::start)) continue;
                        const juce::Range<double> extent (Project::boxStart (box), Project::boxEnd (box));
                        if (! extent.intersects (span)) continue;
                        if (Project::isContainer (box)) containers.add (box);
                        else if (Project::isBoard (box)) boardsToPrint.add (box);
                    }
            }
            std::stable_sort (containers.begin(), containers.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
                              { return Project::boxStart (a) < Project::boxStart (b); });
            if (! options.openContainers) return;
            for (auto& c : containers)
            {
                ++result.containers;
                juce::String h = heading == "The piece" ? juce::String() : heading + " / ";
                printLevel (c, {}, h + levelName (c), boardsToPrint, false);
            }
        }

        void printBoard (const juce::ValueTree& box)
        {
            auto board = Project::boardOf (box);
            auto bounds = Board::contentBounds (board);
            if (bounds.isEmpty()) return;
            bounds = bounds.expanded (24.0f);
            const auto name = box[IDs::name].toString().trim();
            newPage ("Board: " + (name.isEmpty() ? juce::String ("Board") : name));
            ++result.boards;
            const float scale = juce::jmin (content.getWidth() / bounds.getWidth(), content.getHeight() / bounds.getHeight(), 1.2f);
            auto& page = result.pages.back();
            Vector::Recorder recorder (page, map, options.imageDpi);
            recorder.beginGroup ("board " + name);
            juce::Graphics g (recorder);
            g.addTransform (juce::AffineTransform::translation (-bounds.getX(), -bounds.getY()).scaled (scale).translated (content.getX(), content.getY()));
            BoardPainter::Context ctx;
            ctx.framework = &framework;
            ctx.projectFolder = folder.getFolder();
            ctx.project = &project;
            ctx.pool = &pool;
            ctx.glyphs = &glyphs;
            ctx.boxLength = Project::boxLength (box);
            BoardPainter::paint (g, board, ctx);
        }

        /** Each page's head: the piece, what the page shows, the date and the page number. */
        void finishPages()
        {
            const auto date = juce::Time::getCurrentTime().formatted ("%d %B %Y");
            const int n = (int) result.pages.size();
            for (int i = 0; i < n; ++i)
            {
                auto& page = result.pages[(size_t) i];
                Vector::Recorder recorder (page, map, options.imageDpi);
                recorder.beginGroup ("head");
                juce::Graphics g (recorder);
                auto head = juce::Rectangle<float> (margin, margin - 4.0f, pageWidth - 2.0f * margin, headerHeight - 6.0f);
                g.setColour (Theme::text);
                g.setFont (Theme::font (11.0f, true));
                const auto title = options.title + (headings[(size_t) i] == "The piece" ? juce::String() : "  " + juce::String::fromUTF8 ("\xc2\xb7") + "  " + headings[(size_t) i]);
                g.drawText (title, head, juce::Justification::centredLeft, true);
                g.setFont (Theme::font (9.5f));
                g.setColour (Theme::textMuted);
                g.drawText (date + "   " + juce::String (i + 1) + " / " + juce::String (n), head, juce::Justification::centredRight, true);
                g.setColour (Theme::line);
                g.fillRect (margin, margin + headerHeight - 6.0f, pageWidth - 2.0f * margin, 0.6f);
            }
        }
    };
}

Result render (Project& project, projet::SoundPool& pool, const Framework& framework, const ProjectFolder& folder, const Options& options)
{
    Vector::Recorder::clearImageCache();
    Layout layout (project, pool, framework, folder, options);
    auto result = layout.run();
    return result;
}

} // namespace Score
