#include "Noise.h"

#include <cmath>

Noise2D::Noise2D(uint32_t seed) : seed(seed) {}

double Noise2D::smoothstep(double t) {
    return t * t * (3.0 - 2.0 * t);
}

double Noise2D::lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

double Noise2D::lattice(int x, int z) const {
    uint32_t h = (uint32_t)x * 374761393u;
    h ^= (uint32_t)z * 668265263u;
    h ^= seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return ((double)(h & 0xFFFFFF) / (double)0x7FFFFF) - 1.0;
}

double Noise2D::valueNoise(double x, double z) const {
    int x0 = (int)std::floor(x);
    int z0 = (int)std::floor(z);
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    double tx = smoothstep(x - (double)x0);
    double tz = smoothstep(z - (double)z0);

    double v00 = lattice(x0, z0);
    double v10 = lattice(x1, z0);
    double v01 = lattice(x0, z1);
    double v11 = lattice(x1, z1);

    double vx0 = lerp(v00, v10, tx);
    double vx1 = lerp(v01, v11, tx);
    return lerp(vx0, vx1, tz);
}

double Noise2D::lattice3D(int x, int y, int z) const {
    uint32_t h = (uint32_t)x * 374761393u;
    h ^= (uint32_t)y * 3266489917u;
    h ^= (uint32_t)z * 668265263u;
    h ^= seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return ((double)(h & 0xFFFFFF) / (double)0x7FFFFF) - 1.0;
}

double Noise2D::valueNoise3D(double x, double y, double z) const {
    int x0 = (int)std::floor(x);
    int y0 = (int)std::floor(y);
    int z0 = (int)std::floor(z);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;
    double tx = smoothstep(x - (double)x0);
    double ty = smoothstep(y - (double)y0);
    double tz = smoothstep(z - (double)z0);

    double c000 = lattice3D(x0, y0, z0);
    double c100 = lattice3D(x1, y0, z0);
    double c010 = lattice3D(x0, y1, z0);
    double c110 = lattice3D(x1, y1, z0);
    double c001 = lattice3D(x0, y0, z1);
    double c101 = lattice3D(x1, y0, z1);
    double c011 = lattice3D(x0, y1, z1);
    double c111 = lattice3D(x1, y1, z1);

    double x00 = lerp(c000, c100, tx);
    double x10 = lerp(c010, c110, tx);
    double x01 = lerp(c001, c101, tx);
    double x11 = lerp(c011, c111, tx);
    double y0v = lerp(x00, x10, ty);
    double y1v = lerp(x01, x11, ty);
    return lerp(y0v, y1v, tz);
}

double Noise2D::fractal(double x, double z, int octaves, double baseFrequency, double persistence) const {
    double amplitude = 1.0;
    double frequency = baseFrequency;
    double total = 0.0;
    double norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        total += valueNoise(x * frequency, z * frequency) * amplitude;
        norm += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    if (norm <= 0.0) return 0.0;
    return total / norm;
}

double Noise2D::fractal3D(double x, double y, double z, int octaves, double baseFrequency, double persistence) const {
    double amplitude = 1.0;
    double frequency = baseFrequency;
    double total = 0.0;
    double norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        total += valueNoise3D(x * frequency, y * frequency, z * frequency) * amplitude;
        norm += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    if (norm <= 0.0) return 0.0;
    return total / norm;
}
