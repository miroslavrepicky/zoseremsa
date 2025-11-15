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

    // Initialize shader only once
    if (!shader) {
        shader = std::make_unique<ppgso::Shader>(terrain_vert_glsl, terrain_frag_glsl);
    }

    // Initialize Perlin permutation table only once
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

    // Initialize Voronoi cells
    initVoronoiCells();

    generateGrid();
    computeNormals();

    // Setup OpenGL buffers
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBO: positions
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // NBO: normals
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // TBO: UV
    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2),
                 uvs.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // EBO: indices
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

    // Clean up shader when last instance is destroyed
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
    // Find unit grid cell containing point
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    // Get relative xy coordinates within cell
    x -= floor(x);
    y -= floor(y);

    // Compute fade curves
    float u = fade(x);
    float v = fade(y);

    // Hash coordinates of the 4 cube corners
    int A  = permutation[X] + Y;
    int AA = permutation[A];
    int AB = permutation[A + 1];
    int B  = permutation[X + 1] + Y;
    int BA = permutation[B];
    int BB = permutation[B + 1];

    // Blend results from 4 corners
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
    return h * h;  // Sharper ridges
}

void Terrain::initVoronoiCells() {
    voronoiCells.clear();
    std::random_device rd;
    std::mt19937 gen(12345); // Fixed seed for consistency
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

    // Create canyon channels
    float channel = sin(x * 0.05f + detail * 2.0f) * 0.5f + 0.5f;
    channel = pow(channel, 3.0f);

    return base * channel + detail * 0.2f;
}

float Terrain::plateaus(float x, float y) {
    float h = fbm(x * 0.03f, y * 0.03f, 5);

    // Create stepped plateaus
    const int steps = 5;
    h = floor(h * steps) / steps;

    // Add small detail
    h += fbm(x * 0.2f, y * 0.2f, 2) * 0.1f;

    return h;
}

// ===================== Masks and Filters =========================

float Terrain::islandMask(float x, float y) {
    float nx = x / size;
    float ny = y / size;
    float d = sqrt(nx * nx + ny * ny);
    return glm::clamp(1.f - glm::pow(d, 2.5f), 0.f, 1.f);
}

float Terrain::erosionFilter(float height, float slope) {
    // Simulate erosion: steeper slopes lose height
    float erosion = glm::clamp(slope * 2.0f, 0.f, 1.f);
    return height * (1.0f - erosion * 0.3f);
}

// ===================== Height Computation =========================

float Terrain::finalHeight(float x, float y) {
    float h = 0.f;

    switch (type) {
        case TerrainType::ISLAND:
            h = fbm(x * 0.05f, y * 0.05f, 5);
            h = pow(glm::max(h, 0.f), 1.2f);
            break;

        case TerrainType::RIDGED:
            h = ridged(x * 0.03f, y * 0.03f);
            break;

        case TerrainType::VORONOI:
            h = voronoi(x, y);
            break;

        case TerrainType::CANYON:
            h = canyon(x, y);
            break;

        case TerrainType::PLATEAUS:
            h = plateaus(x, y);
            break;
    }

    // Apply island mask
    h *= islandMask(x, y);

    return h * maxHeight;
}

float Terrain::getHeightAt(float worldX, float worldZ) const {
    // Convert world coordinates to grid coordinates
    float fx = (worldX / size + 0.5f) * resolution;
    float fz = (worldZ / size + 0.5f) * resolution;

    // Clamp to valid range
    if (fx < 0 || fx >= resolution || fz < 0 || fz >= resolution) {
        return 0.f;
    }

    // Get the four surrounding vertices
    int x0 = (int)floor(fx);
    int z0 = (int)floor(fz);
    int x1 = std::min(x0 + 1, resolution);
    int z1 = std::min(z0 + 1, resolution);

    // Get interpolation factors
    float tx = fx - x0;
    float tz = fz - z0;

    // Get heights at corners
    int idx00 = z0 * (resolution + 1) + x0;
    int idx10 = z0 * (resolution + 1) + x1;
    int idx01 = z1 * (resolution + 1) + x0;
    int idx11 = z1 * (resolution + 1) + x1;

    float h00 = positions[idx00].y;
    float h10 = positions[idx10].y;
    float h01 = positions[idx01].y;
    float h11 = positions[idx11].y;

    // Bilinear interpolation
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

    // Calculate face normals and accumulate
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

    // Normalize all normals with safety check
    for (auto &n : normals) {
        float len = glm::length(n);
        n = len > 0.0001f ? n / len : glm::vec3(0, 1, 0);
    }
}

void Terrain::updateBuffers() {
    glBindVertexArray(vao);

    // Update positions
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_STATIC_DRAW);

    // Update normals
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_STATIC_DRAW);

    // Update UVs
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2),
                 uvs.data(), GL_STATIC_DRAW);

    // Update indices
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