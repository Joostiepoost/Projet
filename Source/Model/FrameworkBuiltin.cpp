// FrameworkBuiltin.cpp — the framework packages Projet ships (stage 4b).
//
// composition: your framework (Vande Gorne's energies, the semiotic levels,
// Thoresen's roles or Vande Gorne's figure and ground); schaeffer: his
// typology; rk1: the operations of your structuring language (for stage 8).
// They are written to the library the first time Projet runs, and copied into
// new projects. Generated from the 4a framework file (FrameworkLegacy.cpp)
// plus what 4b added (the profile category; the glyph marks). The composition
// package also ships a glyphs.js: Glyphs::builtinSource() (GlyphsBuiltin.cpp).

#include "Framework.h"
#include "Glyphs.h"

namespace
{
    juce::String utf8 (const char* s)     { return juce::String::fromUTF8 (s); }

    Framework::Builtin make (const char* id, const char* json, const char* help, juce::String glyphs = {})
    {
        Framework::Builtin b;
        b.id = id;
        b.json = utf8 (json);
        b.help = utf8 (help);
        b.glyphs = std::move (glyphs);
        b.version = (int) juce::JSON::parse (b.json).getProperty ("version", 1);
        return b;
    }
}

const juce::Array<Framework::Builtin>& Framework::builtins()
{
    static const juce::Array<Builtin> list
    {
        make ("composition",
R"PKG({
  "projet": "framework package",
  "id": "composition",
  "version": 5,
  "order": 10,
  "name": {"fr": "Cadre de composition", "en": "Composition framework"},
  "about": {
    "fr": "Les modèles d'énergie (Vande Gorne), les niveaux sémiotiques (Peirce par Bayle, avec le signe métonymique de Thoresen) et les rôles (fonctions de couche de Thoresen, ou figure et fond de Vande Gorne).",
    "en": "The energy models (Vande Gorne), the semiotic levels (Peirce via Bayle, with Thoresen's metonymic sign) and the roles (Thoresen's layer functions, or Vande Gorne's figure and ground)."
  },
  "categories": [
    {
      "id": "energy",
      "name": {"fr": "Énergie", "en": "Energy"},
      "about": {
        "fr": "le modèle d'énergie du son (Vande Gorne, Techniques d'écriture, ch. 1)",
        "en": "the energy model of the sound (Vande Gorne, Techniques d'écriture, ch. 1)"
      },
      "kind": "choice",
      "control": "menu",
      "mark": "glyph",
      "groups": {
        "name": {"fr": "famille", "en": "family"},
        "list": [
          {
            "id": "materielle",
            "name": {"fr": "matérielle", "en": "material"},
            "colour": "#c49a4a",
            "dark": "#3a3220",
            "light": "#e3c58a"
          },
          {
            "id": "globale",
            "name": {"fr": "gestion globale", "en": "global management"},
            "colour": "#4ea597",
            "dark": "#1f403b",
            "light": "#8fd6c9"
          },
          {
            "id": "gestuelle",
            "name": {"fr": "gestuelle", "en": "gestural"},
            "colour": "#b9707e",
            "dark": "#3d2529",
            "light": "#e3a9b4"
          },
          {
            "id": "mouvement",
            "name": {"fr": "mouvement", "en": "movement"},
            "colour": "#5d82bd",
            "dark": "#2a3a55",
            "light": "#a9c3ec"
          },
          {
            "id": "loi",
            "name": {"fr": "loi", "en": "law"},
            "colour": "#8f7cc6",
            "dark": "#2e2a45",
            "light": "#c4b8ef"
          }
        ]
      },
      "details": {"fr": "Facteurs de variation", "en": "Variation factors"},
      "values": [
        {
          "id": "percussion-resonance",
          "name": {"fr": "Percussion-résonance", "en": "Percussion-resonance"},
          "glyph": "percussion-resonance",
          "groups": ["materielle"],
          "details": {
            "fr": ["dynamique", "halo harmonique", "attaque", "entretien"],
            "en": ["dynamics", "harmonic halo", "attack", "sustainment"]
          }
        },
        {
          "id": "frottement-granulation",
          "name": {"fr": "Frottement-granulation", "en": "Friction-granulation"},
          "glyph": "frottement-granulation",
          "groups": ["materielle", "gestuelle"],
          "details": {
            "fr": ["vitesse", "grain", "entretien itératif"],
            "en": ["speed", "grain", "iterative sustainment"]
          }
        },
        {
          "id": "accumulation-corpuscules",
          "name": {"fr": "Accumulation de corpuscules", "en": "Accumulation of corpuscles"},
          "glyph": "accumulation-corpuscules",
          "groups": ["globale"],
          "details": {"fr": ["grain", "masse", "calibre", "densité"], "en": ["grain", "mass", "caliber", "density"]}
        },
        {
          "id": "oscillation",
          "name": {"fr": "Oscillation", "en": "Oscillation"},
          "glyph": "oscillation",
          "groups": ["loi"],
          "details": {
            "fr": [
              "force de déclenchement",
              "vitesse",
              "densité",
              "fréquence",
              "spectre",
              "espace G-D",
              "profondeur"
            ],
            "en": ["triggering force", "speed", "density", "frequency", "spectrum", "left-right space", "depth"]
          }
        },
        {
          "id": "balancement",
          "name": {"fr": "Balancement", "en": "Swaying"},
          "glyph": "balancement",
          "groups": ["gestuelle"],
          "details": {
            "fr": ["vitesse", "densité", "fréquence", "spectre", "espace G-D", "profondeur"],
            "en": ["speed", "density", "frequency", "spectrum", "left-right space", "depth"]
          }
        },
        {
          "id": "rebond",
          "name": {"fr": "Rebond", "en": "Bounce"},
          "glyph": "rebond",
          "groups": ["gestuelle", "loi"],
          "details": {"fr": ["vitesse", "halo harmonique", "matière"], "en": ["speed", "harmonic halo", "matter"]}
        },
        {
          "id": "flux",
          "name": {"fr": "Flux", "en": "Flow"},
          "glyph": "flux",
          "groups": ["globale", "mouvement"],
          "details": {"fr": ["densité", "profil de masse"], "en": ["density", "mass profile"]}
        },
        {
          "id": "pression-deformation",
          "name": {"fr": "Pression-déformation", "en": "Pressure-deformation"},
          "glyph": "pression-deformation",
          "groups": ["mouvement"],
          "details": {
            "fr": ["vitesse", "profil de masse ou mélodique", "enveloppe spectrale"],
            "en": ["speed", "mass or melodic profile", "spectral envelope"]
          }
        },
        {
          "id": "rotation",
          "name": {"fr": "Rotation", "en": "Rotation"},
          "glyph": "rotation",
          "groups": ["mouvement"],
          "details": {"fr": ["vitesse", "fréquence", "espace"], "en": ["speed", "frequency", "space"]}
        },
        {
          "id": "spirale",
          "name": {"fr": "Spirale", "en": "Spiral"},
          "glyph": "spirale",
          "groups": ["mouvement"],
          "details": {
            "fr": ["vitesse", "fréquence", "espace", "direction de l'accélération"],
            "en": ["speed", "frequency", "space", "direction of acceleration"]
          }
        }
      ]
    },
    {
      "id": "energy2",
      "name": {"fr": "Seconde énergie", "en": "Second energy"},
      "about": {"fr": "une deuxième énergie, dessinée plus claire", "en": "a second energy, drawn lighter"},
      "kind": "choice",
      "control": "menu",
      "mark": "glyph",
      "valuesFrom": "energy"
    },
    {
      "id": "level",
      "name": {"fr": "Niveau sémiotique", "en": "Semiotic level"},
      "about": {
        "fr": "de la source telle quelle à la source cachée (Peirce par Bayle, plus le signe métonymique de Thoresen)",
        "en": "from the source as it is to the source hidden (Peirce via Bayle, plus Thoresen's metonymic sign)"
      },
      "kind": "choice",
      "control": "buttons",
      "mark": "strip",
      "along": true,
      "folders": true,
      "values": [
        {
          "id": "ison",
          "order": 1,
          "fill": "solid",
          "colour": "#4c8fd6",
          "name": {"fr": "I-son", "en": "I-son"},
          "about": {"fr": "icône : la source telle quelle", "en": "icon: the source as it is"}
        },
        {
          "id": "dison",
          "order": 2,
          "fill": "hatched",
          "colour": "#49b3a4",
          "name": {"fr": "Di-son", "en": "Di-son"},
          "about": {"fr": "diagramme : la source comme modèle", "en": "diagram: the source as a model"}
        },
        {
          "id": "metonymic",
          "order": 3,
          "fill": "dotted",
          "colour": "#c0a050",
          "name": {"fr": "Métonymique", "en": "Metonymic"},
          "about": {
            "fr": "une partie ou un voisin pour le tout (Thoresen)",
            "en": "a part or a neighbour stands for the whole (Thoresen)"
          }
        },
        {
          "id": "meson",
          "order": 4,
          "fill": "outline",
          "colour": "#c27ab8",
          "name": {"fr": "Mé-son", "en": "Mé-son"},
          "about": {
            "fr": "métaphore : la source cachée ou arbitraire",
            "en": "metaphor: the source hidden or arbitrary"
          }
        }
      ]
    },
    {
      "id": "role",
      "name": {"fr": "Rôle", "en": "Role"},
      "about": {
        "fr": "fonction de couche (Thoresen) ou figure et fond (Vande Gorne)",
        "en": "layer function (Thoresen) or figure and ground (Vande Gorne)"
      },
      "kind": "choice",
      "control": "buttons",
      "mark": "badge",
      "along": true,
      "default": "M",
      "showDefault": false,
      "names": {"first": {"fr": "Thoresen", "en": "Thoresen"}, "second": {"fr": "Vande Gorne", "en": "Vande Gorne"}},
      "details": {"fr": "Traits (Vande Gorne)", "en": "Traits (Vande Gorne)"},
      "values": [
        {
          "id": "F",
          "label": "F",
          "weight": 3,
          "name": {"fr": "premier plan", "en": "foreground"},
          "name2": {"fr": "figure", "en": "figure"},
          "details": {
            "fr": [
              "discontinu",
              "morphologique",
              "proche",
              "mobile",
              "varié",
              "mince",
              "fort",
              "aigu",
              "grain",
              "net",
              "intermittent"
            ],
            "en": [
              "discontinuous",
              "morphological",
              "near",
              "mobile",
              "varied",
              "thin",
              "loud",
              "high",
              "grainy",
              "sharp",
              "intermittent"
            ]
          }
        },
        {
          "id": "M",
          "label": "M",
          "weight": 2,
          "name": {"fr": "plan moyen", "en": "middleground"},
          "name2": {"fr": "entre les deux", "en": "in between"}
        },
        {
          "id": "B",
          "label": "B",
          "weight": 1,
          "name": {"fr": "arrière-plan", "en": "background"},
          "name2": {"fr": "fond", "en": "ground"},
          "details": {
            "fr": [
              "continu",
              "harmonique",
              "loin",
              "immobile",
              "répétitif",
              "épais",
              "doux",
              "grave",
              "lisse",
              "flou",
              "récurrent"
            ],
            "en": [
              "continuous",
              "harmonic",
              "far",
              "still",
              "repetitive",
              "thick",
              "soft",
              "low",
              "smooth",
              "blurred",
              "recurrent"
            ]
          }
        }
      ]
    },
    {
      "id": "profile",
      "name": {"fr": "Profil dynamique", "en": "Dynamic profile"},
      "about": {
        "fr": "l'enveloppe d'intensité de la boîte : le glyphe d'énergie est dessiné dedans (deltas de Vande Gorne ; noms de Schaeffer, Traité 31.4)",
        "en": "the box's intensity envelope: the energy glyph is drawn inside it (Vande Gorne's deltas; Schaeffer's names, Treatise 31.4)"
      },
      "kind": "choice",
      "control": "buttons",
      "mark": "badge",
      "default": "flat",
      "showDefault": false,
      "names": {"first": {"fr": "Schaeffer", "en": "Schaeffer"}, "second": {"fr": "Vande Gorne", "en": "Vande Gorne"}},
      "values": [
        {"id": "flat", "label": "=", "name": {"fr": "plat", "en": "flat"}, "env": [[0, 1], [1, 1]]},
        {
          "id": "resonance",
          "label": "|>",
          "name": {"fr": "résonance", "en": "resonance"},
          "name2": {"fr": "attaque-résonance", "en": "attack-resonance"},
          "env": [[0, 1], [0.08, 0.55], [0.35, 0.22], [1, 0.06]]
        },
        {
          "id": "crescendo",
          "label": "<",
          "name": {"fr": "crescendo", "en": "crescendo"},
          "name2": {"fr": "son renversé", "en": "reversed sound"},
          "env": [[0, 0.06], [1, 1]]
        },
        {
          "id": "decrescendo",
          "label": ">",
          "name": {"fr": "décrescendo", "en": "decrescendo"},
          "env": [[0, 1], [1, 0.08]]
        },
        {
          "id": "delta",
          "label": "<>",
          "name": {"fr": "delta", "en": "delta"},
          "env": [[0, 0.06], [0.5, 1], [1, 0.06]],
          "peak": 1
        },
        {
          "id": "hollow",
          "label": "><",
          "name": {"fr": "en creux", "en": "hollow"},
          "name2": {"fr": "delta inversé", "en": "reversed delta"},
          "env": [[0, 1], [0.5, 0.15], [1, 1]],
          "peak": 1
        },
        {
          "id": "mordent",
          "label": "|=",
          "name": {"fr": "mordant", "en": "mordent"},
          "env": [[0, 1], [0.07, 1], [0.2, 0.4], [1, 0.35]]
        }
      ]
    },
    {
      "id": "peak",
      "name": {"fr": "Position du pic", "en": "Peak position"},
      "about": {
        "fr": "où se trouve le pic du profil (delta, en creux), en % de la boîte : un delta asymétrique",
        "en": "where the profile's peak sits (delta, hollow), in % of the box: an asymmetric delta"
      },
      "kind": "number",
      "min": 0,
      "max": 100,
      "step": 1,
      "unit": "%",
      "default": 50,
      "envelope": "peak",
      "onlyWith": {"category": "profile", "values": ["delta", "hollow"]},
      "mark": "none"
    },
    {
      "id": "peakshape",
      "name": {"fr": "Pic", "en": "Peak"},
      "about": {"fr": "un pic dur (anguleux) ou doux (arrondi)", "en": "a hard (angular) or soft (rounded) peak"},
      "kind": "choice",
      "control": "buttons",
      "mark": "none",
      "default": "hard",
      "showDefault": false,
      "envelope": "shape",
      "onlyWith": {"category": "profile", "values": ["delta", "hollow"]},
      "values": [
        {"id": "hard", "name": {"fr": "dur", "en": "hard"}, "curve": "hard"},
        {"id": "soft", "name": {"fr": "doux", "en": "soft"}, "curve": "soft"}
      ]
    }
  ],
  "lanes": [
    {
      "id": "energies",
      "name": {"fr": "Énergies", "en": "Energies"},
      "about": {
        "fr": "une rangée par famille : où les boîtes ont une énergie de cette famille (la seconde énergie plus claire)",
        "en": "one row per family: where boxes have an energy of that family (the second energy lighter)"
      },
      "kind": "rows",
      "category": "energy",
      "second": "energy2",
      "rows": "groups"
    },
    {
      "id": "semiotic",
      "name": {"fr": "Profil sémiotique", "en": "Semiotic profile"},
      "about": {
        "fr": "du niveau le plus concret au plus abstrait présent à chaque instant, et leur moyenne pondérée par le rôle (F 3, M 2, B 1)",
        "en": "from the most concrete to the most abstract level present at each moment, and their average weighted by role (F 3, M 2, B 1)"
      },
      "kind": "scale",
      "category": "level",
      "weightBy": {"category": "role", "weights": {"F": 3, "M": 2, "B": 1}, "default": 2}
    }
  ]
}
)PKG",
R"HELP(# Composition framework

**Energy.** Vande Gorne's energy models (*Techniques d'écriture*, chapter 1), in five families (material, global management, gestural, movement, law), each with its variation factors. A box can have a second energy, drawn lighter.

