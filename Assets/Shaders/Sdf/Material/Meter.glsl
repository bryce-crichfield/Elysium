
// Progress meter clipped to the shape: cooldowns, cast bars, HP rings. uMode 0 fills
// left -> right across the box, 1 fills clockwise from 12 o'clock. uHole (0..1) cuts
// the centre out of radial mode to make a ring. The leading edge glows. uAutoSpeed > 0
// loops progress on its own (demo/idle). uFlash (0..1) washes the span uFlashFrom..uFlashTo
// (of the bar, left -> right mode only) white: the chunk just gained or lost, fading out.
uniform float uProgress; // default: 0.65
uniform float uMode; // default: 0
uniform float uHole; // default: 0
uniform float uAutoSpeed; // default: 0
uniform vec4 uFillColor; // default: 0.3 0.85 1 1
uniform vec4 uTrackColor; // default: 0.08 0.1 0.16 0.85
uniform float uEdgeGlow; // default: 0.8
uniform float uFlash; // default: 0
uniform float uFlashFrom; // default: 0
uniform float uFlashTo; // default: 0

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float progress = uAutoSpeed > 0.0 ? fract(e_Time * uAutoSpeed) : clamp(uProgress, 0.0, 1.0);

    float filled;       // 0..1 anti-aliased coverage of the filled part
    float toFront;      // world-unit distance to the leading edge
    float shapeMask = Coverage(sd);

    if (uMode < 0.5)
    {
        float x = uv.x * e_Size.x;
        toFront = progress * e_Size.x - x;
        filled = clamp(toFront / max(fwidth(x), 1e-4) + 0.5, 0.0, 1.0);
    }
    else
    {
        // Angle from 12 o'clock, clockwise on screen (y down), as 0..1 of a turn.
        float turn = fract(atan(p.x, -p.y) / 6.2831853 + 1.0);
        float circumference = 6.2831853 * max(length(p), 1e-3);
        // Arc distances to the start seam and to the front, in world units, so both
        // edges anti-alias at about one pixel regardless of radius.
        float fromStart = turn * circumference;
        toFront = (progress - turn) * circumference;
        filled = clamp(min(fromStart, toFront) + 0.5, 0.0, 1.0) * step(0.0001, progress);

        float hole = uHole * min(e_Size.x, e_Size.y) * 0.5;
        shapeMask *= Coverage(hole - length(p));
    }

    vec4 color = mix(uTrackColor, uFillColor, filled);
    color.rgb += uFillColor.rgb * uEdgeGlow * exp(-abs(toFront) / 6.0) * filled;
    if (uFlash > 0.0 && uMode < 0.5)
    {
        float x = uv.x * e_Size.x;
        float aa = max(fwidth(x), 1e-4);
        float inside = clamp((x - uFlashFrom * e_Size.x) / aa + 0.5, 0.0, 1.0)
                     * clamp((uFlashTo * e_Size.x - x) / aa + 0.5, 0.0, 1.0);
        color = mix(color, vec4(1.0), inside * clamp(uFlash, 0.0, 1.0));
    }
    return vec4(color.rgb, color.a * shapeMask);
}
