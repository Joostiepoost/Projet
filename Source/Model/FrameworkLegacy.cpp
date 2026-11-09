// FrameworkLegacy.cpp — the framework.json Projet 4a shipped.
//
// Kept to recognise 4a framework files that were never edited (they become
// the builtin packages as they are) and to check, in the tests, that the
// builtin packages carry everything 4a had. Stage 4b reads packages instead
// (FrameworkBuiltin.cpp).

#include "Framework.h"

juce::String Framework::legacyDefaultJson()
{
    // UTF-8 (accents, primes); juce::String would read a plain char* as ASCII.
    return juce::String::fromUTF8 (R"FRAMEWORK({
  "framework": "Projet",
  "version": 1,
  "language": "fr",
  "roleNames": "both",

  "families": [
    { "id": "materielle", "name": { "fr": "matérielle", "en": "material" },
      "colour": "#c49a4a", "dark": "#3a3220", "light": "#e3c58a" },
    { "id": "globale", "name": { "fr": "gestion globale", "en": "global management" },
      "colour": "#4ea597", "dark": "#1f403b", "light": "#8fd6c9" },
    { "id": "gestuelle", "name": { "fr": "gestuelle", "en": "gestural" },
      "colour": "#b9707e", "dark": "#3d2529", "light": "#e3a9b4" },
    { "id": "mouvement", "name": { "fr": "mouvement", "en": "movement" },
      "colour": "#5d82bd", "dark": "#2a3a55", "light": "#a9c3ec" },
    { "id": "loi", "name": { "fr": "loi", "en": "law" },
      "colour": "#8f7cc6", "dark": "#2e2a45", "light": "#c4b8ef" }
  ],

  "energies": [
    { "id": "percussion-resonance", "glyph": "percussion-resonance",
      "name": { "fr": "Percussion-résonance", "en": "Percussion-resonance" },
      "families": [ "materielle" ],
      "factors": { "fr": [ "dynamique", "halo harmonique", "attaque", "entretien" ],
                   "en": [ "dynamics", "harmonic halo", "attack", "sustainment" ] } },
    { "id": "frottement-granulation", "glyph": "frottement-granulation",
      "name": { "fr": "Frottement-granulation", "en": "Friction-granulation" },
      "families": [ "materielle", "gestuelle" ],
      "factors": { "fr": [ "vitesse", "grain", "entretien itératif" ],
                   "en": [ "speed", "grain", "iterative sustainment" ] } },
    { "id": "accumulation-corpuscules", "glyph": "accumulation-corpuscules",
      "name": { "fr": "Accumulation de corpuscules", "en": "Accumulation of corpuscles" },
      "families": [ "globale" ],
      "factors": { "fr": [ "grain", "masse", "calibre", "densité" ],
                   "en": [ "grain", "mass", "caliber", "density" ] } },
    { "id": "oscillation", "glyph": "oscillation",
      "name": { "fr": "Oscillation", "en": "Oscillation" },
      "families": [ "loi" ],
      "factors": { "fr": [ "force de déclenchement", "vitesse", "densité", "fréquence", "spectre", "espace G-D", "profondeur" ],
                   "en": [ "triggering force", "speed", "density", "frequency", "spectrum", "left-right space", "depth" ] } },
    { "id": "balancement", "glyph": "balancement",
      "name": { "fr": "Balancement", "en": "Swaying" },
      "families": [ "gestuelle" ],
      "factors": { "fr": [ "vitesse", "densité", "fréquence", "spectre", "espace G-D", "profondeur" ],
                   "en": [ "speed", "density", "frequency", "spectrum", "left-right space", "depth" ] } },
    { "id": "rebond", "glyph": "rebond",
      "name": { "fr": "Rebond", "en": "Bounce" },
      "families": [ "gestuelle", "loi" ],
      "factors": { "fr": [ "vitesse", "halo harmonique", "matière" ],
                   "en": [ "speed", "harmonic halo", "matter" ] } },
    { "id": "flux", "glyph": "flux",
      "name": { "fr": "Flux", "en": "Flow" },
      "families": [ "globale", "mouvement" ],
      "factors": { "fr": [ "densité", "profil de masse" ],
                   "en": [ "density", "mass profile" ] } },
    { "id": "pression-deformation", "glyph": "pression-deformation",
      "name": { "fr": "Pression-déformation", "en": "Pressure-deformation" },
      "families": [ "mouvement" ],
      "factors": { "fr": [ "vitesse", "profil de masse ou mélodique", "enveloppe spectrale" ],
                   "en": [ "speed", "mass or melodic profile", "spectral envelope" ] } },
    { "id": "rotation", "glyph": "rotation",
      "name": { "fr": "Rotation", "en": "Rotation" },
      "families": [ "mouvement" ],
      "factors": { "fr": [ "vitesse", "fréquence", "espace" ],
                   "en": [ "speed", "frequency", "space" ] } },
    { "id": "spirale", "glyph": "spirale",
      "name": { "fr": "Spirale", "en": "Spiral" },
      "families": [ "mouvement" ],
      "factors": { "fr": [ "vitesse", "fréquence", "espace", "direction de l'accélération" ],
                   "en": [ "speed", "frequency", "space", "direction of acceleration" ] } }
  ],

  "levels": [
    { "id": "ison", "order": 1, "fill": "solid", "colour": "#4c8fd6",
      "name": { "fr": "I-son", "en": "I-son" },
      "about": { "fr": "icône : la source telle quelle", "en": "icon: the source as it is" } },
    { "id": "dison", "order": 2, "fill": "hatched", "colour": "#49b3a4",
      "name": { "fr": "Di-son", "en": "Di-son" },
      "about": { "fr": "diagramme : la source comme modèle", "en": "diagram: the source as a model" } },
    { "id": "metonymic", "order": 3, "fill": "dotted", "colour": "#c0a050",
      "name": { "fr": "Métonymique", "en": "Metonymic" },
      "about": { "fr": "une partie ou un voisin pour le tout (Thoresen)", "en": "a part or a neighbour stands for the whole (Thoresen)" } },
    { "id": "meson", "order": 4, "fill": "outline", "colour": "#c27ab8",
      "name": { "fr": "Mé-son", "en": "Mé-son" },
      "about": { "fr": "métaphore : la source cachée ou arbitraire", "en": "metaphor: the source hidden or arbitrary" } }
  ],

  "roles": [
    { "id": "F", "letter": "F", "weight": 3,
      "thoresen": { "fr": "premier plan", "en": "foreground" },
      "vandeGorne": { "fr": "figure", "en": "figure" } },
    { "id": "M", "letter": "M", "weight": 2, "default": true,
      "thoresen": { "fr": "plan moyen", "en": "middleground" },
      "vandeGorne": { "fr": "entre les deux", "en": "in between" } },
    { "id": "B", "letter": "B", "weight": 1,
      "thoresen": { "fr": "arrière-plan", "en": "background" },
      "vandeGorne": { "fr": "fond", "en": "ground" } }
  ],

  "figureGround": {
    "figure": { "fr": [ "discontinu", "morphologique", "proche", "mobile", "varié", "mince", "fort", "aigu", "grain", "net", "intermittent" ],
                "en": [ "discontinuous", "morphological", "near", "mobile", "varied", "thin", "loud", "high", "grainy", "sharp", "intermittent" ] },
    "ground": { "fr": [ "continu", "harmonique", "loin", "immobile", "répétitif", "épais", "doux", "grave", "lisse", "flou", "récurrent" ],
                "en": [ "continuous", "harmonic", "far", "still", "repetitive", "thick", "soft", "low", "smooth", "blurred", "recurrent" ] }
  },

  "typology": {
    "masses": [
      { "id": "pitch",         "name": { "fr": "hauteur définie", "en": "definite pitch" } },
      { "id": "complex",       "name": { "fr": "complexe, fixe", "en": "complex, fixed" } },
      { "id": "varying",       "name": { "fr": "peu variable", "en": "slightly varying" } },
      { "id": "unpredictable", "name": { "fr": "variation imprévisible", "en": "unpredictable" } }
    ],
    "sustainments": [
      { "id": "unpredictable-held",  "name": { "fr": "facture imprévisible", "en": "unpredictable facture" } },
      { "id": "homogeneous-held",    "name": { "fr": "facture nulle (tenue)", "en": "no facture (held)" } },
      { "id": "held",                "name": { "fr": "tenu formé", "en": "formed held" } },
      { "id": "impulse",             "name": { "fr": "impulsion", "en": "impulse" } },
      { "id": "iterative",           "name": { "fr": "itératif formé", "en": "formed iterative" } },
      { "id": "homogeneous-iterative", "name": { "fr": "facture nulle (itérative)", "en": "no facture (iterative)" } },
      { "id": "unpredictable-iterative", "name": { "fr": "facture imprévisible", "en": "unpredictable facture" } }
    ],
    "zones": [
      { "id": "balanced",  "name": { "fr": "équilibré", "en": "balanced" },
        "colour": "#4ea597", "dark": "#1f403b", "light": "#8fd6c9" },
      { "id": "redundant", "name": { "fr": "redondant", "en": "redundant" },
        "colour": "#7a7d85", "dark": "#2b2d33", "light": "#c9c6bd" },
      { "id": "eccentric", "name": { "fr": "excentrique", "en": "eccentric" },
        "colour": "#8f7cc6", "dark": "#2e2a45", "light": "#c4b8ef" }
    ],
    "types": [
      { "id": "En",  "label": "(En)", "row": 0, "col": 0, "zone": "eccentric", "name": { "fr": "échantillon, tonique", "en": "sample, tonic" } },
      { "id": "Hn",  "label": "Hn",   "row": 0, "col": 1, "zone": "redundant", "name": { "fr": "son homogène tonique", "en": "tonic homogeneous sound" } },
      { "id": "N",   "label": "N",    "row": 0, "col": 2, "zone": "balanced",  "name": { "fr": "son tenu formé, tonique", "en": "formed held note, tonic" } },
      { "id": "N'",  "label": "N′",   "row": 0, "col": 3, "zone": "balanced",  "name": { "fr": "impulsion tonique", "en": "tonic impulse" } },
      { "id": "N''", "label": "N″",   "row": 0, "col": 4, "zone": "balanced",  "name": { "fr": "itératif formé, tonique", "en": "formed iterative note, tonic" } },
      { "id": "Zn",  "label": "Zn",   "row": 0, "col": 5, "zone": "redundant", "name": { "fr": "itératif homogène, tonique", "en": "tonic homogeneous iteration" } },
      { "id": "An",  "label": "(An)", "row": 0, "col": 6, "zone": "eccentric", "name": { "fr": "accumulation, tonique", "en": "accumulation, tonic" } },

      { "id": "Ex",  "label": "(Ex)", "row": 1, "col": 0, "zone": "eccentric", "name": { "fr": "échantillon, complexe", "en": "sample, complex" } },
      { "id": "Hx",  "label": "Hx",   "row": 1, "col": 1, "zone": "redundant", "name": { "fr": "son homogène complexe", "en": "complex homogeneous sound" } },
      { "id": "X",   "label": "X",    "row": 1, "col": 2, "zone": "balanced",  "name": { "fr": "son tenu formé, complexe", "en": "formed held note, complex" } },
      { "id": "X'",  "label": "X′",   "row": 1, "col": 3, "zone": "balanced",  "name": { "fr": "impulsion complexe", "en": "complex impulse" } },
      { "id": "X''", "label": "X″",   "row": 1, "col": 4, "zone": "balanced",  "name": { "fr": "itératif formé, complexe", "en": "formed iterative note, complex" } },
      { "id": "Zx",  "label": "Zx",   "row": 1, "col": 5, "zone": "redundant", "name": { "fr": "itératif homogène, complexe", "en": "complex homogeneous iteration" } },
      { "id": "Ax",  "label": "(Ax)", "row": 1, "col": 6, "zone": "eccentric", "name": { "fr": "accumulation, complexe", "en": "accumulation, complex" } },

      { "id": "Ey",  "label": "(Ey)", "row": 2, "col": 0, "zone": "eccentric", "name": { "fr": "échantillon, variable", "en": "sample, varying" } },
      { "id": "Tn",  "label": "Tn",   "row": 2, "col": 1, "zone": "redundant", "name": { "fr": "trame tonique", "en": "tonic weft" } },
      { "id": "Tx",  "label": "Tx",   "row": 2, "col": 1, "zone": "redundant", "name": { "fr": "trame complexe", "en": "complex weft" } },
      { "id": "Y",   "label": "Y",    "row": 2, "col": 2, "zone": "balanced",  "name": { "fr": "son tenu formé, variable", "en": "formed held note, varying" } },
      { "id": "Y'",  "label": "Y′",   "row": 2, "col": 3, "zone": "balanced",  "name": { "fr": "impulsion variable", "en": "varying impulse" } },
      { "id": "Y''", "label": "Y″",   "row": 2, "col": 4, "zone": "balanced",  "name": { "fr": "itératif formé, variable", "en": "formed iterative note, varying" } },
      { "id": "Zy",  "label": "Zy",   "row": 2, "col": 5, "zone": "redundant", "name": { "fr": "itératif homogène, variable (ostinato particulier)", "en": "varying homogeneous iteration (a particular ostinato)" } },
      { "id": "Ay",  "label": "(Ay)", "row": 2, "col": 6, "zone": "eccentric", "name": { "fr": "accumulation, variable", "en": "accumulation, varying" } },

      { "id": "E",   "label": "E",    "row": 3, "col": 0, "zone": "eccentric", "name": { "fr": "échantillon", "en": "sample" } },
      { "id": "T",   "label": "T",    "row": 3, "col": 1, "zone": "eccentric", "name": { "fr": "trame", "en": "weft" } },
      { "id": "W",   "label": "W",    "row": 3, "col": 2, "zone": "eccentric", "name": { "fr": "grosse note", "en": "large note" } },
      { "id": "Phi", "label": "Φ",    "row": 3, "col": 3, "zone": "eccentric", "name": { "fr": "fragment", "en": "fragment" } },
      { "id": "K",   "label": "K",    "row": 3, "col": 4, "zone": "eccentric", "name": { "fr": "cellule", "en": "cell" } },
      { "id": "P",   "label": "P",    "row": 3, "col": 5, "zone": "eccentric", "name": { "fr": "ostinato", "en": "ostinato" } },
      { "id": "A",   "label": "A",    "row": 3, "col": 6, "zone": "eccentric", "name": { "fr": "accumulation", "en": "accumulation" } }
    ]
  },

  "operations": {
    "status": "provisional: the structuring language of the RK1 research (26 October 2026); used from stage 8",
    "primitives": [
      { "id": "REPEAT",      "about": "Copy objects with selected attributes inherited",              "params": [ "source", "inherit", "override" ] },
      { "id": "TRANSPOSE",   "about": "Shift pitches in chromatic, diatonic or registral coordinates", "params": [ "field", "amount" ] },
      { "id": "INTERPOLATE", "about": "Insert new events between structural anchors",                  "params": [ "from", "to", "count" ] },
      { "id": "NEIGHBOUR",   "about": "Expand an anchor by departure and return",                     "params": [ "anchor", "field", "amount" ] },
      { "id": "SUBDIVIDE",   "about": "Replace one duration or event with shorter events",            "params": [ "parts" ] },
      { "id": "SUBSTITUTE",  "about": "Replace selected events or their attribute values",            "params": [ "target", "with" ] },
      { "id": "SUPERPOSE",   "about": "Combine processes in simultaneous or time-offset layers",      "params": [ "layers", "offset" ] },
      { "id": "CONCAT",      "about": "Concatenate sequences into larger structures",                 "params": [ "parts" ] },
      { "id": "ARPEGGIATE",  "about": "Linearize a collection of simultaneous pitches",               "params": [ "order", "spacing" ] },
      { "id": "INVERT",      "about": "Reflect pitch or interval directions around an axis",          "params": [ "field", "axis" ] }
    ],
    "macros": [
      { "id": "OSTINATO",               "expansion": "REPEAT + stable attribute template",            "onBoxes": true },
      { "id": "DIATONIC_RUN",           "expansion": "INTERPOLATE (diatonic)",                        "onBoxes": false, "alias": "CONNECT" },
      { "id": "DIATONIC_SEQUENCE",      "expansion": "REPEAT + systematic TRANSPOSE",                 "onBoxes": true },
      { "id": "ORNAMENT_CHORD",         "expansion": "chord-tone path + NEIGHBOUR / INTERPOLATE",     "onBoxes": false },
      { "id": "ARPEGGIATED_EXPANSION",  "expansion": "ARPEGGIATE + ornament",                         "onBoxes": false },
      { "id": "HARMONIC_PROJECTION",    "expansion": "two parts from one pitch collection",           "onBoxes": false },
      { "id": "HARMONIC_CONSOLIDATION", "expansion": "earlier linear pitches recombined as a sonority", "onBoxes": false },
      { "id": "CHROMATIC_DESCENT",      "expansion": "REPEAT + TRANSPOSE (-1 semitone)",              "onBoxes": false },
      { "id": "IMITATE",                "expansion": "REPEAT + time offset + TRANSPOSE",              "onBoxes": true },
      { "id": "ROLE_EXCHANGE",          "expansion": "SUBSTITUTE of active and supporting roles",     "onBoxes": true },
      { "id": "CADENTIAL_SUBSTITUTION", "expansion": "REPEAT + SUBSTITUTE (ending)",                  "onBoxes": true },
      { "id": "VARIED_RECAPITULATION",  "expansion": "REPEAT with selective overrides",               "onBoxes": true },
      { "id": "ARTICULATED_GESTURE",    "expansion": "rhythmic profile + articulation pattern",       "onBoxes": true },
      { "id": "CLOSING_GESTURE",        "expansion": "recurrent closing rhythmic profile",            "onBoxes": true },
      { "id": "DYNAMIC_REVERSAL",       "expansion": "SUBSTITUTE (permuted dynamic values)",          "onBoxes": true },
      { "id": "SUSTAINED_DYAD",         "expansion": "sustained layer, tied continuations",           "onBoxes": true }
    ]
  }
}
)FRAMEWORK");
}
