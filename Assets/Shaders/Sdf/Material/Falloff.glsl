// Radial falloff from the shape's center to its box edge, clipped to the shape: a light,
// or a vision cutout on a Multiply-composited layer (fog of war). On an Ellipse, 2:1
// radii give the isometric look.
uniform vec4 uColor; // default: 1 1 1 1
uniform float uHardness; // default: 0

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float r = length(uv - 0.5) * 2.0;  // 0 at the center, 1 at the box edge
    float a = 1.0 - smoothstep(min(uHardness, 0.999), 1.0, r);
    return vec4(uColor.rgb, uColor.a * a * Coverage(sd));
}
