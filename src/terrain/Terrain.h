#pragma once
#include <vector>
#include <ppgso/ppgso.h>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

enum class TerrainType {
    ISLAND,
    RIDGED,
    VORONOI
};

class Terrain {
public:
    Terrain(int resolution, float size, float height, TerrainType type = TerrainType::ISLAND);

    void update(float dt);
    void render(const glm::mat4 &view, const glm::mat4 &projection);
    void setType(TerrainType newType);

private:
    static std::unique_ptr<ppgso::Shader> shader;
    int resolution;
    float size;
    float maxHeight;
    TerrainType type;

    GLuint vao, vbo, nbo, tbo, ebo;
    size_t indexCount;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;

    // === height algorithms ===
    float perlin(float x, float y);
    float fbm(float x, float y);
    float ridged(float x, float y);
    float voronoi(float x, float y);
    float islandMask(float x, float y);

    float finalHeight(float x, float y);

    // === mesh generation ===
    void generateGrid();
    void computeNormals();
};
