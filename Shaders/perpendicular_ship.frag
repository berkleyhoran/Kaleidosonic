// Perpendicular Ship — z = (Re z + i|Im z|)^2 + c: the Burning Ship's
// abs() fold applied to the imaginary part only, which shears the ship
// into the famous asymmetric "perpendicular" variant with its hidden
// dragon-like structures. Same manual navigation as the other explorers.

void main()
{
    fragColor = vec4(exploreFractal(2, -1.0), 1.0);
}
