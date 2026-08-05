// Burning Ship Explorer — z = (|Re z| + i|Im z|)^2 + c, rendered plainly
// in its classic orientation (imaginary axis down, so the "ship" reads
// right side up) with sharp distance-estimate shading. Manual navigation:
// wheel/arrows zoom, drag pans -- see exploreFractal in common.glsl.

void main()
{
    fragColor = vec4(exploreFractal(1, -1.0), 1.0);
}
