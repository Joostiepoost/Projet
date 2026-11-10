# Composition framework

**Energy.** Vande Gorne's energy models (*Techniques d'écriture*, chapter 1), in five families (material, global management, gestural, movement, law), each with its variation factors. A box can have a second energy, drawn lighter.

**Semiotic level.** From the source as it is to the source hidden: I-son (icon), Di-son (diagram), metonymic (Thoresen: a part or a neighbour stands for the whole), Mé-son (metaphor). Peirce via Bayle. A folder can give a level to the boxes inside; a box's own level wins. The level can change along a box.

**Role.** Thoresen's layer functions (foreground, middleground, background) or Vande Gorne's figure and ground: F, M (the default) and B. In the Framework menu, *Role: names* chooses which names show. The role can change along a box.

**Dynamic profile.** The box's intensity envelope, with Schaeffer's names (*Traité*, 31.4) and Vande Gorne's where she has one: flat, resonance (attack-resonance), crescendo (reversed sound), decrescendo, delta, hollow (reversed delta), mordent. The energy glyph is drawn inside the profile, so a bounce can live in a delta. A delta or a hollow profile also has a **peak position** (in % of the box: an asymmetric delta) and a **hard or soft peak**.

**Glyphs.** The energies are drawn on the boxes by `glyphs.js`, this package's drawing script (JavaScript, sandboxed: no files or network). Edit it to change the shapes; each value's `glyph` field names the shape it uses.

Edit `framework.json` to rename, translate or add values, then *Framework > Reload frameworks*.
