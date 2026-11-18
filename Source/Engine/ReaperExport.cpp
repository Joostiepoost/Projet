// ReaperExport.cpp

#include "ReaperExport.h"
#include "Arrangement.h"
#include "../Model/Board.h"
#include "../Model/Factors.h"

#include <vector>

namespace projet::ReaperExport
{

juce::String quoted (const juce::String& s)
{
    auto t = s.replaceCharacters ("\r\n\t", "   ");
    if (! t.containsChar ('"')) return "\"" + t + "\"";
    if (! t.containsChar ('\'')) return "'" + t + "'";
    if (! t.containsChar ('`')) return "`" + t + "`";
    return "\"" + t.replaceCharacter ('"', '\'') + "\"";
}

int colourFor (int argb)
{
    // macOS: 0xRRGGBB, with the flag that marks a custom colour.
    const auto c = juce::Colour ((juce::uint32) argb);
    return 0x1000000 | (c.getRed() << 16) | (c.getGreen() << 8) | c.getBlue();
}

namespace
{
    juce::String num (double v)
    {
        if (std::abs (v) < 1.0e-12) return "0";
        auto s = juce::String (v, 10);
        if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s;
    }

    juce::String guid()
    {
        return "{" + juce::Uuid().toDashedString().toUpperCase() + "}";
    }

    /** Projet's fade shapes as Reaper's: linear, fast start (equal power), slow start and end (S). */
    int fadeShape (FadeShape s, bool fadeIn)
    {
        switch (s)
        {
            case FadeShape::linear:     return 0;
            case FadeShape::equalPower: return fadeIn ? 1 : 2;
            case FadeShape::sCurve:     return 5;
        }
        return 0;
    }

    /** The same for an envelope point (the segment after it). */
    int envelopeShape (FadeShape s, bool fadeIn)
    {
        switch (s)
        {
            case FadeShape::linear:     return 0;
            case FadeShape::equalPower: return fadeIn ? 3 : 4;
            case FadeShape::sCurve:     return 2;
        }
        return 0;
    }

    FadeShape shapeOf (const juce::ValueTree& box, const juce::Identifier& id)
    {
        return (FadeShape) juce::jlimit (0, numFadeShapes - 1, (int) box.getProperty (id, (int) FadeShape::sCurve));
    }

    float gainOf (const juce::ValueTree& t)
    {
        return (float) juce::Decibels::decibelsToGain ((double) t.getProperty (IDs::gainDb, 0.0), -100.0);
    }

    struct Take
    {
        juce::String name, file, type;
        double offset = 0.0;
        float gain = 1.0f;
    };

    struct Item
    {
        double position = 0.0, length = 0.0;
        std::vector<Take> takes;            // none: an empty item
        int activeTake = 0;
        double fadeIn = 0.0, fadeOut = 0.0;
        int inShape = 0, outShape = 0;
        juce::String notes;
    };

    struct Point { double time, value; int shape; };

    struct Track
    {
        juce::String name;
        int colour = 0;                     // ARGB, 0: none (Reaper's own)
        bool mute = false, solo = false;
        double volume = 1.0;
        int depth = 0;
        std::vector<Item> items;
        std::vector<Point> envelope;
    };

    struct Region { double start, end; juce::String name; };

    /** Where a timeline lies on the piece's (piece seconds = a + b t), and
        the containers' extents around it. */
    struct Level
    {
        double a = 0.0, b = 1.0;
        std::vector<juce::Range<double>> windows;
    };

    class Writer
    {
    public:
        Writer (Project& p, const Framework& f, const ProjectFolder& pf, const Options& o, Result& r)
            : project (p), framework (f), folder (pf), options (o), result (r) {}

        void run()
        {
            const Project::ScopedFocus atPiece (project, {});
            pieceSpan = options.span.isEmpty() ? juce::Range<double> (0.0, 1.0e9) : options.span;
            shift = options.span.isEmpty() ? 0.0 : options.span.getStart();
            addTimeline (project.getRootTracks(), Level(), 0, true);
            if (options.markers) collectMarkers();
            emit();
        }

    private:
        Project& project;
        const Framework& framework;
        const ProjectFolder& folder;
        const Options& options;
        Result& result;
        juce::Range<double> pieceSpan;
        double shift = 0.0;
        std::vector<Track> tracks;
        std::vector<Region> regions;
        std::vector<std::pair<double, juce::String>> markers;

