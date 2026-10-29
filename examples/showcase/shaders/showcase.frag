#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Combined image sampler descriptor supplied by TextureDescriptorSet.
layout(set = 0, binding = 0) uniform sampler2D texSampler;

// Shared push constant block supplied per draw call.
layout(push_constant) uniform Push {
    vec4 tint;
    float time;
} push;

void main() {
    vec4 texel = texture(texSampler, fragTexCoord);

    // Mix vertex color and texture, then apply the push-constant tint.
    vec3 color = mix(fragColor, texel.rgb, 0.75f) * push.tint.rgb;
    outColor = vec4(color, texel.a);
}