**Semiotic level.** From the source as it is to the source hidden: I-son (icon), Di-son (diagram), metonymic (Thoresen: a part or a neighbour stands for the whole), Mé-son (metaphor). Peirce via Bayle. A folder can give a level to the boxes inside; a box's own level wins. The level can change along a box.

**Role.** Thoresen's layer functions (foreground, middleground, background) or Vande Gorne's figure and ground: F, M (the default) and B. In the Framework menu, *Role: names* chooses which names show. The role can change along a box.

**Dynamic profile.** The box's intensity envelope, with Schaeffer's names (*Traité*, 31.4) and Vande Gorne's where she has one: flat, resonance (attack-resonance), crescendo (reversed sound), decrescendo, delta, hollow (reversed delta), mordent. The energy glyph is drawn inside the profile, so a bounce can live in a delta. A delta or a hollow profile also has a **peak position** (in % of the box: an asymmetric delta) and a **hard or soft peak**.

**Lanes.** Two overview lanes, added in the ruler panel (*Add lane*): **Energies**, one row per family, with a bar wherever a box has an energy of that family (the second energy lighter, the bar thicker where more boxes sound at once); and the **Semiotic profile**, a band from the most concrete to the most abstract level present at each moment, with a line for their average weighted by role (F 3, M 2, B 1). Muted tracks are left out. The `lanes` list in `framework.json` declares them.

