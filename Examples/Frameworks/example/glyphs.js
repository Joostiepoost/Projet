// The example package's drawing script. A package can draw the shapes of its
// values: define one object called glyphs; each entry is a function
// (w, h, rand) for one glyph id (the "glyph" field of a value). It returns a
// list of strokes in a w by h band, y running downward; rand() is seeded, so
// a shape repeats exactly:
//
//   { pts: [x0,y0, x1,y1, ...] }   a line through the points, with
//       close: true (join the ends), fill: true (fill it), faint: true
//       (dimmer), width: 2 (thicker), alpha: 0.5
//   { dots: [x,y,r, x,y,r, ...] }  filled circles
//
// The box's profile (the intensity envelope) is applied by Projet afterwards.
// Scripts can only compute shapes: no files, no network, and a shape that
// runs too long is stopped.

var glyphs = {

    // A pulse train for the added energy: regular verticals on a base line.
    "example-pulse": function (w, h, rand)
    {
        var base = h * 0.82, strokes = [ { pts: [0, base, w, base], faint: true } ];
        for (var x = 6; x < w - 2; x += 15)
            strokes.push ({ pts: [x, base, x, h * (0.18 + rand() * 0.1)], width: 3 });
        return strokes;
    }
};
