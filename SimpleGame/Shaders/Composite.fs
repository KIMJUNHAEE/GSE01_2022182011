#version 330

// Final post-process pass: combines the sharp scene with its own blurred
// version to fake three things at once with a single blur texture:
//   1. A soft HDR-ish glow/bloom around bright areas (lights, the mark, UI)
//   2. Progressively blurry screen edges (vignette blur)
//   3. Darkened corners (vignette darkening)
// plus a cheap Reinhard tonemap for a bit of cinematic highlight rolloff.

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Scene;
uniform sampler2D u_Blurred;
uniform float u_BloomIntensity;
uniform float u_VignetteStrength;
uniform float u_VignetteBlur;

void main()
{
	vec3 scene = texture(u_Scene, v_TexCoord).rgb;
	vec3 blurred = texture(u_Blurred, v_TexCoord).rgb;

	float dist = distance(v_TexCoord, vec2(0.5));
	float vignette = smoothstep(0.25, 0.75, dist); // 0 at center, 1 toward corners

	// Blend toward the blurred image as we approach the edges.
	vec3 color = mix(scene, blurred, vignette * u_VignetteBlur);

	// Additive glow, weighted by the blurred sample's own brightness so it
	// concentrates around already-bright spots instead of washing out everything.
	float blurredLuma = dot(blurred, vec3(0.2126, 0.7152, 0.0722));
	color += blurred * blurredLuma * u_BloomIntensity;

	// Cheap Reinhard-style tonemap, then re-lift midtones (Reinhard alone dims everything).
	color = color / (color + vec3(0.85));
	color *= 1.35;

	// Darken toward the edges.
	color *= (1.0 - vignette * u_VignetteStrength);

	FragColor = vec4(color, 1.0);
}