        //== Gathering =========================================================

        /** What of [start, end) sounds inside the windows and the exported span. */
        juce::Range<double> visible (double start, double end, const Level& level) const
        {
            juce::Range<double> r (start, end);
            for (auto& w : level.windows) r = r.getIntersectionWith (w);
            return r.getIntersectionWith (pieceSpan);
        }

        static int localDepth (const juce::ValueTree& track)
        {
            auto list = track.getParent();
            int depth = 0;
            auto id = track[IDs::folderId].toString();
            while (id.isNotEmpty() && depth < 64)
            {
                auto f = list.getChildWithProperty (IDs::id, id);
                if (! f.isValid()) break;
                ++depth;
                id = f[IDs::folderId].toString();
            }
            return depth;
        }

        juce::String valueText (const Framework::Category& c, const juce::String& value) const
        {
            switch (c.kind)
            {
                case Framework::Kind::choice: return framework.valueName (c, value);
                case Framework::Kind::tags:
                {
                    juce::StringArray names;
                    for (auto& t : Tags::parse (value)) names.add (framework.valueName (c, t));
                    return names.joinIntoString (", ");
                }
                case Framework::Kind::number: return framework.formatNumber (c, value.getDoubleValue());
                case Framework::Kind::text:   return value;
            }
            return value;
        }

        /** The box's name, its framework values (with the curves drawn) and its notes. */
        juce::String notesFor (const juce::ValueTree& box, const juce::String& extra = {})
        {
            juce::StringArray lines;
            const auto name = box[IDs::name].toString().trim();
            if (name.isNotEmpty()) lines.add (name);
            for (auto& c : framework.getCategories())
            {
                const auto v = project.frameworkValue (box, c);
                if (v.value.isEmpty() || ! (v.isOwn() || v.fromFolder())) continue;
                juce::String line = framework.text (c.name) + ": " + valueText (c, v.value);
                // The variation factors drawn for it (stage 8).
                const auto curves = Factors::parse (box[c.factorsProperty()].toString());
                juce::StringArray drawn;
                if (c.kind == Framework::Kind::choice)
                    for (auto* f : framework.factorsOf (c, v.value))
                        if (auto it = curves.find (f->id); it != curves.end() && ! it->second.isEmpty())
                            drawn.add (framework.text (f->name));
                if (! drawn.isEmpty()) line << " (curves: " << drawn.joinIntoString (", ") << ")";
                lines.add (line);
            }
            if (extra.isNotEmpty()) lines.add (extra);
            const auto notes = box[IDs::notes].toString().trim();
            if (notes.isNotEmpty())
            {
                if (! lines.isEmpty()) lines.add ({});
                lines.addLines (notes);
            }
            return lines.joinIntoString ("\n");
        }

        bool takeFrom (const juce::ValueTree& content, Take& take)
        {
            auto sound = project.findSound (content[IDs::soundId].toString());
            if (! sound.isValid()) return false;
            const auto file = folder.resolve (sound[IDs::file].toString());
            if (! file.existsAsFile()) result.missing.addIfNotAlreadyThere (file.getFullPathName());
            const auto dir = options.rppFile.getParentDirectory();
            take.file = options.rppFile.getFullPathName().isNotEmpty() ? file.getRelativePathFrom (dir) : file.getFullPathName();
            const auto ext = file.getFileExtension().toLowerCase();
            take.type = ext == ".ogg" || ext == ".oga" ? "VORBIS" : (ext == ".flac" ? "FLAC" : (ext == ".mp3" ? "MP3" : "WAVE"));
            take.offset = (double) content[IDs::offset];
            take.gain = gainOf (content);
            // The sound's name; a box of one take is named as the box (below).
            take.name = sound[IDs::name].toString().trim();
            if (take.name.isEmpty()) take.name = file.getFileNameWithoutExtension();
            return true;
        }

