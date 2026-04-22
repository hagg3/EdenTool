#pragma once

#include <cstdint>

class Noise2D {
public:
    explicit Noise2D(uint32_t seed);
    // Returns approximately [-1, 1].
    double fractal(double x, double z, int octaves, double baseFrequency, double persistence) const;
    // Returns approximately [-1, 1].
    double fractal3D(double x, double y, double z, int octaves, double baseFrequency, double persistence) const;

private:
    uint32_t seed;
    double valueNoise(double x, double z) const;
    double valueNoise3D(double x, double y, double z) const;
    static double smoothstep(double t);
    static double lerp(double a, double b, double t);
    double lattice(int x, int z) const;
    double lattice3D(int x, int y, int z) const;
};