**Glyphs.** The energies are drawn on the boxes by `glyphs.js`, this package's drawing script (JavaScript, sandboxed: no files or network). Edit it to change the shapes; each value's `glyph` field names the shape it uses.

Edit `framework.json` to rename, translate or add values, then *Framework > Reload frameworks*.
)HELP",
Glyphs::builtinSource()),

        make ("schaeffer",
R"PKG({
  "projet": "framework package",
  "id": "schaeffer",
  "version": 3,
  "order": 20,
  "name": {"fr": "Schaeffer", "en": "Schaeffer"},
  "about": {
    "fr": "La typologie du Traité des objets musicaux (tableau récapitulatif, fig. 34) : la masse vers le bas, l'entretien en travers.",
    "en": "The typology of the Treatise on musical objects (summary table, fig. 34): mass down, sustainment across."
  },
  "categories": [
    {
      "id": "type",
      "name": {"fr": "Type (typologie de Schaeffer)", "en": "Type (Schaeffer's typology)"},
      "about": {
        "fr": "le type de l'objet sonore dans la typologie de Schaeffer",
        "en": "the type of the sound object in Schaeffer's typology"
      },
      "kind": "choice",
      "control": "chart",
      "mark": "badge",
      "groups": {
        "name": {"fr": "zone", "en": "zone"},
        "list": [
          {
            "id": "balanced",
            "name": {"fr": "équilibré", "en": "balanced"},
            "colour": "#4ea597",
            "dark": "#1f403b",
            "light": "#8fd6c9"
          },
          {
            "id": "redundant",
            "name": {"fr": "redondant", "en": "redundant"},
            "colour": "#7a7d85",
            "dark": "#2b2d33",
            "light": "#c9c6bd"
          },
          {
            "id": "eccentric",
            "name": {"fr": "excentrique", "en": "eccentric"},
            "colour": "#8f7cc6",
            "dark": "#2e2a45",
            "light": "#c4b8ef"
          }
        ]
      },
      "chart": {
        "rows": [
          {"id": "pitch", "name": {"fr": "hauteur définie", "en": "definite pitch"}},
          {"id": "complex", "name": {"fr": "complexe, fixe", "en": "complex, fixed"}},
          {"id": "varying", "name": {"fr": "peu variable", "en": "slightly varying"}},
          {"id": "unpredictable", "name": {"fr": "variation imprévisible", "en": "unpredictable"}}
        ],
        "columns": [
          {"id": "unpredictable-held", "name": {"fr": "facture imprévisible", "en": "unpredictable facture"}},
          {"id": "homogeneous-held", "name": {"fr": "facture nulle (tenue)", "en": "no facture (held)"}},
          {"id": "held", "name": {"fr": "tenu formé", "en": "formed held"}},
          {"id": "impulse", "name": {"fr": "impulsion", "en": "impulse"}},
          {"id": "iterative", "name": {"fr": "itératif formé", "en": "formed iterative"}},
          {
            "id": "homogeneous-iterative",
            "name": {"fr": "facture nulle (itérative)", "en": "no facture (iterative)"}
          },
          {
            "id": "unpredictable-iterative",
            "name": {"fr": "facture imprévisible", "en": "unpredictable facture"}
          }
        ],
        "caption": {
          "fr": "Masse vers le bas, entretien en travers (Schaeffer, Traité, fig. 34)",
          "en": "Mass down, sustainment across (Schaeffer, Treatise fig. 34)"
        }
      },
      "values": [
        {
          "id": "En",
          "label": "(En)",
          "row": 0,
          "col": 0,
          "groups": ["eccentric"],
          "name": {"fr": "échantillon, tonique", "en": "sample, tonic"}
        },
        {
          "id": "Hn",
          "label": "Hn",
          "row": 0,
          "col": 1,
          "groups": ["redundant"],
          "name": {"fr": "son homogène tonique", "en": "tonic homogeneous sound"}
        },
        {
          "id": "N",
          "label": "N",
          "row": 0,
          "col": 2,
          "groups": ["balanced"],
          "name": {"fr": "son tenu formé, tonique", "en": "formed held note, tonic"}
        },
        {
          "id": "N'",
          "label": "N′",
          "row": 0,
          "col": 3,
          "groups": ["balanced"],
          "name": {"fr": "impulsion tonique", "en": "tonic impulse"}
        },
        {
          "id": "N''",
          "label": "N″",
          "row": 0,
          "col": 4,
          "groups": ["balanced"],
          "name": {"fr": "itératif formé, tonique", "en": "formed iterative note, tonic"}
        },
        {
          "id": "Zn",
          "label": "Zn",
          "row": 0,
          "col": 5,
          "groups": ["redundant"],
          "name": {"fr": "itératif homogène, tonique", "en": "tonic homogeneous iteration"}
        },
        {
          "id": "An",
          "label": "(An)",
          "row": 0,
          "col": 6,
          "groups": ["eccentric"],
          "name": {"fr": "accumulation, tonique", "en": "accumulation, tonic"}
        },
        {
          "id": "Ex",
          "label": "(Ex)",
          "row": 1,
          "col": 0,
          "groups": ["eccentric"],
          "name": {"fr": "échantillon, complexe", "en": "sample, complex"}
        },
        {
          "id": "Hx",
          "label": "Hx",
          "row": 1,
          "col": 1,
          "groups": ["redundant"],
          "name": {"fr": "son homogène complexe", "en": "complex homogeneous sound"}
        },
        {
          "id": "X",
          "label": "X",
          "row": 1,
          "col": 2,
          "groups": ["balanced"],
          "name": {"fr": "son tenu formé, complexe", "en": "formed held note, complex"}
        },
        {
          "id": "X'",
          "label": "X′",
          "row": 1,
          "col": 3,
          "groups": ["balanced"],
          "name": {"fr": "impulsion complexe", "en": "complex impulse"}
        },
        {
          "id": "X''",
          "label": "X″",
          "row": 1,
          "col": 4,
          "groups": ["balanced"],
          "name": {"fr": "itératif formé, complexe", "en": "formed iterative note, complex"}
        },
        {
          "id": "Zx",
          "label": "Zx",
          "row": 1,
          "col": 5,
          "groups": ["redundant"],
          "name": {"fr": "itératif homogène, complexe", "en": "complex homogeneous iteration"}
        },
        {
          "id": "Ax",
          "label": "(Ax)",
          "row": 1,
          "col": 6,
          "groups": ["eccentric"],
          "name": {"fr": "accumulation, complexe", "en": "accumulation, complex"}
        },
        {
          "id": "Ey",
          "label": "(Ey)",
          "row": 2,
          "col": 0,
          "groups": ["eccentric"],
          "name": {"fr": "échantillon, variable", "en": "sample, varying"}
        },
        {
          "id": "Tn",
          "label": "Tn",
          "row": 2,
          "col": 1,
          "groups": ["redundant"],
          "name": {"fr": "trame tonique", "en": "tonic weft"}
        },
        {
          "id": "Tx",
          "label": "Tx",
          "row": 2,
          "col": 1,
          "groups": ["redundant"],
          "name": {"fr": "trame complexe", "en": "complex weft"}
        },
        {
          "id": "Y",
          "label": "Y",
          "row": 2,
          "col": 2,
          "groups": ["balanced"],
          "name": {"fr": "son tenu formé, variable", "en": "formed held note, varying"}
        },
        {
          "id": "Y'",
          "label": "Y′",
          "row": 2,
          "col": 3,
          "groups": ["balanced"],
          "name": {"fr": "impulsion variable", "en": "varying impulse"}
        },
        {
          "id": "Y''",
          "label": "Y″",
          "row": 2,
          "col": 4,
          "groups": ["balanced"],
          "name": {"fr": "itératif formé, variable", "en": "formed iterative note, varying"}
        },
        {
          "id": "Zy",
          "label": "Zy",
          "row": 2,
          "col": 5,
          "groups": ["redundant"],
          "name": {
            "fr": "itératif homogène, variable (ostinato particulier)",
            "en": "varying homogeneous iteration (a particular ostinato)"
          }
        },
        {
          "id": "Ay",
          "label": "(Ay)",
          "row": 2,
          "col": 6,
          "groups": ["eccentric"],
          "name": {"fr": "accumulation, variable", "en": "accumulation, varying"}
        },
        {
          "id": "E",
          "label": "E",
          "row": 3,
          "col": 0,
          "groups": ["eccentric"],
          "name": {"fr": "échantillon", "en": "sample"}
        },
        {
          "id": "T",
          "label": "T",
          "row": 3,
          "col": 1,
          "groups": ["eccentric"],
          "name": {"fr": "trame", "en": "weft"}
        },
        {
          "id": "W",
          "label": "W",
          "row": 3,
          "col": 2,
          "groups": ["eccentric"],
          "name": {"fr": "grosse note", "en": "large note"}
        },
        {
          "id": "Phi",
          "label": "Φ",
          "row": 3,
          "col": 3,
          "groups": ["eccentric"],
          "name": {"fr": "fragment", "en": "fragment"}
        },
        {
          "id": "K",
          "label": "K",
          "row": 3,
          "col": 4,
          "groups": ["eccentric"],
          "name": {"fr": "cellule", "en": "cell"}
        },
        {
          "id": "P",
          "label": "P",
          "row": 3,
          "col": 5,
          "groups": ["eccentric"],
          "name": {"fr": "ostinato", "en": "ostinato"}
        },
        {
          "id": "A",
          "label": "A",
          "row": 3,
          "col": 6,
          "groups": ["eccentric"],
          "name": {"fr": "accumulation", "en": "accumulation"}
        }
      ]
    }
  ],
  "lanes": [
    {
      "id": "typology",
      "name": {"fr": "Typologie", "en": "Typology"},
      "about": {
        "fr": "une rangée par zone (équilibrés, redondants, excentriques) : le mélange des types d'objets au fil de la pièce",
        "en": "one row per zone (balanced, redundant, eccentric): the mix of object kinds across the piece"
      },
      "kind": "rows",
      "category": "type",
      "rows": "groups"
    }
  ]
}
)PKG",
R"HELP(# Schaeffer