        void addSound (const juce::ValueTree& box, const Level& level, int trackIndex)
        {
            const double start = level.a + level.b * Project::boxStart (box);
            const double length = (double) box[IDs::length];        // real seconds, at any depth
            const auto shown = visible (start, start + length, level);
            if (shown.getLength() < 1.0e-4) return;

            Item item;
            // Its takes: the sound ones, the active one active.
            const auto takes = Project::takesOf (box);
            if (takes.isEmpty())
            {
                Take t;
                if (! takeFrom (box, t)) return;
                if (box[IDs::name].toString().trim().isNotEmpty()) t.name = box[IDs::name].toString().trim();
                item.takes.push_back (t);
            }
            else
            {
                const auto active = Project::activeTakeOf (box);
                for (auto& take : takes)
                {
                    const bool isActive = take == active;
                    if (Project::takeKind (box, take) != "sound") continue;
                    Take t;
                    if (! takeFrom (isActive ? box : Project::takeView (box, take), t)) continue;
                    if (take[IDs::name].toString().isNotEmpty())
                        t.name = take[IDs::name].toString() + ": " + t.name;     // B: the sound
                    if (isActive) item.activeTake = (int) item.takes.size();
                    item.takes.push_back (t);
                }
                if (item.takes.empty()) return;
            }
            const double cutStart = shown.getStart() - start, cutEnd = (start + length) - shown.getEnd();
            for (auto& t : item.takes) t.offset += cutStart;
            item.position = shown.getStart() - shift;
            item.length = shown.getLength();
            // Its fades; where a container's edge (or the span's) cuts it, a 5 ms fade, as in playback.
            item.fadeIn = cutStart > 1.0e-6 ? Arrangement::cutFade : juce::jmin ((double) box[IDs::fadeIn], item.length);
            item.inShape = cutStart > 1.0e-6 ? 0 : fadeShape (shapeOf (box, IDs::fadeInShape), true);
            item.fadeOut = cutEnd > 1.0e-6 ? Arrangement::cutFade : juce::jmin ((double) box[IDs::fadeOut], item.length);
            item.outShape = cutEnd > 1.0e-6 ? 0 : fadeShape (shapeOf (box, IDs::fadeOutShape), false);
            item.notes = notesFor (box);
            // Only the name, and it is the item's: nothing else to say.
            if (item.takes.size() == 1 && item.notes == box[IDs::name].toString().trim()) item.notes.clear();
            tracks[(size_t) trackIndex].items.push_back (std::move (item));
            ++result.items;
        }

        void addEmpty (double start, double end, const Level& level, const juce::String& notes, int trackIndex)
        {
            const auto shown = visible (start, end, level);
            if (shown.getLength() < 1.0e-4) return;
            Item item;
            item.position = shown.getStart() - shift;
            item.length = shown.getLength();
            item.notes = notes;
            tracks[(size_t) trackIndex].items.push_back (std::move (item));
            ++result.emptyItems;
        }

        void addBoard (const juce::ValueTree& box, const Level& level, int trackIndex, int depth)
        {
            const double start = level.a + level.b * Project::boxStart (box);
            const double end = start + level.b * Project::boxLength (box);
            auto board = Project::boardOf (box);
            // What the board holds, and its cards' words.
            juce::StringArray cards;
            for (auto item : board)
                if (item.hasType (IDs::CARD))
                {
                    auto text = item[IDs::notes].toString().trim();
                    const auto label = item[IDs::label].toString().trim();
                    if (label.isNotEmpty()) text = label + ": " + text;
                    if (text.isNotEmpty()) cards.add ("- " + text.replace ("\n", " "));
                }
            juce::String extra = "Board: " + Board::summary (board);
            if (! cards.isEmpty()) extra << "\n" << cards.joinIntoString ("\n");
            addEmpty (start, end, level, notesFor (box, extra), trackIndex);

            // Its temporal outs, on a track of their own under it.
            const auto outs = Board::temporalOuts (board);
            if (outs.isEmpty()) return;
            Track t;
            t.name = (box[IDs::name].toString().isEmpty() ? juce::String ("Board") : box[IDs::name].toString()) + ": temporal outs";
            t.colour = tracks[(size_t) trackIndex].colour;
            t.depth = depth + 1;
            tracks.push_back (t);
            const int outsIndex = (int) tracks.size() - 1;
            ++result.tracks;
            Level inBox = level;
            inBox.windows.push_back ({ start, end });       // cut off at the box's end
            for (auto card : outs)
            {
                const double s = start + level.b * juce::jmax (0.0, (double) card.getProperty (IDs::outStart, 0.0));
                const double in = Board::cardIn (card);
                const double to = Board::cardOut (card, project.soundSeconds (card[IDs::soundId].toString()));
                if (to <= in) continue;
                const auto shown = visible (s, s + (to - in), inBox);
                if (shown.getLength() < 1.0e-4) continue;
                Take take;
                if (! takeFrom (card, take)) continue;
                const double cutStart = shown.getStart() - s, cutEnd = (s + to - in) - shown.getEnd();
                take.offset = in + cutStart;
                take.name = cardName (card, take.name);
                Item item;
                item.takes.push_back (take);
                item.position = shown.getStart() - shift;
                item.length = shown.getLength();
                item.fadeIn = cutStart > 1.0e-6 ? Arrangement::cutFade : juce::jmin ((double) card.getProperty (IDs::fadeIn, 0.0), item.length);
                item.fadeOut = cutEnd > 1.0e-6 ? Arrangement::cutFade : juce::jmin ((double) card.getProperty (IDs::fadeOut, 0.0), item.length);
                tracks[(size_t) outsIndex].items.push_back (std::move (item));
                ++result.items;
            }
        }

