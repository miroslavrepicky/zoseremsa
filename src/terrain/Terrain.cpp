#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include <random>

#include <shaders/terrain_vert_glsl.h>
#include <shaders/terrain_frag_glsl.h>

// Static member initialization
std::unique_ptr<ppgso::Shader> Terrain::shader;
int Terrain::instanceCount = 0;
std::vector<int> Terrain::permutation;

Terrain::Terrain(int resolution, float size, float height, TerrainType type)
        : resolution(resolution), size(size), maxHeight(height), type(type) {

    instanceCount++;

    if (!shader) {
        shader = std::make_unique<ppgso::Shader>(terrain_vert_glsl, terrain_frag_glsl);
    }

    if (permutation.empty()) {
        permutation.resize(512);
        std::vector<int> p(256);
        for (int i = 0; i < 256; i++) p[i] = i;

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(p.begin(), p.end(), g);

        for (int i = 0; i < 256; i++) {
            permutation[i] = p[i];
            permutation[256 + i] = p[i];
        }
    }

    initVoronoiCells();
    generateGrid();
    computeNormals();

    // Setup OpenGL buffers
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2),
                 uvs.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    indexCount = indices.size();
}

Terrain::~Terrain() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &nbo);
    glDeleteBuffers(1, &tbo);
    glDeleteBuffers(1, &ebo);

    instanceCount--;

    if (instanceCount == 0) {
        shader.reset();
        permutation.clear();
    }
}

void Terrain::update(float) {}

void Terrain::render(const glm::mat4 &view, const glm::mat4 &projection) {
    shader->use();
    shader->setUniform("modelMatrix", glm::mat4(1.f));
    shader->setUniform("viewMatrix", view);
    shader->setUniform("projectionMatrix", projection);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

// ===================== Perlin Noise Implementation =========================

float Terrain::fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float Terrain::lerp(float t, float a, float b) {
    return a + t * (b - a);
}

float Terrain::grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Terrain::perlin(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    x -= floor(x);
    y -= floor(y);

    float u = fade(x);
    float v = fade(y);

    int A  = permutation[X] + Y;
    int AA = permutation[A];
    int AB = permutation[A + 1];
    int B  = permutation[X + 1] + Y;
    int BA = permutation[B];
    int BB = permutation[B + 1];

    return lerp(v,
        lerp(u, grad(permutation[AA], x, y),
                grad(permutation[BA], x - 1, y)),
        lerp(u, grad(permutation[AB], x, y - 1),
                grad(permutation[BB], x - 1, y - 1))
    );
}

// ===================== Noise Algorithms =========================

float Terrain::fbm(float x, float y, int octaves) {
    float value = 0.f;
    float amplitude = 0.5f;
    float frequency = noiseFrequency;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * perlin(x * frequency, y * frequency);
        frequency *= 2.f;
        amplitude *= 0.5f;
    }

    return value;
}

float Terrain::ridged(float x, float y) {
    float h = fbm(x, y, 6);
    h = 1.f - fabs(h);
    return h * h;
}

void Terrain::initVoronoiCells() {
    voronoiCells.clear();
    std::random_device rd;
    std::mt19937 gen(12345);
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < 64; i++) {
        voronoiCells.push_back(glm::vec2(dis(gen), dis(gen)) * size);
    }
}

float Terrain::voronoi(float x, float y) {
    glm::vec2 p(x, y);
    float minDist = 1e9f;

    for (const auto& cell : voronoiCells) {
        float d = glm::distance(p, cell);
        minDist = std::min(minDist, d);
    }

    return 1.f - glm::clamp(minDist / (size * 0.1f), 0.f, 1.f);
}

float Terrain::canyon(float x, float y) {
    float base = fbm(x * 0.02f, y * 0.02f, 4);
    float detail = fbm(x * 0.1f, y * 0.1f, 3);

    float channel = sin(x * 0.05f + detail * 2.0f) * 0.5f + 0.5f;
    channel = pow(channel, 3.0f);

    return base * channel + detail * 0.2f;
}

float Terrain::plateaus(float x, float y) {
    float h = fbm(x * 0.03f, y * 0.03f, 5);

    const int steps = 5;
    h = floor(h * steps) / steps;

    h += fbm(x * 0.2f, y * 0.2f, 2) * 0.1f;

    return h;
}

