#include "Ocean.h"
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <cmath>

#include <shaders/ocean_vert_glsl.h>
#include <shaders/ocean_frag_glsl.h>

// Static member initialization
std::unique_ptr<ppgso::Shader> Ocean::shader;
int Ocean::instanceCount = 0;

Ocean::Ocean(float size, int resolution, float waveHeight)
    : size(size), resolution(resolution), waveHeight(waveHeight) {

    instanceCount++;

    // Initialize shader only once
    if (!shader) {
        shader = std::make_unique<ppgso::Shader>(ocean_vert_glsl, ocean_frag_glsl);
    }

    // Initialize wave parameters
    initializeWaves();

    // Generate initial mesh
    generateMesh();

    // Setup OpenGL buffers
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBO: positions
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                 positions.data(), GL_DYNAMIC_DRAW); // Dynamic for animation
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // NBO: normals
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
                 normals.data(), GL_DYNAMIC_DRAW);
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

Ocean::~Ocean() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &nbo);
    glDeleteBuffers(1, &tbo);
    glDeleteBuffers(1, &ebo);

    instanceCount--;

    if (instanceCount == 0) {
        shader.reset();
    }
}

void Ocean::initializeWaves() {
    waves.clear();
    
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for consistency
    std::uniform_real_distribution<> angleDis(0.0, 2.0 * M_PI);
    
    // Create multiple waves with different properties
    // Large waves
    waves.push_back({30.0f, 1.5f, 1.0f, glm::normalize(glm::vec2(1.0f, 0.3f))});
    waves.push_back({25.0f, 1.2f, 0.9f, glm::normalize(glm::vec2(0.5f, 1.0f))});
    
    // Medium waves
    waves.push_back({15.0f, 0.8f, 1.2f, glm::normalize(glm::vec2(-0.7f, 0.6f))});
    waves.push_back({12.0f, 0.6f, 1.1f, glm::normalize(glm::vec2(0.8f, -0.4f))});
    
    // Small waves (detail)
    for (int i = 0; i < 4; i++) {
        float angle = angleDis(gen);
        waves.push_back({
            5.0f + i * 2.0f,
            0.3f - i * 0.05f,
            1.3f + i * 0.1f,
            glm::vec2(cos(angle), sin(angle))
        });
    }
}

float Ocean::gerstnerWaveHeight(float x, float z, float t) const {
    float height = 0.0f;
    
    for (const auto& wave : waves) {
        float k = 2.0f * M_PI / wave.wavelength;
        float w = wave.speed * waveSpeed;
        float phi = k * (wave.direction.x * x + wave.direction.y * z - w * t);
        
        height += wave.amplitude * waveHeight * sin(phi);
    }
    
    return height;
}

glm::vec3 Ocean::gerstnerWaveNormal(float x, float z, float t) const {
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    
    for (const auto& wave : waves) {
        float k = 2.0f * M_PI / wave.wavelength;
        float w = wave.speed * waveSpeed;
        float phi = k * (wave.direction.x * x + wave.direction.y * z - w * t);
        float amplitude = wave.amplitude * waveHeight;
        
        float c = cos(phi);
        normal.x -= k * amplitude * wave.direction.x * c;
        normal.z -= k * amplitude * wave.direction.y * c;
    }
    
    return glm::normalize(normal);
}

float Ocean::getHeightAt(float worldX, float worldZ, float t) const {
    return gerstnerWaveHeight(worldX, worldZ, t);
}

void Ocean::generateMesh() {
    positions.clear();
    normals.clear();
    uvs.clear();
    indices.clear();

    // Generate flat grid (will be displaced in update)
    for (int z = 0; z <= resolution; z++) {
        for (int x = 0; x <= resolution; x++) {
            float fx = (float)x / resolution;
            float fz = (float)z / resolution;
            float wx = (fx - 0.5f) * size;
            float wz = (fz - 0.5f) * size;

            positions.push_back({wx, 0.0f, wz});
            normals.push_back({0.0f, 1.0f, 0.0f});
            uvs.push_back({fx * 10.0f, fz * 10.0f}); // Repeat texture
        }
    }

    // Generate indices
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

void Ocean::updateMesh(float dt) {
    // Update vertex positions based on Gerstner waves
    for (int z = 0; z <= resolution; z++) {
        for (int x = 0; x <= resolution; x++) {
            int idx = z * (resolution + 1) + x;
            
            float fx = (float)x / resolution;
            float fz = (float)z / resolution;
            float wx = (fx - 0.5f) * size;
            float wz = (fz - 0.5f) * size;
            
            // Calculate wave height
            float height = gerstnerWaveHeight(wx, wz, time);
            
            // Update position
            positions[idx].y = height;
            
            // Calculate normal
            normals[idx] = gerstnerWaveNormal(wx, wz, time);
        }
    }

    // Update GPU buffers
    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    positions.size() * sizeof(glm::vec3), 
                    positions.data());
    
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    normals.size() * sizeof(glm::vec3), 
                    normals.data());
}

void Ocean::update(float dt) {
    time += dt * waveFrequency;
    updateMesh(dt);
}

void Ocean::render(const glm::mat4 &view, const glm::mat4 &projection) {
    shader->use();
    
    // Set matrices
    shader->setUniform("modelMatrix", glm::mat4(1.0f));
    shader->setUniform("viewMatrix", view);
    shader->setUniform("projectionMatrix", projection);
    
    // Set water properties
    shader->setUniform("waterColor", waterColor);
    shader->setUniform("foamColor", foamColor);
    shader->setUniform("transparency", transparency);
    shader->setUniform("time", time);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Disable depth writing (but keep depth testing)
    glDepthMask(GL_FALSE);
    
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    
    // Restore depth writing
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}