        static juce::String cardName (const juce::ValueTree& card, const juce::String& soundName)
        {
            const auto own = card[IDs::label].toString().trim();
            return own.isNotEmpty() ? own : soundName;
        }

        void addContainer (const juce::ValueTree& box, const Level& level, int trackIndex, int depth, bool atPiece)
        {
            const double start = level.a + level.b * Project::boxStart (box);
            const double length = level.b * Project::boxLength (box);
            auto name = box[IDs::name].toString().trim();
            if (name.isEmpty()) name = "Container";

            Track t;
            t.name = name;
            t.colour = tracks[(size_t) trackIndex].colour;
            t.depth = depth + 1;
            t.volume = gainOf (box);
            // Its fades: the folder's volume envelope (Reaper sums the folder, as Projet plays it).
            const double fi = level.b * (double) box.getProperty (IDs::fadeIn, 0.0);
            const double fo = level.b * (double) box.getProperty (IDs::fadeOut, 0.0);
            if (fi > 1.0e-6 || fo > 1.0e-6)
            {
                const double s = start - shift, e = start + length - shift;
                if (fi > 1.0e-6) { t.envelope.push_back ({ s, 0.0, envelopeShape (shapeOf (box, IDs::fadeInShape), true) }); t.envelope.push_back ({ s + fi, 1.0, 0 }); }
                else t.envelope.push_back ({ s, 1.0, 0 });
                if (fo > 1.0e-6) { t.envelope.push_back ({ e - fo, 1.0, envelopeShape (shapeOf (box, IDs::fadeOutShape), false) }); t.envelope.push_back ({ e, 0.0, 0 }); }
            }
            tracks.push_back (t);
            const int index = (int) tracks.size() - 1;
            ++result.tracks;
            addEmpty (start, start + length, level, notesFor (box), index);

            if (atPiece && options.regions)
            {
                const auto r = juce::Range<double> (start, start + length).getIntersectionWith (pieceSpan);
                if (r.getLength() > 1.0e-4) regions.push_back ({ r.getStart() - shift, r.getEnd() - shift, name });
            }

            Level inner = level;
            inner.b = level.b * Project::containerScale (box);
            inner.a = start - Project::innerOffset (box) * inner.b;
            inner.windows.push_back ({ start, start + length });
            addTimeline (Project::innerTracks (box), inner, depth + 2, false);
        }