**Type.** The typology of the *Traité des objets musicaux* (summary table, fig. 34): the mass of the sound down (definite pitch, complex and fixed, slightly varying, unpredictable), its sustainment across (from unpredictable facture to unpredictable facture, through held, impulse and iterative). The balanced types sit in the middle, the redundant ones beside them, the eccentric ones at the edges.

**Typology lane.** One row each for balanced, redundant and eccentric types, so the mix of object kinds across the piece shows at a glance (in the ruler panel: *Add lane*).

Later stages may add the seven morphological criteria and his genres.
)HELP"),

        make ("rk1",
R"PKG({
  "projet": "framework package",
  "id": "rk1",
  "version": 1,
  "order": 90,
  "name": {"fr": "Langage de structuration (RK1)", "en": "Structuring language (RK1)"},
  "about": {
    "fr": "Les opérations de votre langage de structuration (10 primitives, 16 macros), provisoires : utilisées à partir de l'étape 8.",
    "en": "The operations of your structuring language (10 primitives, 16 macros), provisional: used from stage 8."
  },
  "status": "provisional: the structuring language of the RK1 research (26 October 2026); used from stage 8",
  "categories": [],
  "operations": {
    "primitives": [
      {
        "id": "REPEAT",
        "about": "Copy objects with selected attributes inherited",
        "params": ["source", "inherit", "override"]
      },
      {
        "id": "TRANSPOSE",
        "about": "Shift pitches in chromatic, diatonic or registral coordinates",
        "params": ["field", "amount"]
      },
      {
        "id": "INTERPOLATE",
        "about": "Insert new events between structural anchors",
        "params": ["from", "to", "count"]
      },
      {
        "id": "NEIGHBOUR",
        "about": "Expand an anchor by departure and return",
        "params": ["anchor", "field", "amount"]
      },
      {"id": "SUBDIVIDE", "about": "Replace one duration or event with shorter events", "params": ["parts"]},
      {
        "id": "SUBSTITUTE",
        "about": "Replace selected events or their attribute values",
        "params": ["target", "with"]
      },
      {
        "id": "SUPERPOSE",
        "about": "Combine processes in simultaneous or time-offset layers",
        "params": ["layers", "offset"]
      },
      {"id": "CONCAT", "about": "Concatenate sequences into larger structures", "params": ["parts"]},
      {
        "id": "ARPEGGIATE",
        "about": "Linearize a collection of simultaneous pitches",
        "params": ["order", "spacing"]
      },
      {
        "id": "INVERT",
        "about": "Reflect pitch or interval directions around an axis",
        "params": ["field", "axis"]
      }
    ],
    "macros": [
      {"id": "OSTINATO", "expansion": "REPEAT + stable attribute template", "onBoxes": true},
      {"id": "DIATONIC_RUN", "expansion": "INTERPOLATE (diatonic)", "onBoxes": false, "alias": "CONNECT"},
      {"id": "DIATONIC_SEQUENCE", "expansion": "REPEAT + systematic TRANSPOSE", "onBoxes": true},
      {"id": "ORNAMENT_CHORD", "expansion": "chord-tone path + NEIGHBOUR / INTERPOLATE", "onBoxes": false},
      {"id": "ARPEGGIATED_EXPANSION", "expansion": "ARPEGGIATE + ornament", "onBoxes": false},
      {"id": "HARMONIC_PROJECTION", "expansion": "two parts from one pitch collection", "onBoxes": false},
      {
        "id": "HARMONIC_CONSOLIDATION",
        "expansion": "earlier linear pitches recombined as a sonority",
        "onBoxes": false
      },
      {"id": "CHROMATIC_DESCENT", "expansion": "REPEAT + TRANSPOSE (-1 semitone)", "onBoxes": false},
      {"id": "IMITATE", "expansion": "REPEAT + time offset + TRANSPOSE", "onBoxes": true},
      {"id": "ROLE_EXCHANGE", "expansion": "SUBSTITUTE of active and supporting roles", "onBoxes": true},
      {"id": "CADENTIAL_SUBSTITUTION", "expansion": "REPEAT + SUBSTITUTE (ending)", "onBoxes": true},
      {"id": "VARIED_RECAPITULATION", "expansion": "REPEAT with selective overrides", "onBoxes": true},
      {"id": "ARTICULATED_GESTURE", "expansion": "rhythmic profile + articulation pattern", "onBoxes": true},
      {"id": "CLOSING_GESTURE", "expansion": "recurrent closing rhythmic profile", "onBoxes": true},
      {"id": "DYNAMIC_REVERSAL", "expansion": "SUBSTITUTE (permuted dynamic values)", "onBoxes": true},
      {"id": "SUSTAINED_DYAD", "expansion": "sustained layer, tied continuations", "onBoxes": true}
    ]
  }
}
)PKG",
R"HELP(# Structuring language (RK1)

