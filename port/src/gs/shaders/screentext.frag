#version 450
// Preserve the scene, including its depth and exact alpha, before composing UI
// at display resolution. Only the visible 512x448 part of the GS buffer is used.
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAux;
layout(set = 2, binding = 0) uniform sampler2D sceneColor;
layout(set = 2, binding = 1) uniform sampler2D sceneAux;
layout(set = 2, binding = 2) uniform sampler2D sceneDepth;
void main() {
    vec2 uv = vUv * vec2(512.0 / 1024.0, 448.0 / 1024.0);
    outColor = texture(sceneColor, uv);
    outAux = texture(sceneAux, uv);
    gl_FragDepth = texture(sceneDepth, uv).r;
}
