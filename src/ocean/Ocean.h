#pragma once

#include <ppgso/ppgso.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Ocean {
public:
    // Constructor
    Ocean(float size = 200.0f,
          int resolution = 256,
          float waveHeight = 2.0f);

    ~Ocean();

    void update(float dt);
    void render(const glm::mat4 &view, const glm::mat4 &projection);

    // Wave parameters
    void setWaveSpeed(float speed) { waveSpeed = speed; }
    void setWaveHeight(float height) { waveHeight = height; }
    void setWaveFrequency(float freq) { waveFrequency = freq; }

    // Visual parameters
    void setWaterColor(const glm::vec3& color) { waterColor = color; }
    void setFoamColor(const glm::vec3& color) { foamColor = color; }
    void setTransparency(float alpha) { transparency = alpha; }

    // Get height at position (for foam/intersection detection)
    float getHeightAt(float worldX, float worldZ, float time) const;

private:
    // Mesh data
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;

    // OpenGL buffers
    GLuint vao = 0, vbo = 0, nbo = 0, tbo = 0, ebo = 0;
    size_t indexCount = 0;

    // Ocean parameters
    float size;
    int resolution;
    float waveHeight;
    float waveSpeed = 1.0f;
    float waveFrequency = 1.0f;
    float time = 0.0f;

    // Visual parameters
    glm::vec3 waterColor = glm::vec3(0.1f, 0.3f, 0.5f);
    glm::vec3 foamColor = glm::vec3(0.9f, 0.95f, 1.0f);
    float transparency = 0.7f;

    // Wave functions
    struct Wave {
        float wavelength;
        float amplitude;
        float speed;
        glm::vec2 direction;
    };
    std::vector<Wave> waves;

    void initializeWaves();
    float gerstnerWaveHeight(float x, float z, float t) const;
    glm::vec3 gerstnerWaveNormal(float x, float z, float t) const;

    // Mesh generation
    void generateMesh();
    void updateMesh(float dt);
    void computeNormals();

    // Shader (shared across all ocean instances)
    static std::unique_ptr<ppgso::Shader> shader;
    static int instanceCount;
};