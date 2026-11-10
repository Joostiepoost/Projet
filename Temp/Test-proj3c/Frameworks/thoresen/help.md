# Thoresen: time-fields

Lasse Thoresen's time-field conjunctions (*Emergent Musical Forms*, 6.3), between the boxes of a track.

**Junction with the box before.** How a box follows the one before it on the same track (the one that starts most recently before it): separate (a noticeable silence), bridged (a gap spanned by a box on another track that is not muted), joint (just after), close (within 20 ms), hinged (an overlap up to 8 s), overlapping (a longer one), superimposed (chosen only). Projet finds it from the timing until you choose one; *From the timing* goes back. The rules and thresholds are in `framework.json` (`derive`): edit them, or add kinds of your own.

**Ending.** How a box ends: vague, open, conclusive, cut, disjointed, deferred, or not marked.

**Signs.** Drawn at the top of the lane, after Thoresen's figures 6.2 and 6.3. Where a box with an ending meets the next box, the two make one sign, the ending's mark on the first box's bracket (his figure 6.4). A junction found from the timing has a dashed frame; a chosen one a solid frame. Each value's `sign` is an SVG path: a junction's in a 60 by 20 box with `endAt` where the first box's bracket ends, an ending's `seamSign` drawn from that point.
