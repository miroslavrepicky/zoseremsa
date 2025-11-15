#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include <shaders/terrain_vert_glsl.h>
#include <shaders/terrain_frag_glsl.h>

Terrain::Terrain(int resolution, float size, float height, TerrainType type)
        : resolution(resolution), size(size), maxHeight(height), type(type) {

    // inicializácia shaderu iba raz
    if (!shader) {
        shader = std::make_unique<ppgso::Shader>(terrain_vert_glsl, terrain_frag_glsl);
    }
    generateGrid();
    computeNormals();

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

void Terrain::update(float) {}

void Terrain::render(const glm::mat4 &view, const glm::mat4 &projection) {
    shader->use();
    shader->setUniform("modelMatrix", glm::mat4(1.f));
    shader->setUniform("viewMatrix", view);
    shader->setUniform("projectionMatrix", projection);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

// ===================== Height algorithms =========================

float Terrain::perlin(float x, float y) {
    return glm::perlin(glm::vec2{x, y});
}

float Terrain::fbm(float x, float y) {
    float value = 0.f;
    float amplitude = 0.5f;
    float frequency = 1.f;

    for (int i = 0; i < 5; i++) {
        value += amplitude * perlin(x * frequency, y * frequency);
        frequency *= 2.f;
        amplitude *= 0.5f;
    }

    return value;
}

float Terrain::ridged(float x, float y) {
    float h = fbm(x, y);
    return 1.f - fabs(h);  // ostré hrebene
}

float Terrain::voronoi(float x, float y) {
    glm::vec2 p(x, y);
    float minDist = 1e9f;

    for (int i = 0; i < 32; i++) {
        glm::vec2 cell = glm::vec2(
            glm::fract(sinf(i * 12.989f) * 43758.5453f),
            glm::fract(sinf(i * 78.233f) * 96234.5453f)
        ) * 20.f;

        float d = glm::distance(p, cell);
        minDist = std::min(minDist, d);
    }

    return 1.f - glm::clamp(minDist * 0.2f, 0.f, 1.f);
}

float Terrain::islandMask(float x, float y) {
    float nx = x / size;
    float ny = y / size;
    float d = sqrt(nx * nx + ny * ny);
    return glm::clamp(1.f - glm::pow(d, 3.f), 0.f, 1.f);
}

float Terrain::finalHeight(float x, float y) {
    float h = 0.f;

    switch (type) {
        case TerrainType::ISLAND:
            h = fbm(x * 0.05f, y * 0.05f);
            break;

        case TerrainType::RIDGED:
            h = ridged(x * 0.03f, y * 0.03f);
            break;

        case TerrainType::VORONOI:
            h = voronoi(x * 0.1f, y * 0.1f);
            break;
    }

    h = pow(glm::max(h, 0.f), 1.5f);
    return h * islandMask(x, y) * maxHeight;
}

// ===================== Mesh generation =========================

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

        glm::vec3 n = glm::normalize(glm::cross(
            positions[i1] - positions[i0],
            positions[i2] - positions[i0]
        ));

        normals[i0] += n;
        normals[i1] += n;
        normals[i2] += n;
    }

    for (auto &n : normals)
        n = glm::normalize(n);
}

void Terrain::setType(TerrainType newType) {
    if (type == newType) return; // žiadna zmena

    type = newType;

    // regeneruj mesh
    generateGrid();
    computeNormals();

    // update VBO / NBO / TBO / EBO
    glBindVertexArray(vao);

    // positions
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);

    // normals
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

    // uvs
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);

    // indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    indexCount = indices.size();
}


std::unique_ptr<ppgso::Shader> Terrain::shader;
