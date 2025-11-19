#version 330 core

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in float fragWaveHeight;

uniform vec3 waterColor;
uniform vec3 foamColor;
uniform float transparency;
uniform float time;

out vec4 fragColor;

// Simple hash function for noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Procedural foam pattern
float foam(vec2 uv, float waveHeight) {
    // Wave crest detection
    float crestFactor = smoothstep(0.3, 0.8, waveHeight);

    // Animated foam texture
    vec2 uv1 = uv * 3.0 + time * 0.1;
    vec2 uv2 = uv * 5.0 - time * 0.15;

    float noise1 = hash(floor(uv1));
    float noise2 = hash(floor(uv2));

    float foamPattern = noise1 * noise2;
    foamPattern = smoothstep(0.4, 0.6, foamPattern);

    return foamPattern * crestFactor;
}

void main() {
    // Light direction (sun)
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));

    // Diffuse lighting
    float diff = max(dot(fragNormal, lightDir), 0.0);
    diff = diff * 0.6 + 0.4; // Ambient + diffuse

    // Specular highlights
    vec3 viewDir = normalize(-fragPosition);
    vec3 reflectDir = reflect(-lightDir, fragNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    // Base water color with lighting
    vec3 color = waterColor * diff;

    // Add specular highlights
    color += vec3(0.8, 0.9, 1.0) * spec * 0.5;

    // Add foam on wave crests
    float foamAmount = foam(fragTexCoord, fragWaveHeight);
    color = mix(color, foamColor, foamAmount);

    // Fresnel effect (more reflective at grazing angles)
    float fresnel = pow(1.0 - max(dot(viewDir, fragNormal), 0.0), 3.0);
    color = mix(color, vec3(0.7, 0.8, 0.9), fresnel * 0.3);

    // Depth-based color variation
    float depthFactor = smoothstep(-5.0, 0.0, fragPosition.y);
    color = mix(color * 0.7, color, depthFactor);

    fragColor = vec4(color, transparency);
}