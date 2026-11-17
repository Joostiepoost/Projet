// Projet - the energy glyphs (Vande Gorne), drawn by JavaScript.
//
// A package can draw the shapes of its values: a glyphs.js like this one,
// next to its framework.json. Define one object called glyphs; each entry is
// a function (w, h, rand, f) for one glyph id (the "glyph" field of a value;
// by default the value's id). It returns a list of strokes in a w by h band,
// y running downward; rand() is seeded, so a shape repeats exactly:
//
//   { pts: [x0,y0, x1,y1, ...] }   a line through the points, with
//       close: true (join the ends), fill: true (fill it), faint: true
//       (dimmer), width: 2 (thicker), alpha: 0.5
//   { dots: [x,y,r, x,y,r, ...] }  filled circles
//
// f (id, t) gives a variation factor of the box (stage 8) at t, from 0 at
// the start of the box to 1 at its end: a value from 0 to 1 (the factor's
// low end to its high end), its neutral value where no curve is drawn, and
// 0.5 for a factor the value does not have. The factors are declared in
// framework.json, on the category ("factors": definitions) and on each value
// ("factors": the ids it uses). A shape written before stage 8 simply leaves
// f out.
//
// Projet applies some factors itself, after the shape is made: the box's
// profile, or its "envelope" factor (dynamique) when one is drawn, scales
// the heights; a "shift" factor (espace) moves the shape up or down; a
// "depth" factor (profondeur) draws it smaller and fainter. Scripts can only
// compute shapes: no files, no network, and a shape that runs too long is
// stopped.

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
    // A factor as a multiplier: 1 at its middle, `range` at its high end,
    // 1 / range at its low end.
    function K (v, range) { return Math.pow (range, (v - 0.5) * 2); }
    function clamp (v, lo, hi) { return Math.max (lo, Math.min (hi, v)); }
    // A line cut into pieces, each as thick as the factor says where it is.
    function pieces (pts, w, widthAt, every)
    {
        var out = [], n = pts.length / 2, step = every || 8;
        for (var s = 0; s < n - 1; s += step)
        {
            var e = Math.min (n - 1, s + step), seg = pts.slice (s * 2, e * 2 + 2);
            out.push ({ pts: seg, width: widthAt ((pts[s * 2] + pts[e * 2]) / 2 / w) });
        }
        return out;
    }

    return {

    // Percussion-resonance: her own model: vertical attack, falling resonance.
    // attaque: a soft attack slopes in; entretien: a long sustain stays high
    // longer; halo: a rich harmonic halo draws lines above the resonance.
    "percussion-resonance": function (w, h, rand, f)
    {
        var top = h * 0.06, a = f ("attaque", 0), N = 24;
        var xa = a >= 0.5 ? 0 : w * 0.3 * (0.5 - a) * 2;
        function edge (u)
        {
            var x = xa + (w - xa) * u, p = Math.pow (3, (0.5 - f ("entretien", x / w)) * 2);
            return [x, top + (h - top) * Math.pow (u, p)];
        }
        var pts = [0, h, xa, top], i;
        for (i = 1; i <= N; ++i) { var e = edge (i / N); pts.push (e[0], e[1]); }
        var strokes = [ { pts: pts, close: true, fill: true, alpha: 0.8 } ];
        for (var k = 1; k <= 2; ++k)
        {
            var halo = [], any = false;
            for (i = 0; i <= N; ++i)
            {
                var p = edge (i / N), off = Math.max (0, f ("halo", p[0] / w) - 0.5) * 2 * h * 0.16 * k;
                any = any || off > 0.5;
                halo.push (p[0], Math.max (1, p[1] - off));
            }
            if (any) strokes.push ({ pts: halo, faint: true });
        }
        return strokes;
    },

    // Frottement-granulation: Klee's line and point: a line, then grain, then points.
    // vitesse: the line waves faster; grain: coarser grains and points;
    // iteratif: the grains and points come closer together.
    "frottement-granulation": function (w, h, rand, f)
    {
        var cy = h * 0.55, phase = [0], x;
        for (x = 1; x <= Math.ceil (w) + 8; ++x) phase.push (phase[x - 1] + K (f ("vitesse", x / w), 2.5) / 9);
        function y (x) { return cy + Math.sin (phase[Math.max (0, Math.min (phase.length - 1, Math.round (x)))]) * h * 0.06; }
        var line = [0, cy];
        for (x = 4; x <= w * 0.38; x += 4) line.push (x, y (x));
        var strokes = [ { pts: line } ];
        for (x = w * 0.4; x < w * 0.68; x += 3.2 / K (f ("iteratif", x / w), 2.2))
        {
            var g = K (f ("grain", x / w), 1.8), yy = y (x);
            strokes.push ({ pts: [x, yy, x + 1.8 * Math.sqrt (g), yy], width: 1.4 * g });
        }
        var dots = [];
        for (x = w * 0.7; x < w; x += (4 + rand() * 4) / K (f ("iteratif", x / w), 2.2))
            dots.push (x, cy + (rand() - 0.5) * h * 0.25, (0.8 + rand() * 1.4) * K (f ("grain", x / w), 1.8));
        strokes.push ({ dots: dots });
        return strokes;
    },

    // Accumulation de corpuscules: a cloud of grains.
    // densite: more grains; masse: a wider cloud; grain: bigger grains;
    // calibre: more difference between small and big ones.
    "accumulation-corpuscules": function (w, h, rand, f)
    {
        var dots = [], n = Math.max (12, Math.round (w * h / 55)) * 3;
        for (var i = 0; i < n; ++i)
        {
            var x = rand() * w, t = x / w, keep = rand() < K (f ("densite", t), 3) / 3;
            var spread = (Math.sin (Math.PI * x / w) * 0.45 + 0.15) * K (f ("masse", t), 1.8);
            var y = h / 2 + (rand() - 0.5) * 2 * spread * h;
            var r = (0.5 + rand() * rand() * 2.2 * K (f ("calibre", t), 2)) * K (f ("grain", t), 1.8);
            if (keep) dots.push (x, clamp (y, 1, h - 1), r);
        }
        return [ { dots: dots } ];
    },

    // Oscillation: two poles, fast, regular, mechanical.
    // vitesse: faster swings; force (de declenchement): wider apart;
    // frequence: higher up or lower down; densite: a thicker line;
    // spectre: a bright spectrum doubles the line.
    "oscillation": function (w, h, rand, f)
    {
        function centre (t) { return h / 2 - (f ("frequence", t) - 0.5) * h * 0.3; }
        function amp (t) { return Math.min (h * 0.47, h * 0.28 * K (f ("force", t), 1.35)); }
        var pts = [0, centre (0)], up = true, x = 4.5 / K (f ("vitesse", 0), 3) / 2;
        var bright = [], poleTop = [], poleBot = [];
        while (x <= w)
        {
            var t = x / w, c = centre (t), a = amp (t), y = up ? c - a : c + a;
            pts.push (x, y);
            var s = Math.max (0, f ("spectre", t) - 0.55) * 2;
            bright.push (x, y + (up ? 1 : -1) * s * h * 0.12);
            up = ! up;
            x += 4.5 / K (f ("vitesse", t), 3);
        }
        for (var i = 0; i <= 20; ++i)
        {
            var tt = i / 20, cc = centre (tt), aa = amp (tt);
            poleTop.push (w * tt, cc - aa);
            poleBot.push (w * tt, cc + aa);
        }
        var strokes = [ { pts: poleTop, faint: true }, { pts: poleBot, faint: true } ];
        strokes = strokes.concat (pieces (pts, w, function (t) { return K (f ("densite", t), 2); }));
        var anyBright = false;
        for (i = 0; i < w; i += 8) anyBright = anyBright || f ("spectre", i / w) > 0.55;
        if (anyBright) strokes.push ({ pts: bright, faint: true });
        return strokes;
    },

    // Balancement: slower, irregular swings that reach the poles.
    // As oscillation: vitesse, frequence, densite, spectre.
    "balancement": function (w, h, rand, f)
    {
        function centre (t) { return h / 2 - (f ("frequence", t) - 0.5) * h * 0.3; }
        var reach = h * 0.3, pts = [0, centre (0)], bright = [], x = 0, px = 0, up = true, anyBright = false;
        while (x < w)
        {
            var t = x / w, span = (26 + rand() * 30) / K (f ("vitesse", t), 3), a = 0.82 + rand() * 0.18;
            var c = centre (Math.min (1, (x + span / 2) / w));
            var y = c + (up ? -1 : 1) * reach * a;
            var nx = Math.min (w, x + span);
            var startY = pts[pts.length - 1];
            Q (pts, px, startY, x + span / 2, y, nx, centre (nx / w));
            var s = Math.max (0, f ("spectre", t) - 0.55) * 2;
            anyBright = anyBright || s > 0;
            Q (bright, px, startY, x + span / 2, y + (up ? 1 : -1) * s * h * 0.12, nx, centre (nx / w));
            px = nx; x += span; up = ! up;
        }
        var poleTop = [], poleBot = [];
        for (var i = 0; i <= 20; ++i) { poleTop.push (w * i / 20, centre (i / 20) - reach); poleBot.push (w * i / 20, centre (i / 20) + reach); }
        var strokes = [ { pts: poleTop, faint: true }, { pts: poleBot, faint: true } ];
        strokes = strokes.concat (pieces (pts, w, function (t) { return K (f ("densite", t), 2); }, 16));
        if (anyBright) strokes.push ({ pts: [0, centre (0)].concat (bright), faint: true });
        return strokes;
    },

    // Rebond: one pole; sharp peaks (concave sides), faster and lower each time.
    // vitesse: the bounces speed up sooner; halo: lines above the peaks;
    // matiere: a rougher, thicker line.
    "rebond": function (w, h, rand, f)
    {
        var base = h * 0.92, e = Math.pow (0.72, K (f ("vitesse", 0), 1.8)), x = 0, k = 0, pts = [0, base], halo = [0, base];
        var w0 = w * (1 - e) / (1 - Math.pow (e, 12)), anyHalo = false;
        while (k < 12 && x < w - 1)
        {
            var wk = w0 * Math.pow (e, k), hk = (base - h * 0.08) * Math.pow (e, 2 * k);
            var top = base - hk, xm = x + wk / 2;
            C (pts, x, base, x + wk * 0.3, base - hk * 0.08, xm - wk * 0.06, top + hk * 0.45, xm, top);
            C (pts, xm, top, xm + wk * 0.06, top + hk * 0.45, x + wk * 0.7, base - hk * 0.08, x + wk, base);
            var lift = Math.max (0, f ("halo", xm / w) - 0.5) * 2 * h * 0.14;
            anyHalo = anyHalo || lift > 0.5;
            C (halo, x, base, x + wk * 0.3, base - hk * 0.08 - lift, xm - wk * 0.06, top + hk * 0.45 - lift, xm, Math.max (1, top - lift));
            C (halo, xm, Math.max (1, top - lift), xm + wk * 0.06, top + hk * 0.45 - lift, x + wk * 0.7, base - hk * 0.08 - lift, x + wk, base);
            x += wk; ++k;
        }
        var rough = Math.max (0, f ("matiere", 0.5) - 0.5) * 2;
        if (rough > 0)
            for (var i = 3; i < pts.length; i += 2) pts[i] = Math.min (base, pts[i] + (rand() - 0.5) * rough * h * 0.08);
        var strokes = [ { pts: [0, base, w, base], faint: true } ];
        strokes = strokes.concat (pieces (pts, w, function (t) { return K (f ("matiere", t), 1.8); }, 20));
        if (anyHalo) strokes.push ({ pts: halo, faint: true });
        return strokes;
    },

    // Flux: a homogeneous ribbon, slowly changing.
    // densite: more lines inside, a denser fill; profilmasse: a wider ribbon.
    "flux": function (w, h, rand, f)
    {
        var N = 40, poly = [], back = [];
        var lines = Math.round (clamp (3 * K (f ("densite", 0.5), 2.2), 1, 7)), ks = [], lp = [], k;
        for (k = 0; k < lines; ++k) { ks.push (lines == 1 ? 0 : -0.5 + k / (lines - 1)); lp.push ([]); }
        for (var i = 0; i <= N; ++i)
        {
            var t = i / N, x = w * t, m = K (f ("profilmasse", t), 2.2);
            var thick = Math.min (h * 0.48, h * (0.22 + 0.13 * Math.sin (t * Math.PI * 1.3 + 0.4) + 0.05 * Math.sin (t * 9)) * m);
            var mid = h / 2 + h * 0.06 * Math.sin (t * Math.PI * 0.9);
            poly.push (x, mid - thick);
            back.push (x, mid + thick);
            for (k = 0; k < lines; ++k)
                lp[k].push (x, mid + ks[k] * Math.min (h * 0.9, h * (0.18 + 0.1 * Math.sin (t * Math.PI * 1.3 + 0.4)) * m));
        }
        for (var j = N; j >= 0; --j) poly.push (back[j * 2], back[j * 2 + 1]);
        var strokes = [ { pts: poly, close: true, fill: true, alpha: clamp (0.5 * K (f ("densite", 0.5), 1.4), 0.15, 0.9) } ];
        for (k = 0; k < lines; ++k) strokes.push ({ pts: lp[k], faint: true });
        return strokes;
    },

    // Pression-deformation: irregular swellings of a thin mass, mirrored below.
    // vitesse: quicker swellings; profilmasse: bigger ones; profilmelodique:
    // the mass rises or falls; envspectrale: a bright spectrum shows its core.
    "pression-deformation": function (w, h, rand, f)
    {
        var t0 = h * 0.045, top = h * 0.47, maxA = top - t0;
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
            var speed = K (f ("vitesse", Math.min (1, x / w)), 2.5), mass = K (f ("profilmasse", Math.min (1, x / w)), 1.6);
            var span = (22 + rand() * 58) / speed, a = 0.22 + rand() * 0.56, p = maxA * (0.2 + rand() * 0.8) * mass;
            bump (x, span, a, p);
            if (rand() < 0.45) bump (x + span * (0.1 + rand() * 0.6), span * (0.2 + rand() * 0.2), 0.3 + rand() * 0.4, maxA * (0.1 + rand() * 0.3) * mass);
            x += span * (0.75 + rand() * 0.35);                         // swellings may overlap
        }
        function cy (i) { return h * 0.5 - (f ("profilmelodique", i / w) - 0.5) * h * 0.5; }
        var pts = [], core = [], anyCore = false;
        for (i = 0; i <= n; i += 2)
        {
            var c = cy (i), lim = Math.min (c, h - c) - 0.5;
            pts.push (i, c - Math.min (amp[i], lim));
        }
        for (i = n - (n % 2); i >= 0; i -= 2)
        {
            var c2 = cy (i), lim2 = Math.min (c2, h - c2) - 0.5;
            pts.push (i, c2 + Math.min (amp[i], lim2));
        }
        for (i = 0; i <= n; i += 4) { core.push (i, cy (i)); anyCore = anyCore || f ("envspectrale", i / w) > 0.55; }
        var strokes = [ { pts: pts, close: true, fill: true, alpha: 0.85 } ];
        if (anyCore) strokes.push ({ pts: core, width: 1.4, alpha: 0.9 });
        return strokes;
    },

    // Rotation: loops at a constant rate.
    // vitesse: tighter loops; frequence: smaller loops (higher).
    "rotation": function (w, h, rand, f)
    {
        var cy = h / 2, pts = [], X = 0;
        for (var t = 0; ; t += 0.15)
        {
            var at = Math.min (1, X / w), b = Math.min (h * 0.47, h * 0.34 / K (f ("frequence", at), 1.5));
            var x = X + b * 0.7 - b * 0.7 * Math.cos (t + Math.PI / 2);
            if (x > w) break;
            pts.push (x, cy - b * Math.sin (t + Math.PI / 2));
            X += 22 / (2 * Math.PI) * 0.15 / K (f ("vitesse", at), 2.5);
        }
        return [ { pts: pts } ];
    },

    // Spirale: loops accelerating as they climb, tighter and smaller toward the top.
    // vitesse: more loops; frequence: higher; direction: low, it slows and falls instead.
    "spirale": function (w, h, rand, f)
    {
        var turns = Math.max (3, Math.round (w / 20 * K (f ("vitesse", 0.5), 2))), total = turns * 2 * Math.PI, K0 = 0.08;
        var lift = (f ("frequence", 0.5) - 0.5) * h * 0.25;
        var us = [], xc = 0, t;
        for (t = 0; t <= total; t += 0.12)
        {
            var p = t / total;
            us.push (xc, h * (0.27 - 0.14 * p), h * (0.72 - 0.48 * Math.pow (p, 1.6)) - lift, t);
            xc += Math.pow (K0, p) * 0.12 / (2 * Math.PI);
        }
        var m = us.length - 4, bEnd = us[m + 1];
        var S = (w - 1.4 * bEnd - 1) / Math.max (1e-6, us[m]);
        var falling = f ("direction", 0.5) < 0.5;
        var pts = [];
        for (var i = 0; i < us.length; i += 4)
        {
            var u = us[i], b = us[i + 1], cy = us[i + 2];
            t = us[i + 3];
            var x = u * S + 0.7 * b * (1 + Math.sin (t)), y = clamp (cy - b * Math.cos (t), 0, h);
            pts.push (x, falling ? h - y : y);
        }
        return [ { pts: pts } ];
    }

    };
})();