// ===================== Unified Island Generation =========================

float Terrain::islandMask(float x, float y) {
    // Convert to normalized coordinates
    float nx = x / size;
    float ny = y / size;
    float distFromCenter = sqrt(nx * nx + ny * ny);

    // Create organic island shape using noise
    float angle = atan2(ny, nx);

    // Large-scale shape variation (makes island non-circular)
    float shapeNoise = perlin(angle * 2.0f, 0.0f) * 0.15f;
    shapeNoise += perlin(angle * 5.0f, 100.0f) * 0.08f;

    // Adjust distance based on shape noise
    float adjustedDist = distFromCenter - shapeNoise;

    // Create smooth falloff from center
    float mask = 1.0f - adjustedDist;
    mask = glm::clamp(mask, 0.f, 1.f);

    // Smooth curve
    mask = glm::pow(mask, 1.8f);

    return mask;
}

float Terrain::coastlineVariation(float x, float y) {
    // Single coherent noise for coastal features
    float nx = x / size;
    float ny = y / size;
    float angle = atan2(ny, nx);

    // Coastal type variation (smooth, continuous)
    float coastal = perlin(angle * 3.0f + 50.0f, 0.0f) * 0.5f + 0.5f;
    coastal += perlin(angle * 7.0f + 150.0f, 100.0f) * 0.25f;

    return glm::clamp(coastal, 0.f, 1.f);
}

float Terrain::erosionFilter(float height, float slope) {
    float erosion = glm::clamp(slope * 2.0f, 0.f, 1.f);
    return height * (1.0f - erosion * 0.3f);
}

// ===================== MAIN HEIGHT FUNCTION =========================

float Terrain::finalHeight(float x, float y) {
    // Normalized position
    float nx = x / size;
    float ny = y / size;
    float distFromCenter = sqrt(nx * nx + ny * ny);

    // Get island shape mask (handles non-circular shape)
    float islandShape = islandMask(x, y);

    // Base terrain using selected type
    float baseNoise = 0.f;
    switch (type) {
        case TerrainType::ISLAND:
            baseNoise = fbm(x * 0.04f, y * 0.04f, 6);
            break;
        case TerrainType::RIDGED:
            baseNoise = ridged(x * 0.03f, y * 0.03f);
            break;
        case TerrainType::VORONOI:
            baseNoise = voronoi(x, y);
            break;
        case TerrainType::CANYON:
            baseNoise = canyon(x, y);
            break;
        case TerrainType::PLATEAUS:
            baseNoise = plateaus(x, y);
            break;
    }

    // Normalize base noise to 0-1 range
    baseNoise = (baseNoise + 1.0f) * 0.5f;
    baseNoise = glm::clamp(baseNoise, 0.f, 1.f);

    // Coastal variation (determines beach vs cliff)
    float coastType = coastlineVariation(x, y);

    // === HEIGHT PROFILE ===

    // Ocean floor depth
    float oceanFloor = -15.0f;

    // Calculate base elevation from island shape
    float elevation;

    if (islandShape < 0.05f) {
        // Deep ocean
        elevation = oceanFloor;
    }
    else if (islandShape < 0.25f) {
        // Underwater slope
        float t = (islandShape - 0.05f) / 0.20f;
        elevation = glm::mix(oceanFloor, -3.0f, glm::pow(t, 1.5f));
    }
    else if (islandShape < 0.40f) {
        // Coastline transition (beaches and cliffs)
        float t = (islandShape - 0.25f) / 0.15f;

        // Beach areas
        if (coastType > 0.55f) {
            elevation = glm::mix(-3.0f, 1.0f, glm::pow(t, 0.6f));

            // Sand ripples
            float ripple = sin(x * 4.0f) * cos(y * 4.0f) * 0.12f;
            if (elevation > -1.0f && elevation < 2.0f) {
                elevation += ripple * (1.0f - abs(elevation) * 0.5f);
            }
        }
        // Cliff areas
        else {
            elevation = glm::mix(-3.0f, 5.0f, glm::pow(t, 3.0f));

            // Rocky texture for cliffs
            float rockDetail = fbm(x * 0.25f, y * 0.25f, 3) * 1.2f;
            elevation += rockDetail;
        }
    }
    else {
        // Inland terrain
        float t = (islandShape - 0.40f) / 0.60f;

        // Height increases towards center
        float centerHeight = maxHeight * t;

        // Apply terrain noise
        float terrainHeight = centerHeight * baseNoise;

        // Blend from coast to inland
        float coastalHeight = (coastType > 0.55f) ? 1.0f : 5.0f;
        elevation = glm::mix(coastalHeight, terrainHeight, glm::pow(t, 0.7f));

        // Add detail layers
        elevation += fbm(x * 0.12f, y * 0.12f, 3) * 2.5f * t;

        // Central peak/crater
        if (islandShape > 0.85f) {
            float centerMod = (islandShape - 0.85f) / 0.15f;

            if (baseNoise > 0.5f) {
                // Peak
                elevation += centerMod * 8.0f;
            } else {
                // Crater/valley
                elevation -= centerMod * 4.0f;
            }
        }
    }

    return elevation;
}

