#version 330

// Depth-only pass into the sun's shadow map. Paired with Lit.vs, so swaying and
// breathing models cast shadows that move with them. Nothing is discarded here: a creature
// coming apart is left out of the pass whole (Renderer::DrawModel), so the early depth test holds.

void main()
{
}