The operations of your structuring language, from the RK1 research: 10 primitives and 16 macros built on them. Provisional, and not used before stage 8 (relations), where they become operations between boxes. The package has no categories yet: it only carries the operations, so they travel with the projects and the library.
)HELP"),
        make ("thoresen",
R"PKG({
  "projet": "framework package",
  "id": "thoresen",
  "version": 3,
  "order": 30,
  "name": {"fr": "Thoresen : champs temporels", "en": "Thoresen: time-fields"},
  "about": {
    "fr": "Les jonctions entre boîtes et leurs fins, d'après les conjonctions de champs temporels de Lasse Thoresen (Emergent Musical Forms, 6.3) ; la forme dynamique, dans sa piste (ch. 8).",
    "en": "The junctions between boxes and how boxes end, after Lasse Thoresen's time-field conjunctions (Emergent Musical Forms, 6.3); the dynamic form, in its lane (chapter 8)."
  },
  "categories": [
    {
      "id": "junction",
      "name": {"fr": "Jonction avec la boîte précédente", "en": "Junction with the box before"},
      "about": {
        "fr": "comment la boîte s'enchaîne à la précédente sur la même piste (positionnement des champs temporels, 6.3.1) : trouvé d'après le minutage, ou choisi",
        "en": "how the box follows the one before it on the same track (time-field positioning, 6.3.1): found from the timing, or chosen"
      },
      "kind": "choice",
      "control": "buttons",
      "between": "previous",
      "mark": "seam",
      "derive": [
        {"value": "separate", "from": 0.5, "covered": false},
        {"value": "bridged", "from": 0.5, "covered": true},
        {"value": "joint", "from": 0.02},
        {"value": "close", "from": -0.02},
        {"value": "hinged", "from": -8},
        {"value": "overlapping"}
      ],
      "notes": [
        {
          "from": 0.02,
          "to": 0.05,
          "text": {
            "fr": "entre 20 et 50 ms, l'attaque se brouille (Vande Gorne)",
            "en": "between 20 and 50 ms the attack blurs (Vande Gorne)"
          }
        }
      ],
      "values": [
        {
          "id": "separate",
          "name": {"fr": "séparé", "en": "separate"},
          "about": {"fr": "un silence perceptible entre les deux", "en": "a noticeable silence between them"},
          "sign": "M0 15 L20 15 L20 7 M40 7 L40 15 L60 15 M26 10 L29 14 L34 5",
          "endAt": [20, 15]
        },
        {
          "id": "bridged",
          "name": {"fr": "ponté", "en": "bridged"},
          "about": {
            "fr": "un écart franchi par une transition ou un fond ininterrompu (une boîte sur une autre piste)",
            "en": "a gap spanned by a transition or an uninterrupted background (a box on another track)"
          },
          "sign": "M0 15 L20 15 L20 7 M40 7 L40 15 L60 15 M26 12 L34 9",
          "endAt": [20, 15]
        },
        {
          "id": "joint",
          "name": {"fr": "joint", "en": "joint"},
          "about": {
            "fr": "la suivante commence juste après la fin de la première",
            "en": "the next begins just after the first has ended"
          },
          "sign": "M0 15 L27 15 L27 7 M33 7 L33 15 L60 15",
          "endAt": [27, 15]
        },
        {
          "id": "close",
          "name": {"fr": "serré", "en": "close"},
          "about": {"fr": "une succession très serrée (à 20 ms près)", "en": "a very tight succession (within 20 ms)"},
          "sign": "M0 15 L60 15 M30 15 L30 7",
          "endAt": [30, 15]
        },
        {
          "id": "hinged",
          "name": {"fr": "charnière", "en": "hinged"},
          "about": {
            "fr": "la fin de l'une coïncide avec le début de la suivante (un chevauchement jusqu'à 8 s)",
            "en": "the end of one coincides with the start of the next (an overlap up to 8 s)"
          },
          "sign": "M0 15 L33 15 L33 7 M27 7 L27 15 L60 15",
          "endAt": [33, 15],
          "fadeAt": 27
        },
        {
          "id": "overlapping",
          "name": {"fr": "chevauchant", "en": "overlapping"},
          "about": {
            "fr": "la suivante commence bien avant la fin de la première",
            "en": "the next begins well before the first has ended"
          },
          "sign": "M0 11 L40 11 L40 5 M20 9 L20 17 L60 17",
          "endAt": [40, 11]
        },
        {
          "id": "superimposed",
          "name": {"fr": "superposé", "en": "superimposed"},
          "about": {
            "fr": "en couches, l'une sur l'autre (seulement choisi)",
            "en": "in layers, one over the other (chosen only)"
          },
          "sign": "M8 3 L8 8 L28 8 L28 3 M32 3 L32 8 L52 8 L52 3 M0 17 L12 17 L12 12 M18 12 L18 17 L42 17 L42 12 M48 12 L48 17 L60 17"
        }
      ]
    },
    {
      "id": "ending",
      "name": {"fr": "Fin", "en": "Ending"},
      "about": {
        "fr": "comment la boîte se termine (démarcation des champs temporels, 6.3.2)",
        "en": "how the box ends (time-field demarcation, 6.3.2)"
      },
      "kind": "choice",
      "control": "buttons",
      "atEnd": true,
      "mark": "end",
      "values": [
        {
          "id": "vague",
          "name": {"fr": "vague", "en": "vague"},
          "about": {"fr": "on ne sait pas bien où le champ s'arrête", "en": "it is unclear where the field ends"},
          "sign": "M0 15 L13.4 15 M17.1 15 L19.9 15 M23.6 15 L24.9 15 M29.4 15 L29.6 15",
          "seamFade": 16
        },
        {
          "id": "open",
          "name": {"fr": "ouverte", "en": "open"},
          "about": {
            "fr": "une fin arrondie, qui suggère une suite",
            "en": "a rounded ending that suggests a continuation"
          },
          "sign": "M0 15 L22 15 M22 9 L22 20",
          "seamSign": "M0 0 L0 5"
        },
        {
          "id": "conclusive",
          "name": {"fr": "conclusive", "en": "conclusive"},
          "about": {"fr": "une fin nette, comme une cadence parfaite", "en": "a definite ending, like a full cadence"},
          "sign": "M0 15 L20 15 M20 9 L20 20 M24 9 L24 20",
          "seamSign": "M0 0 L0 5 M-4 -7 L-4 5"
        },
        {
          "id": "cut",
          "name": {"fr": "coupée", "en": "cut"},
          "about": {
            "fr": "une fin soudaine, ou un raccourci inattendu",
            "en": "a sudden ending, or an unexpected abbreviation"
          },
          "sign": "M0 15 L22 15 L22 10 M20.5 6 A1.5 1.5 0 1 0 23.5 6 A1.5 1.5 0 1 0 20.5 6",
          "seamSign": "M-1.5 -11 A1.5 1.5 0 1 0 1.5 -11 A1.5 1.5 0 1 0 -1.5 -11"
        },
        {
          "id": "disjointed",
          "name": {"fr": "disloquée", "en": "disjointed"},
          "about": {"fr": "une fin très abrupte, fragmentée", "en": "a very abrupt, fragmented ending"},
          "sign": "M0 15 L22 15 L22 10 M19 3 L25 9 M25 3 L19 9",
          "seamSign": "M-3 -15 L3 -9 M3 -15 L-3 -9"
        },
        {
          "id": "deferred",
          "name": {"fr": "différée", "en": "deferred"},
          "about": {"fr": "le champ se prolonge de façon inattendue", "en": "the field is unexpectedly prolonged"},
          "sign": "M0 15 L14 15 C22 15 24 6 19 6 C14 6 15 15 24 15 L30 15",
          "seamSign": "M-16 0 C-8 0 -6 -9 -11 -9 C-16 -9 -15 0 -6 0"
        }
      ]
    },
    {
      "id": "function",
      "name": {"fr": "Fonction de forme", "en": "Form-building function"},
      "about": {
        "fr": "ce que fait une plage de la forme dynamique (8.1.1)",
        "en": "what a stretch of the dynamic form does (8.1.1)"
      },
      "kind": "choice",
      "control": "buttons",
      "on": "lane",
      "values": [
        {
          "id": "forward",
          "name": {"fr": "orientée vers l'avant", "en": "forward-oriented"},
          "about": {
            "fr": "dirigée vers un point à venir, ou une tendance croissante",
            "en": "directed towards a future point in time, or an increasing tendency"
          },
          "shape": "forward"
        },
        {
          "id": "presence",
          "name": {"fr": "orientée vers la présence", "en": "presence-oriented"},
          "about": {
            "fr": "une tendance stable, ou nulle : ni vers l'avenir ni vers le passé",
            "en": "a stable (or no) tendency: directed towards neither future nor past"
          },
          "shape": "presence"
        },
        {
          "id": "backward",
          "name": {"fr": "orientée vers l'arrière", "en": "backward-oriented"},
          "about": {
            "fr": "s'éloignant d'un point passé, ou une tendance décroissante",
            "en": "directed away from a past point in time, or a decreasing tendency"
          },
          "shape": "backward"
        },
        {
          "id": "neutral",
          "name": {"fr": "neutre", "en": "neutral"},
          "about": {
            "fr": "la négation des trois (non leur contraire) : aucune fonction claire",
            "en": "the negation of the three (not their opposite): no clear function"
          },
          "shape": "neutral"
        },
        {
          "id": "delta",
          "name": {"fr": "delta", "en": "delta"},
          "about": {
            "fr": "vers un but puis s'en éloignant, le but n'importe où dans la plage (la phrase en delta de Vande Gorne)",
            "en": "towards a goal and away from it, the goal anywhere along the span (Vande Gorne's phrase shaped as a delta)"
          },
          "shape": "delta"
        }
      ]
    },
    {
      "id": "emphasis",
      "name": {"fr": "Accentuation", "en": "Emphasis"},
      "about": {
        "fr": "avec quelle force la fonction s'affirme ; une fonction faible, simultanée à une autre, est secondaire",
        "en": "how strongly the function shows; a faint one beside another is secondary"
      },
      "kind": "choice",
      "control": "buttons",
      "on": "lane",
      "default": "average",
      "values": [
        {
          "id": "faint",
          "name": {"fr": "faible (secondaire)", "en": "faint (secondary)"},
          "about": {"fr": "en tirets", "en": "dashed"},
          "stroke": "dashed"
        },
        {
          "id": "average",
          "name": {"fr": "moyenne (primaire)", "en": "average (primary)"},
          "about": {"fr": "en trait plein", "en": "a plain line"},
          "stroke": "solid"
        },
        {
          "id": "emphasised",
          "name": {"fr": "accentuée", "en": "emphasised"},
          "about": {"fr": "en trait épais", "en": "a heavy line"},
          "stroke": "bold"
        }
      ]
    },
    {
      "id": "goal",
      "name": {"fr": "Atteinte du but", "en": "Goal attainment"},
      "about": {
        "fr": "au bout d'une fonction orientée vers l'avant (ou au but d'un delta) : le but est-il atteint ? (8.1.1)",
        "en": "at the end of a forward-oriented function (or a delta's goal): is the goal reached? (8.1.1)"
      },
      "kind": "choice",
      "control": "buttons",
      "on": "lane",
      "values": [
        {
          "id": "attained",
          "name": {"fr": "but atteint", "en": "goal attainment"},
          "about": {"fr": "la fonction atteint le but attendu", "en": "the function reaches its expected goal"},
          "sign": "M1 0 L1 20"
        },
        {
          "id": "blunted",
          "name": {"fr": "atteinte émoussée", "en": "blunted"},
          "about": {
            "fr": "le but est atteint, mais les dimensions y arrivent à des moments différents",
            "en": "the goal is reached, but different dimensions arrive at different times"
          },
          "sign": "M-3 2 L-3 8 M1 6 L1 14 M5 12 L5 18"
        },
        {
          "id": "suspended",
          "name": {"fr": "but suspendu", "en": "suspended"},
          "about": {
            "fr": "le but attendu est d'abord suspendu, puis arrive plus tard",
            "en": "the expected goal is first suspended, then arrives later"
          },
          "sign": "M2 10 L3 10 M5.5 10 L6.5 10 M9 10 L10 10 M13 0 L13 20"
        },
        {
          "id": "evaded",
          "name": {"fr": "évitement complet", "en": "fully evaded"},
          "about": {"fr": "le but n'est pas atteint (annulé)", "en": "the goal is not reached (cancelled)"},
          "sign": "M1 0 L1 20 M-3 5 L5 15 M5 5 L-3 15"
        },
        {
          "id": "partial",
          "name": {"fr": "évitement partiel", "en": "partly evaded"},
          "about": {
            "fr": "le but n'est pas atteint pleinement, ou pas comme attendu",
            "en": "the goal is not fully reached, or not as expected"
          },
          "sign": "M1 6 L1 14"
        }
      ]
    },
    {
      "id": "accent",
      "name": {"fr": "Fonctions d'accent", "en": "Accent functions"},
      "about": {
        "fr": "un point pondéré qui ponctue la forme ; plusieurs fonctions peuvent se combiner (8.1.2)",
        "en": "a weighted point that punctuates the form; several functions can combine (8.1.2)"
      },
      "kind": "tags",
      "on": "lane",
      "values": [
        {
          "id": "accent",
          "name": {"fr": "accent", "en": "accent"},
          "about": {"fr": "le signe général : un point pondéré", "en": "the general sign: a weighted point"},
          "sign": "M4 4 L16 4"
        },
        {
          "id": "goal",
          "name": {"fr": "point de but", "en": "goal point"},
          "about": {
            "fr": "un accent qui marque le but d'un mouvement",
            "en": "an accent marking the goal of a movement"
          },
          "sign": "M6 4 A4 4 0 1 0 14 4 A4 4 0 1 0 6 4 M10 3.6 L10 4.4"
        },
        {
          "id": "separation",
          "name": {"fr": "point de séparation", "en": "separation point"},
          "about": {
            "fr": "un accent qui sépare deux champs temporels",
            "en": "an accent setting two time-fields apart"
          },
          "sign": "M5 12 L5 20 M15 12 L15 20"
        },
        {
          "id": "release",
          "name": {"fr": "point de lancement", "en": "release point"},
          "about": {
            "fr": "un accent qui lance un mouvement, ou ouvre un champ",
            "en": "an accent launching a motion, or opening a field"
          },
          "sign": "M10 12 L18 12 M15 9 L18 12 L15 15"
        },
        {
          "id": "termination",
          "name": {"fr": "point de terminaison", "en": "termination point"},
          "about": {
            "fr": "un accent qui termine un champ ou une fonction",
            "en": "an accent ending a time-field or a function"
          },
          "sign": "M4 1 L4 8 M16 1 L16 8"
        },
        {
          "id": "affirmation",
          "name": {"fr": "point d'affirmation", "en": "affirmation point"},
          "about": {
            "fr": "un accent qui affirme ou répète un accent ou un élément passé",
            "en": "an accent affirming or repeating an earlier accent or element"
          },
          "sign": "M4 8 L16 8"
        },
        {
          "id": "warning",
          "name": {"fr": "point d'alerte", "en": "warning point"},
          "about": {
            "fr": "un accent qui annonce un événement imminent",
            "en": "an accent alerting the listener to an imminent event"
          },
          "sign": "M1.5 4 L1.6 4"
        }
      ]
    }
  ],
  "lanes": [
    {
      "id": "dynamic",
      "name": {"fr": "Forme dynamique", "en": "Dynamic form"},
      "about": {
        "fr": "les fonctions de forme, l'atteinte du but et les accents (Thoresen, ch. 8), dessinés à la main jusqu'aux conteneurs (étape 5)",
        "en": "form-building functions, goal attainment and accents (Thoresen, chapter 8), drawn by hand until containers come (stage 5)"
      },
      "kind": "drawn",
      "spans": "function",
      "emphasis": "emphasis",
      "goal": "goal",
      "goalOn": ["forward", "delta"],
      "points": "accent"
    }
  ]
}
)PKG",
R"HELP(# Thoresen: time-fields

