// GlyphsBuiltin.cpp — Projet's own glyph shapes: the ten energy models
// (Vande Gorne), as revised 9 November (rebond with sharp peaks,
// pression-déformation mirrored below, spirale accelerating toward the top).
// This source is also written into the composition package as its glyphs.js,
// so its comment doubles as the guide for writing your own.

#include "Glyphs.h"

juce::String Glyphs::builtinSource()
{
    return juce::String::fromUTF8 (R"JSGLYPHS(// Projet - the energy glyphs (Vande Gorne), drawn by JavaScript.
//
// A package can draw the shapes of its values: a glyphs.js like this one,
// next to its framework.json. Define one object called glyphs; each entry is
// a function (w, h, rand) for one glyph id (the "glyph" field of a value; by
// default the value's id). It returns a list of strokes in a w by h band,
// y running downward; rand() is seeded, so a shape repeats exactly:
//
//   { pts: [x0,y0, x1,y1, ...] }   a line through the points, with
//       close: true (join the ends), fill: true (fill it), faint: true
//       (dimmer), width: 2 (thicker), alpha: 0.5
//   { dots: [x,y,r, x,y,r, ...] }  filled circles
//
// The box's profile (the intensity envelope) is applied by Projet afterwards,
// so a shape need not handle it. Scripts can only compute shapes: no files,
// no network, and a shape that runs too long is stopped.

var glyphs = (function()
{
    // A quadratic and a cubic curve, sampled into the polyline.
    function Q (pts, x0, y0, cx, cy, x1, y1)
    {
        for (var i = 1; i <= 8; ++i)
        {
            var t = i / 8, u = 1 - t;
            pts.push (u*u*x0 + 2*u*t*cx + t*t*x1, u*u*y0 + 2*u*t*cy + t*t*y1);
        }
    }
    function C (pts, x0, y0, c1x, c1y, c2x, c2y, x1, y1)
    {
        for (var i = 1; i <= 10; ++i)
        {
            var t = i / 10, u = 1 - t;
            pts.push (u*u*u*x0 + 3*u*u*t*c1x + 3*u*t*t*c2x + t*t*t*x1,
                      u*u*u*y0 + 3*u*u*t*c1y + 3*u*t*t*c2y + t*t*t*y1);
        }
    }

    return {

    // Percussion-resonance: her own model: vertical attack, falling resonance.
    "percussion-resonance": function (w, h)
    {
        return [ { pts: [0, h, 0, h * 0.06, w, h], close: true, fill: true, alpha: 0.8 } ];
    },

    // Frottement-granulation: Klee's line and point: a line, then grain, then points.
    "frottement-granulation": function (w, h, rand)
    {
        var cy = h * 0.55, line = [0, cy], x;
        for (x = 4; x <= w * 0.38; x += 4) line.push (x, cy + Math.sin (x / 9) * h * 0.06);
        var strokes = [ { pts: line } ];
        for (x = w * 0.4; x < w * 0.68; x += 3.2)
        {
            var y = cy + Math.sin (x / 9) * h * 0.06;
            strokes.push ({ pts: [x, y, x + 1.8, y], width: 1.4 });
        }
        var dots = [];
        for (x = w * 0.7; x < w; x += 4 + rand() * 4)
            dots.push (x, cy + (rand() - 0.5) * h * 0.25, 0.8 + rand() * 1.4);
        strokes.push ({ dots: dots });
        return strokes;
    },

    // Accumulation de corpuscules: a cloud of grains.
    "accumulation-corpuscules": function (w, h, rand)
    {
        var dots = [], n = Math.max (12, Math.round (w * h / 55));
        for (var i = 0; i < n; ++i)
        {
            var x = rand() * w, spread = Math.sin (Math.PI * x / w) * 0.45 + 0.15;
            var y = h / 2 + (rand() - 0.5) * 2 * spread * h;
            dots.push (x, Math.min (h - 1, Math.max (1, y)), 0.5 + rand() * rand() * 2.2);
        }
        return [ { dots: dots } ];
    },

    // Oscillation: two poles, fast, regular, mechanical.
    "oscillation": function (w, h)
    {
        var top = h * 0.22, bot = h * 0.78, step = 4.5, pts = [0, h / 2], up = true;
        for (var x = step / 2; x <= w; x += step) { pts.push (x, up ? top : bot); up = ! up; }
        return [ { pts: [0, top, w, top], faint: true },
                 { pts: [0, bot, w, bot], faint: true },
                 { pts: pts } ];
    },

    // Balancement: slower, irregular swings that reach the poles.
    "balancement": function (w, h, rand)
    {
        var top = h * 0.2, bot = h * 0.8, pts = [0, h / 2], x = 0, px = 0, up = true;
        while (x < w)
        {
            var span = 26 + rand() * 30, amp = 0.82 + rand() * 0.18;
            var y = h / 2 + (up ? -1 : 1) * (h / 2 - top) * amp;
            var nx = Math.min (w, x + span);
            Q (pts, px, h / 2, x + span / 2, y, nx, h / 2);
            px = nx; x += span; up = ! up;
        }
        return [ { pts: [0, top, w, top], faint: true },
                 { pts: [0, bot, w, bot], faint: true },
                 { pts: pts } ];
    },

    // Rebond: one pole; sharp peaks (concave sides), faster and lower each time.
    "rebond": function (w, h)
    {
        var base = h * 0.92, e = 0.72, x = 0, k = 0, pts = [0, base];
        var w0 = w * (1 - e) / (1 - Math.pow (e, 12));
        while (k < 12 && x < w - 1)
        {
            var wk = w0 * Math.pow (e, k), hk = (base - h * 0.08) * Math.pow (e, 2 * k);
            var top = base - hk, xm = x + wk / 2;
            C (pts, x, base, x + wk * 0.3, base - hk * 0.08, xm - wk * 0.06, top + hk * 0.45, xm, top);
            C (pts, xm, top, xm + wk * 0.06, top + hk * 0.45, x + wk * 0.7, base - hk * 0.08, x + wk, base);
            x += wk; ++k;
        }
        return [ { pts: [0, base, w, base], faint: true }, { pts: pts } ];
    },

    // Flux: a homogeneous ribbon, slowly changing.
    "flux": function (w, h)
    {
        var N = 40, poly = [], back = [], lines = [[], [], []], ks = [-0.5, 0, 0.5];
        for (var i = 0; i <= N; ++i)
        {
            var t = i / N, x = w * t;
            var thick = h * (0.22 + 0.13 * Math.sin (t * Math.PI * 1.3 + 0.4) + 0.05 * Math.sin (t * 9));
            var mid = h / 2 + h * 0.06 * Math.sin (t * Math.PI * 0.9);
            poly.push (x, mid - thick);
            back.push (x, mid + thick);
            for (var k = 0; k < 3; ++k)
                lines[k].push (x, mid + ks[k] * h * (0.18 + 0.1 * Math.sin (t * Math.PI * 1.3 + 0.4)));
        }
        for (var j = N; j >= 0; --j) poly.push (back[j * 2], back[j * 2 + 1]);
        return [ { pts: poly, close: true, fill: true, alpha: 0.5 },
                 { pts: lines[0], faint: true }, { pts: lines[1], faint: true }, { pts: lines[2], faint: true } ];
    },

    // Pression-deformation: irregular swellings of a thin mass, mirrored below.
    "pression-deformation": function (w, h, rand)
    {
        var cy = h * 0.5, t0 = h * 0.045, top = h * 0.47, maxA = top - t0;
        var n = Math.ceil (w), amp = [], i;
        for (i = 0; i <= n; ++i) amp.push (t0);
        function bump (x0, span, a, p)
        {
            for (var j = Math.max (0, Math.floor (x0)); j <= Math.min (n, Math.ceil (x0 + span)); ++j)
            {
                var t = (j - x0) / span;
                if (t < 0 || t > 1) continue;
                var v = t < a ? (1 - Math.cos (Math.PI * t / a)) / 2 : (1 + Math.cos (Math.PI * (t - a) / (1 - a))) / 2;
                amp[j] = Math.min (top, amp[j] + p * v);
            }
        }
        var x = 0;
        while (x < w)
        {
            if (rand() < 0.35) x += 4 + rand() * 14;                    // a moment of rest
            var span = 22 + rand() * 58, a = 0.22 + rand() * 0.56, p = maxA * (0.2 + rand() * 0.8);
            bump (x, span, a, p);
            if (rand() < 0.45) bump (x + span * (0.1 + rand() * 0.6), span * (0.2 + rand() * 0.2), 0.3 + rand() * 0.4, maxA * (0.1 + rand() * 0.3));
            x += span * (0.75 + rand() * 0.35);                         // swellings may overlap
        }
        var pts = [];
        for (i = 0; i <= n; i += 2) pts.push (i, cy - amp[i]);
        for (i = n - (n % 2); i >= 0; i -= 2) pts.push (i, cy + amp[i]);
        return [ { pts: pts, close: true, fill: true, alpha: 0.85 } ];
    },

    // Rotation: loops at a constant rate.
    "rotation": function (w, h)
    {
        var spacing = 22, a = spacing / (2 * Math.PI), b = h * 0.34, cy = h / 2, pts = [];
        for (var t = 0; ; t += 0.15)
        {
            var x = a * t + b * 0.7 - b * 0.7 * Math.cos (t + Math.PI / 2);
            if (x > w) break;
            pts.push (x, cy - b * Math.sin (t + Math.PI / 2));
        }
        return [ { pts: pts } ];
    },

    // Spirale: loops accelerating as they climb, tighter and smaller toward the top.
    "spirale": function (w, h)
    {
        var turns = Math.max (3, Math.round (w / 20)), total = turns * 2 * Math.PI, K = 0.08;
        var us = [], xc = 0, t;
        for (t = 0; t <= total; t += 0.12)
        {
            var p = t / total;
            us.push (xc, h * (0.27 - 0.14 * p), h * (0.72 - 0.48 * Math.pow (p, 1.6)), t);
            xc += Math.pow (K, p) * 0.12 / (2 * Math.PI);
        }
        var m = us.length - 4, bEnd = us[m + 1];
        var S = (w - 1.4 * bEnd - 1) / Math.max (1e-6, us[m]);
        var pts = [];
        for (var i = 0; i < us.length; i += 4)
        {
            var u = us[i], b = us[i + 1], cy = us[i + 2];
            t = us[i + 3];
            pts.push (u * S + 0.7 * b * (1 + Math.sin (t)), cy - b * Math.cos (t));
        }
        return [ { pts: pts } ];
    }

    };
})();
)JSGLYPHS");
}
