#version 330

// Silhouettes of outlined models, paired with Lit.vs so they sway and move with what is drawn. Each writes the
// colour of its ring and, in alpha, how strongly it is ringed; Post.fs grows them by a few pixels and draws the
// ring where the grown shape leaves the silhouette.

in vec3  v_Tint;
in float v_Emissive;

layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = vec4(v_Tint, clamp(v_Emissive, 0.0, 1.0));
}