        void addTimeline (const juce::ValueTree& list, const Level& level, int baseDepth, bool atPiece)
        {
            for (auto track : list)
            {
                Track t;
                t.name = track[IDs::name].toString();
                // A folder gives its colour to what is inside.
                if (options.trackColour) t.colour = (int) options.trackColour (project.effectiveColour (track)).getARGB();
                t.mute = (bool) track[IDs::mute];
                t.solo = (bool) track[IDs::solo];
                t.depth = baseDepth + localDepth (track);
                tracks.push_back (t);
                const int index = (int) tracks.size() - 1;
                ++result.tracks;
                if (Project::isFolder (track)) continue;

                juce::Array<juce::ValueTree> boxes;
                for (auto box : track) if (box.hasProperty (IDs::start)) boxes.add (box);
                std::stable_sort (boxes.begin(), boxes.end(), [] (const juce::ValueTree& x, const juce::ValueTree& y)
                                  { return Project::boxStart (x) < Project::boxStart (y); });
                for (auto& box : boxes)
                {
                    if (Project::isContainer (box)) addContainer (box, level, index, t.depth, atPiece);
                    else if (Project::isBoard (box)) addBoard (box, level, index, t.depth);
                    else if (Project::isSound (box)) addSound (box, level, index);
                    else
                    {
                        const double start = level.a + level.b * Project::boxStart (box);
                        addEmpty (start, start + level.b * Project::boxLength (box), level, notesFor (box), index);
                    }
                }
            }
        }

        void collectMarkers()
        {
            for (auto mark : project.getMarks())
            {
                const double t = project.markTime (mark);
                if (t >= 0.0 && pieceSpan.contains (t)) markers.push_back ({ t - shift, mark[IDs::name].toString() });
            }
            for (auto node : project.getNodes())
            {
                const double t = (double) node[IDs::start];
                auto f = project.findTrack (node[IDs::folderId].toString());
                if (pieceSpan.contains (t)) markers.push_back ({ t - shift, "Node" + (f.isValid() ? ": " + f[IDs::name].toString() : juce::String()) });
            }
            std::sort (markers.begin(), markers.end(), [] (const auto& x, const auto& y) { return x.first < y.first; });
        }

        //== Writing ===========================================================

        juce::String out;
        void line (int indent, const juce::String& s)       { out << juce::String::repeatedString ("  ", indent) << s << "\n"; }

        void notesBlock (int indent, const juce::String& head, const juce::String& text)
        {
            line (indent, head);
            for (auto& l : juce::StringArray::fromLines (text)) line (indent + 1, "|" + l);
            line (indent, ">");
        }

        void source (int indent, const Take& t)
        {
            line (indent, "<SOURCE " + t.type);
            line (indent + 1, "FILE " + quoted (t.file));
            line (indent, ">");
        }

        void item (const Item& it, int& iid)
        {
            line (2, "<ITEM");
            line (3, "POSITION " + num (it.position));
            line (3, "SNAPOFFS 0");
            line (3, "LENGTH " + num (it.length));
            line (3, "LOOP 0");
            line (3, "ALLTAKES 0");
            line (3, "FADEIN " + juce::String (it.inShape) + " " + num (it.fadeIn) + " 0 " + juce::String (it.inShape) + " 0 0 0");
            line (3, "FADEOUT " + juce::String (it.outShape) + " " + num (it.fadeOut) + " 0 " + juce::String (it.outShape) + " 0 0 0");
            line (3, "MUTE 0 0");
            line (3, "SEL 0");
            line (3, "IGUID " + guid());
            line (3, "IID " + juce::String (++iid));
            if (it.notes.isNotEmpty()) notesBlock (3, "<NOTES", it.notes);
            if (it.takes.empty())
            {
                line (3, "IMGRESOURCEFLAGS 0");
                line (2, ">");
                return;
            }
            for (size_t i = 0; i < it.takes.size(); ++i)
            {
                const auto& t = it.takes[i];
                if (i > 0) line (3, (int) i == it.activeTake ? "TAKE SEL" : "TAKE");
                line (3, "NAME " + quoted (t.name));
                if (i == 0) line (3, "VOLPAN 1 0 " + num (t.gain) + " -1");
                else        line (3, "TAKEVOLPAN 0 " + num (t.gain) + " -1");
                line (3, "SOFFS " + num (t.offset));
                line (3, "PLAYRATE 1 1 0 -1 0 0.0025");
                line (3, "CHANMODE 0");
                line (3, "GUID " + guid());
                source (3, t);
            }
            line (2, ">");
        }

