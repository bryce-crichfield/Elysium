
// Linear gradient across the shape's box, clipped to the shape.
uniform vec4 uColorA; // default: 1 1 1 1
uniform vec4 uColorB; // default: 0 0 0 1
uniform float uAngle; // default: 90

vec4 Shade(float sd, vec2 p, vec2 uv)
{
    float a = radians(uAngle);
    vec2 dir = vec2(cos(a), sin(a));
    float t = clamp(dot(uv - 0.5, dir) + 0.5, 0.0, 1.0);
    vec4 color = mix(uColorA, uColorB, t);
    return vec4(color.rgb, color.a * Coverage(sd));
}
