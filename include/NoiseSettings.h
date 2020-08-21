#ifndef NOISESETTINGS_H
#define NOISESETTINGS_H

struct NoiseSettings
{
    float noiseScale = 2.5f;
    float lacunarity = 2.f;
    float persistence = 0.35f;
    int octaves = 5;

    bool closeEdges = false;

    float stepSize = 8.f;
    float stepWeight = 0.5f;

    float floorOffset = 10.f;
    float hardFloor = 2.f;
    float floorWeight = 100.f;

    float noiseWeight = 40.f;

    struct {
        float x = 0.f, y = 0.f, z = 0.f;
    } offset;

    int offsetSeed = 0;

    void seed(int newOffsetSeed);
};

#endif // NOISESETTINGS_H
