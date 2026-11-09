# Example framework

A small package to try what a framework can bring, and a model for your own.

- **Motion** (a choice): rising, falling, static, circling. It can change along a box, and a folder can give it to the boxes inside. Shown as a badge.
- **Materials** (tags): several at once, shown as a label.
- **Density** (a number from 0 to 10): shown as a strip, darker for denser.
- **Source** (a text): where and how the sound was recorded. Not shown on the boxes until you choose a mark (*View > Display*).
- It also **adds a value** to another package: *Pulsation (example)* among the energies of the composition framework, in the family *law*. Updating the composition framework does not remove it.

**To use it:** *Framework > Frameworks...*, then *Add a package folder...* and choose this folder (it goes into the library and this project). Or copy the folder into the library (`~/Library/Application Support/Projet/Frameworks`) and choose *Use in this project*.

**To make your own:** copy this folder, give it another name and another `"id"`, and edit `framework.json`. Kinds: `choice`, `tags`, `number`, `text`. Marks: `strip`, `badge`, `label`, `none`. See the README for every key.
