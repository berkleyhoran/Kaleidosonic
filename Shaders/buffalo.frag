// Buffalo — |Re(w^2)| + i Im(w^2) + c with w = |Re z| + i|Im z|: the
// Burning Ship's fold plus a post-square abs on the real part, producing
// the squat, horned "buffalo" silhouette with celtic-knot style boundary
// detail. Same manual navigation as the other explorers.

void main()
{
    fragColor = vec4(exploreFractal(3, -1.0), 1.0);
}