Lasse Thoresen's time-field conjunctions (*Emergent Musical Forms*, 6.3), between the boxes of a track.

**Junction with the box before.** How a box follows the one before it on the same track (the one that starts most recently before it): separate (a noticeable silence), bridged (a gap spanned by a box on another track that is not muted), joint (just after), close (within 20 ms), hinged (an overlap up to 8 s), overlapping (a longer one), superimposed (chosen only). Projet finds it from the timing until you choose one; *From the timing* goes back. The rules and thresholds are in `framework.json` (`derive`): edit them, or add kinds of your own.

**Ending.** How a box ends: vague, open, conclusive, cut, disjointed, deferred, or not marked.

**Signs.** Drawn at the top of the lane, after Thoresen's figures 6.2 and 6.3. Where a box with an ending meets the next box, the two make one sign, the ending's mark on the first box's bracket (his figure 6.4). A junction found from the timing has a dashed frame; a chosen one a solid frame. Each value's `sign` is an SVG path: a junction's in a 60 by 20 box with `endAt` where the first box's bracket ends, an ending's `seamSign` drawn from that point. An ending's `seamFade` turns that much of the line before the bracket into dashes instead (the vague ending), stopping at the junction's `fadeAt` if it has one (where the next box begins, for hinged).

**The dynamic form** (chapter 8), in a lane of its own (*Add a lane below* > *Forme dynamique*), drawn by hand until containers come (stage 5). Drag in the lane to draw a span and choose its **form-building function**: forward-oriented (a wedge opening towards what comes), presence-oriented (a bar), backward-oriented (a wedge closing), neutral (a thin line), or a **delta** (towards a goal and away from it, the goal anywhere: drag its handle). Its **emphasis**: faint (secondary, dashed), average (primary) or emphasised (heavy). Overlapping spans stack, so a secondary function can run beside a primary one. A forward-oriented span or a delta can reach its **goal**: attained, blunted, suspended, fully or partly evaded. Click in the lane (without dragging) for an **accent**, a weighted point with one or more functions: accent, goal point, separation, release, termination, affirmation, warning.