        void emit()
        {
            const auto stamp = options.timeStamp != 0 ? options.timeStamp : juce::Time::currentTimeMillis() / 1000;
            line (0, "<REAPER_PROJECT 0.1 \"7.0/macOS-arm64\" " + juce::String (stamp));
            line (1, "RIPPLE 0");
            line (1, "AUTOXFADE 0");
            line (1, "PEAKGAIN 1");
            line (1, "CURSOR 0");
            line (1, "SAMPLERATE " + juce::String (juce::roundToInt (project.getSampleRate())) + " 0 0");

            // The loop: Reaper's loop and time selection.
            juce::Range<double> loop;
            bool looping = false;
            if (! options.span.isEmpty())
            {
                loop = { 0.0, options.span.getLength() };
                looping = project.isLoopOn();
            }
            else if (project.hasLoop())
            {
                loop = project.getLoop();
                looping = project.isLoopOn();
            }
            line (1, juce::String ("LOOP ") + (looping ? "1" : "0"));
            line (1, "SELECTION " + num (loop.getStart()) + " " + num (loop.getEnd()));
            line (1, "SELECTION2 " + num (loop.getStart()) + " " + num (loop.getEnd()));

            juce::String notes = "Exported from Projet: " + folder.getProjectFile().getFileNameWithoutExtension()
                                 + ", " + juce::Time::getCurrentTime().formatted ("%d %B %Y %H:%M") + ".";
            if (! options.span.isEmpty())
                notes << "\nThe span from " << num (options.span.getStart()) << " s to " << num (options.span.getEnd()) << " s, moved to 0:00.";
            notes << "\nContainers are folder tracks: their gain is the folder's volume, their fades its volume envelope."
                  << "\nNot carried over: glyphs, factor curves, boards (their text is in the items' notes), relations.";
            const auto pieceNotes = project.getState()[IDs::notes].toString().trim();
            if (pieceNotes.isNotEmpty()) notes << "\n\n" << pieceNotes;
            notesBlock (1, "<NOTES 0 2", notes);

            int index = 0;
            for (auto& r : regions)
            {
                ++index;
                line (1, "MARKER " + juce::String (index) + " " + num (r.start) + " " + quoted (r.name) + " 1 0 1 R");
                line (1, "MARKER " + juce::String (index) + " " + num (r.end) + " \"\" 1");
            }
            for (auto& m : markers)
                line (1, "MARKER " + juce::String (++index) + " " + num (m.first) + " " + quoted (m.second) + " 0 0 1");
            result.regions = (int) regions.size();
            result.markers = (int) markers.size();

            int iid = 0;
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                const auto& t = tracks[i];
                const int next = i + 1 < tracks.size() ? tracks[i + 1].depth : 0;
                juce::String bus = "0 0";
                if (next > t.depth) bus = "1 1";
                else if (next < t.depth) bus = "2 " + juce::String (next - t.depth);
                line (1, "<TRACK " + guid());
                line (2, "NAME " + quoted (t.name));
                line (2, "PEAKCOL " + juce::String (t.colour != 0 ? colourFor (t.colour) : 16576));
                line (2, "BEAT -1");
                line (2, "AUTOMODE 0");
                line (2, "VOLPAN " + num (t.volume) + " 0 -1 -1 1");
                line (2, juce::String ("MUTESOLO ") + (t.mute ? "1" : "0") + (t.solo ? " 2" : " 0") + " 0");
                line (2, "IPHASE 0");
                line (2, "ISBUS " + bus);
                line (2, "BUSCOMP 0 0 0 0 0");
                line (2, "SEL 0");
                line (2, "NCHAN 2");
                line (2, "TRACKID " + guid());
                line (2, "MAINSEND 1 0");
                if (! t.envelope.empty())
                {
                    line (2, "<VOLENV2");
                    line (3, "EGUID " + guid());
                    line (3, "ACT 1 -1");
                    line (3, "VIS 1 1 1");
                    line (3, "LANEHEIGHT 0 0");
                    line (3, "ARM 0");
                    line (3, "DEFSHAPE 0 -1 -1");
                    for (auto& p : t.envelope)
                        line (3, "PT " + num (juce::jmax (0.0, p.time)) + " " + num (p.value) + " " + juce::String (p.shape));
                    line (2, ">");
                }
                for (auto& it : t.items) item (it, iid);
                line (1, ">");
            }
            line (0, ">");
            result.text = out;
        }
    };
}

Result write (Project& project, const Framework& framework, const ProjectFolder& folder, const Options& options)
{
    Result result;
    Writer (project, framework, folder, options, result).run();
    return result;
}

} // namespace projet::ReaperExport
