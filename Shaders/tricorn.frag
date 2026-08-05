// Tricorn (Mandelbar) — z = conj(z)^2 + c: squaring the conjugate instead
// of z itself gives a three-cornered set whose spikes sprout smaller
// tricorns in threefold symmetry. Same manual navigation as the other
// explorers.

void main()
{
    fragColor = vec4(exploreFractal(4, 1.0), 1.0);
}
