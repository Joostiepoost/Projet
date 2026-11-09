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
  "version": 3,
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
          "env": [[0, 0.06], [0.5, 1], [1, 0.06]]
        },
        {
          "id": "hollow",
          "label": "><",
          "name": {"fr": "en creux", "en": "hollow"},
          "name2": {"fr": "delta inversé", "en": "reversed delta"},
          "env": [[0, 1], [0.5, 0.15], [1, 1]]
        },
        {
          "id": "mordent",
          "label": "|=",
          "name": {"fr": "mordant", "en": "mordent"},
          "env": [[0, 1], [0.07, 1], [0.2, 0.4], [1, 0.35]]
        }
      ]
    }
  ]
}
)PKG",
R"HELP(# Composition framework

**Energy.** Vande Gorne's energy models (*Techniques d'écriture*, chapter 1), in five families (material, global management, gestural, movement, law), each with its variation factors. A box can have a second energy, drawn lighter.

**Semiotic level.** From the source as it is to the source hidden: I-son (icon), Di-son (diagram), metonymic (Thoresen: a part or a neighbour stands for the whole), Mé-son (metaphor). Peirce via Bayle. A folder can give a level to the boxes inside; a box's own level wins. The level can change along a box.

**Role.** Thoresen's layer functions (foreground, middleground, background) or Vande Gorne's figure and ground: F, M (the default) and B. In the Framework menu, *Role: names* chooses which names show. The role can change along a box.

**Dynamic profile.** The box's intensity envelope, with Schaeffer's names (*Traité*, 31.4) and Vande Gorne's where she has one: flat, resonance (attack-resonance), crescendo (reversed sound), decrescendo, delta, hollow (reversed delta), mordent. The energy glyph is drawn inside the profile, so a bounce can live in a delta.

**Glyphs.** The energies are drawn on the boxes by `glyphs.js`, this package's drawing script (JavaScript, sandboxed: no files or network). Edit it to change the shapes; each value's `glyph` field names the shape it uses.

Edit `framework.json` to rename, translate or add values, then *Framework > Reload frameworks*.
)HELP",
Glyphs::builtinSource()),

        make ("schaeffer",
R"PKG({
  "projet": "framework package",
  "id": "schaeffer",
  "version": 2,
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
  ]
}
)PKG",
R"HELP(# Schaeffer

**Type.** The typology of the *Traité des objets musicaux* (summary table, fig. 34): the mass of the sound down (definite pitch, complex and fixed, slightly varying, unpredictable), its sustainment across (from unpredictable facture to unpredictable facture, through held, impulse and iterative). The balanced types sit in the middle, the redundant ones beside them, the eccentric ones at the edges.

Later stages may add his dynamic profiles, the seven morphological criteria and his genres.
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
)HELP")
    };
    return list;
}
