#pragma once

#include <ppgso/ppgso.h>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <vector>
#include <memory>

enum class TerrainType {
    ISLAND,
    RIDGED,
    VORONOI,
    CANYON,      // New type suggestion
    PLATEAUS     // New type suggestion
};

class Terrain {
public:
    // Constructor with default parameters
    Terrain(int resolution = 128,
            float size = 100.0f,
            float height = 20.0f,
            TerrainType type = TerrainType::ISLAND);

    ~Terrain();

    void update(float dt);
    void render(const glm::mat4 &view, const glm::mat4 &projection);

    // Terrain type switching
    void setType(TerrainType newType);
    TerrainType getType() const { return type; }

    // Parameter adjustment
    void setHeightScale(float scale);
    void setNoiseFrequency(float freq);
    void regenerate();

    // Height query for collision detection
    float getHeightAt(float worldX, float worldZ) const;

private:
    // Mesh data
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;

    // OpenGL buffers
    GLuint vao = 0, vbo = 0, nbo = 0, tbo = 0, ebo = 0;
    size_t indexCount = 0;

    // Terrain parameters
    int resolution;
    float size;
    float maxHeight;
    float noiseFrequency = 1.0f;
    TerrainType type;

    // Voronoi cell cache for consistency
    std::vector<glm::vec2> voronoiCells;
    void initVoronoiCells();

    // Perlin noise helpers
    float fade(float t);
    float lerp(float t, float a, float b);
    float grad(int hash, float x, float y);

    // Noise functions
    float perlin(float x, float y);
    float fbm(float x, float y, int octaves = 5);
    float ridged(float x, float y);
    float voronoi(float x, float y);
    float canyon(float x, float y);
    float plateaus(float x, float y);

    // Masks and filters
    float islandMask(float x, float y);
    float erosionFilter(float height, float slope);

    // Permutation table for Perlin noise
    static std::vector<int> permutation;

    // Final height computation
    float finalHeight(float x, float y);

    // Mesh generation
    void generateGrid();
    void computeNormals();
    void updateBuffers();

    // Shader (shared across all terrain instances)
    static std::unique_ptr<ppgso::Shader> shader;
    static int instanceCount;
};