The scans of figures 8.1 to 8.4 are hard to read, so the signs follow the text: wedges and a bar, dashed when faint and heavy when emphasised, a T for the weighted point and a circled point for a goal. They are data: a span kind's `shape` (forward, backward, presence, neutral, delta), an emphasis' `stroke` (dashed, solid, bold), a goal's and an accent's `sign` (an SVG path in a 20 by 20 box: a goal's from the goal at x 0, an accent's around a stem at x 10). Categories with `"on": "lane"` belong to the lane; the lane names them (`spans`, `emphasis`, `goal`, `goalOn`, `points`).
)HELP"),
        make ("vandegorne",
R"PKG({
  "projet": "framework package",
  "id": "vandegorne",
  "version": 3,
  "order": 40,
  "name": {"fr": "Vande Gorne : écriture", "en": "Vande Gorne: writing"},
  "about": {
    "fr": "Les relations de mélange sur les chevauchements, d'après Annette Vande Gorne (Techniques d'écriture sur support, ch. 3).",
    "en": "Mixing relations on overlaps, after Annette Vande Gorne (writing techniques for fixed media, chapter 3)."
  },
  "categories": [
    {
      "id": "mixing",
      "name": {"fr": "Relation de mélange", "en": "Mixing relation"},
      "about": {
        "fr": "sur un chevauchement avec la boîte précédente : comment les deux sons se mêlent",
        "en": "on an overlap with the box before: how the two sounds mix"
      },
      "kind": "choice",
      "control": "buttons",
      "between": "previous",
      "mark": "seam",
      "onlyWith": {"category": "thoresen.junction", "values": ["hinged", "overlapping", "superimposed"]},
      "values": [
        {
          "id": "fusion",
          "name": {"fr": "fusion", "en": "fusion"},
          "about": {"fr": "un nouveau son naît des deux", "en": "a new sound arises from both"},
          "sign": "M0 5 L10 10 L20 10 M0 15 L10 10"
        },
        {
          "id": "complementarity",
          "name": {"fr": "complémentarité", "en": "complementarity"},
          "about": {"fr": "chacun garde son identité, comme des voix", "en": "each keeps its identity, like voices"},
          "sign": "M0 7 L5 7 M10 7 L15 7 M5 13 L10 13 M15 13 L20 13"
        },
        {
          "id": "rejection",
          "name": {"fr": "rejet", "en": "rejection"},
          "about": {
            "fr": "l'un masque l'autre, ou le démasque en disparaissant",
            "en": "one masks the other, or unmasks it as it goes"
          },
          "sign": "M4 3 Q10 10 4 17 M16 3 Q10 10 16 17"
        },
        {
          "id": "relay",
          "name": {"fr": "relais d'énergie", "en": "relay of energy"},
          "about": {
            "fr": "un fondu-enchaîné où un son lance l'autre, sans fusion",
            "en": "a crossfade where one sound launches the next, without fusion"
          },
          "sign": "M0 10 L16 10 M11 5 L16 10 L11 15",
          "onlyWith": {"category": "thoresen.junction", "values": ["hinged"]}
        },
        {
          "id": "metamorphosis",
          "name": {"fr": "métamorphose indirecte", "en": "indirect metamorphosis"},
          "about": {
            "fr": "un fondu-enchaîné par un point d'équilibre, la même énergie",
            "en": "a crossfade through a point of balance, the same energy"
          },
          "sign": "M0 10 Q2.5 4 5 10 Q7.5 16 10 10 L20 10",
          "onlyWith": {"category": "thoresen.junction", "values": ["hinged"]}
        }
      ]
    },
    {
      "id": "writing",
      "name": {"fr": "Type d'écriture", "en": "Writing type"},
      "about": {
        "fr": "comment les voix du dossier se mêlent (Vande Gorne, ch. 3) ; certains impliquent une relation de mélange pour les chevauchements du dossier",
        "en": "how the voices of the folder mix (Vande Gorne, chapter 3); some imply a mixing relation for the overlaps in the folder"
      },
      "kind": "choice",
      "control": "menu",
      "folders": "only",
      "mark": "label",
      "values": [
        {
          "id": "weaving",
          "name": {"fr": "tissage, texture", "en": "weaving, texture"},
          "about": {
            "fr": "des éléments courts d'égale importance, en fusion ; un temps non linéaire, sans développement",
            "en": "short elements of equal importance, fused; time without direction or development"
          },
          "implies": {"category": "vandegorne.mixing", "value": "fusion"}
        },
        {
          "id": "stainedglass",
          "name": {"fr": "vitrail, mosaïque", "en": "stained glass, mosaic"},
          "about": {
            "fr": "des états de polyphonie juxtaposés, comme des changements de couleur",
            "en": "juxtaposed states of polyphony, like changes of colour"
          }
        },
        {
          "id": "complex",
          "name": {"fr": "dessin complexe", "en": "complex drawing"},
          "about": {
            "fr": "des figures très formées, superposées et synchronisées en détail : mélange par complémentarité",
            "en": "highly formed figures, superposed and synchronised in detail: mixing by complementarity"
          },
          "implies": {"category": "vandegorne.mixing", "value": "complementarity"}
        },
        {
          "id": "lines",
          "name": {"fr": "lignes-divergences et nœuds-convergences", "en": "diverging lines and converging nodes"},
          "about": {
            "fr": "des lignes parallèles qui divergent, puis convergent vers des nœuds : des changements brusques et synchrones",
            "en": "parallel lines that diverge, then converge on nodes: sudden, synchronous changes"
          }
        },
        {
          "id": "rhythms",
          "name": {"fr": "rythmes et vitesses", "en": "rhythms and speeds"},
          "about": {
            "fr": "des couches de rythmes répétés et différenciés, aux rencontres toujours renouvelées : mélange par complémentarité",
            "en": "layers of repeated, differentiated rhythms whose meetings keep changing: mixing by complementarity"
          },
          "implies": {"category": "vandegorne.mixing", "value": "complementarity"}
        },
        {
          "id": "melismas",
          "name": {"fr": "mélismes sur ostinato", "en": "melismas on an ostinato"},
          "about": {
            "fr": "des séquences agiles et changeantes sur un fond qui se répète à grande échelle",
            "en": "agile, changing sequences over a background that repeats on a large scale"
          }
        },
        {
          "id": "images",
          "name": {"fr": "superposition polyphonique d'images", "en": "superposed images"},
          "about": {
            "fr": "une icône (un faux paysage) faite de plusieurs i-sons, développés dans le temps",
            "en": "an icon (a false landscape) built from several I-sons, developed in time"
          }
        }
      ]
    },
    {
      "id": "timefelt",
      "name": {"fr": "Temps ressenti", "en": "Time felt"},
      "about": {
        "fr": "comment le temps de la section est perçu (Vande Gorne, ch. 4)",
        "en": "how the section's time is perceived (Vande Gorne, chapter 4)"
      },
      "kind": "choice",
      "control": "buttons",
      "folders": "only",
      "mark": "label",
      "values": [
        {
          "id": "long",
          "name": {"fr": "long", "en": "long"},
          "about": {
            "fr": "peu d'événements, des silences, de la répétition, des sons longs ou graves",
            "en": "few events, silences, repetition, long or low sounds"
          }
        },
        {
          "id": "short",
          "name": {"fr": "court", "en": "short"},
          "about": {
            "fr": "la stratégie inverse : des événements nombreux, variés, rapides",
            "en": "the opposite: many varied, quick events"
          }
        },
        {
          "id": "suspended",
          "name": {"fr": "suspendu", "en": "suspended"},
          "about": {
            "fr": "un temps gelé, hypnotique : lent, continu, sans agitation, ou fondé sur la répétition",
            "en": "frozen, hypnotic time: slow, continuous, calm, or built on repetition"
          }
        }
      ]
    },
    {
      "id": "timemove",
      "name": {"fr": "Mouvement du temps", "en": "Time movement"},
      "about": {
        "fr": "comment le temps évolue dans la section (Vande Gorne, ch. 4 : fixe, mobile, intermodulé)",
        "en": "how time moves through the section (Vande Gorne, chapter 4: fixed, mobile, intermodulated)"
      },
      "kind": "choice",
      "control": "menu",
      "folders": "only",
      "mark": "label",
      "values": [
        {
          "id": "fixed",
          "name": {"fr": "fixe", "en": "fixed"},
          "about": {
            "fr": "une seule perception du temps sur toute la section",
            "en": "one perception of time through the whole section"
          }
        },
        {
          "id": "speeding",
          "name": {"fr": "mobile : accélérant", "en": "mobile: speeding up"},
          "about": {
            "fr": "un profil temporel par accélération progressive",
            "en": "a time profile of gradual speeding up"
          }
        },
        {
          "id": "slowing",
          "name": {"fr": "mobile : ralentissant", "en": "mobile: slowing down"},
          "about": {
            "fr": "un profil temporel par ralentissement progressif",
            "en": "a time profile of gradual slowing down"
          }
        },
        {
          "id": "contrasts",
          "name": {"fr": "mobile : contrastes", "en": "mobile: contrasts"},
          "about": {
            "fr": "des contrastes temporels successifs et différenciés",
            "en": "successive, differentiated contrasts of time"
          }
        },
        {
          "id": "intermodulated",
          "name": {"fr": "intermodulé", "en": "intermodulated"},
          "about": {
            "fr": "une voie de commande module le temps : compression-expansion, vitesse, longueur de boucle",
            "en": "a control channel modulates time: compression and expansion, speed, loop length"
          }
        }
      ]
    }
  ],
  "nodes": {
    "name": {"fr": "nœud", "en": "node"},
    "about": {
      "fr": "un changement brusque et synchrone de toutes les voies : une courte résolution, unique et évidente ; trop court il passe inaperçu, trop long il devient ligne (ch. 3)",
      "en": "a sudden, synchronous change of every voice: a short, single, obvious resolution; too short it goes unheard, too long it becomes a line (chapter 3)"
    },
    "short": 0.02,
    "long": 0.25,
    "window": 0.05
  }
}
)PKG",
R"HELP(# Vande Gorne: writing

