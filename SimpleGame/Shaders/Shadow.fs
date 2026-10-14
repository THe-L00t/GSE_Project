#version 330

// Depth-only pass into the sun's shadow map. Paired with Lit.vs, so swaying and
// breathing models cast shadows that move with them.

in float v_Dissolve;

void main()
{
	// A creature coming apart stops casting once half of it is light.
	if (v_Dissolve > 0.5) discard;
}