float Terrain::getHeightAt(float worldX, float worldZ) const {
    float fx = (worldX / size + 0.5f) * resolution;
    float fz = (worldZ / size + 0.5f) * resolution;

    if (fx < 0 || fx >= resolution || fz < 0 || fz >= resolution) {
        return 0.f;
    }

    int x0 = (int)floor(fx);
    int z0 = (int)floor(fz);
    int x1 = std::min(x0 + 1, resolution - 1);
    int z1 = std::min(z0 + 1, resolution - 1);

    float tx = fx - x0;
    float tz = fz - z0;

    int idx00 = z0 * (resolution + 1) + x0;
    int idx10 = z0 * (resolution + 1) + x1;
    int idx01 = z1 * (resolution + 1) + x0;
    int idx11 = z1 * (resolution + 1) + x1;

    float h00 = positions[idx00].y;
    float h10 = positions[idx10].y;
    float h01 = positions[idx01].y;
    float h11 = positions[idx11].y;

    float h0 = h00 * (1 - tx) + h10 * tx;
    float h1 = h01 * (1 - tx) + h11 * tx;
    return h0 * (1 - tz) + h1 * tz;
}

// ===================== Mesh Generation =========================

void Terrain::generateGrid() {
    positions.clear();
    normals.clear();
    uvs.clear();
    indices.clear();

    for (int z = 0; z <= resolution; z++) {
        for (int x = 0; x <= resolution; x++) {
            float fx = (float)x / resolution;
            float fz = (float)z / resolution;
            float wx = (fx - 0.5f) * size;
            float wz = (fz - 0.5f) * size;
            float wy = finalHeight(wx, wz);

            positions.push_back({wx, wy, wz});
            uvs.push_back({fx, fz});
        }
    }

    for (int z = 0; z < resolution; z++) {
        for (int x = 0; x < resolution; x++) {
            int i0 = z * (resolution + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (resolution + 1);
            int i3 = i2 + 1;

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }
}

void Terrain::computeNormals() {
    normals.resize(positions.size(), glm::vec3(0.f));

    for (size_t i = 0; i < indices.size(); i += 3) {
        int i0 = indices[i];
        int i1 = indices[i + 1];
        int i2 = indices[i + 2];

        glm::vec3 edge1 = positions[i1] - positions[i0];
        glm::vec3 edge2 = positions[i2] - positions[i0];
        glm::vec3 n = glm::cross(edge1, edge2);

        normals[i0] += n;
        normals[i1] += n;
        normals[i2] += n;
    }

    for (auto &n : normals) {
        float len = glm::length(n);
        n = len > 0.0001f ? n / len : glm::vec3(0, 1, 0);
    }
}

void Terrain::updateBuffers() {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2),
                 uvs.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    indexCount = indices.size();
}

// ===================== Public API =========================

void Terrain::setType(TerrainType newType) {
    if (type == newType) return;
    type = newType;
    regenerate();
}

void Terrain::setHeightScale(float scale) {
    maxHeight = scale;
    regenerate();
}

void Terrain::setNoiseFrequency(float freq) {
    noiseFrequency = freq;
    regenerate();
}

void Terrain::regenerate() {
    generateGrid();
    computeNormals();
    updateBuffers();
}