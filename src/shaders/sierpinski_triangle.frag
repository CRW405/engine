#version 450

layout(location = 0) in vec2 fragCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    int n;
    float zoom;
    float aspect_ratio;
} push;

void main() {
    int n = push.n;

    // Map NDC screen space [-1, 1] to fractal coordinate space [0, 1]
    vec2 centered = (fragCoord + 1.0) * 0.5 - vec2(0.5);
    centered.x *= push.aspect_ratio;
    vec2 c = centered / push.zoom + vec2(0.5);
    bool solid = true;

    // Fractal transformation loop (IFS)
    for (int i = 0; i < n; i++) {
        c *= 2.0;

        // Mutually exclusive quadrant checks
        if (c.x > 1.0 && c.y > 1.0) {
            solid = false; // Central inverted triangle hole
        } else if (c.x > 1.0) {
            c.x -= 1.0;    // Bottom-right sub-triangle
        } else if (c.y > 1.0) {
            c.y -= 1.0;    // Top-left sub-triangle
        }
    }

    // Color and boundary output
    if (!solid || c.x < 0.0 || c.y < 0.0 || (c.x + c.y > 1.0)) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0); // Background / Hole
    } else {
        outColor = vec4(1.0, 1.0, 1.0, 1.0); // Fractal shape color
    }
}