Annette Vande Gorne's mixing relations and writing types (*Techniques d'écriture sur support*, chapter 3) and her time-feel (chapter 4).

**Mixing relation.** Offered when the junction with the box before is hinged, overlapping or superimposed: fusion (a new sound arises from both), complementarity (each keeps its identity), rejection (one masks the other, or unmasks it as it goes). On a hinged overlap used as a crossfade, also relay of energy (one sound launches the next, without fusion) and indirect metamorphosis (through a point of balance, the same energy). Its sign sits beside the junction sign.

**Writing type**, set on a folder (right-click its header): weaving or texture, stained glass or mosaic, complex drawing, diverging lines and converging nodes, rhythms and speeds, melismas on an ostinato, superposed images. Where she names the mixing relation a writing implies, the overlaps in the folder take it (weaving: fusion; complex drawing, rhythms and speeds: complementarity), drawn faint until you choose another; *From the folder* goes back. A value's `implies` field says which.

**Time felt** (long, short, suspended) and **time movement** (fixed; mobile: speeding up, slowing down, contrasts; intermodulated), also on folders, the sections of a piece until containers come (stage 5).

Folder values show on the folder's lane. Between boxes on different tracks, mixing relations come with the relations of stage 8.

**Nodes** (chapter 3): a sudden, synchronous change of every voice, a short resolution. Drag on a folder's lane to place one (right-click it to remove it). It is drawn as a narrow band across the folder's tracks and the lanes. Its length should balance what comes before it: too short and it goes unheard, too long and it becomes a line. So a node is flagged when shorter than 2% or longer than 25% of the stretch since the previous node (or the folder's first box), and the panel names the folder's tracks that do not change within it. The numbers are in `framework.json` (`nodes`: `short`, `long`, and `window`, how far outside the node a change still counts, in seconds).
)HELP")
    };
    return list;
}
