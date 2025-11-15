// src/examples/island_demo.cpp
#include <iostream>

// GLEW must be included before GLFW on this project (Windows build in this repo uses GLEW).
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// PPGSO helpers (optional, only if you use classes from ppgso)
#include <ppgso/ppgso.h>

// Our terrain
#include "terrain/Terrain.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>



const unsigned int WIDTH  = 1280;
const unsigned int HEIGHT = 720;

int main() {
    // Init GLFW
    std::cout << "SOM V TELKE = " << std::endl;
    std::ifstream vertFile("shader/terrain_vert.glsl");
    std::stringstream buffer;
    buffer << vertFile.rdbuf();
    std::string vertSource = buffer.str();
    std::cout << "VERT SOURCE:\n" << vertSource << "\n";
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    // Request OpenGL 3.3 core profile (matches examples in repo)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window + context
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Island Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Init GLEW (this repo bundles GLEW on Windows builds)
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    // glewInit may generate a harmless GL_INVALID_ENUM on some drivers; check result nonetheless
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: "
                  << reinterpret_cast<const char*>(glewGetErrorString(err)) << std::endl;
        glfwTerminate();
        return -1;
    }

    // OpenGL state
    glEnable(GL_DEPTH_TEST);

    // Create terrain (resolution, size, maxHeight)
    Terrain terrain(200, 80.0f, 12.0f);

    // Simple camera
    glm::vec3 camPos(80.0f, 40.0f, 80.0f);
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Handle input / events
        glfwPollEvents();

        // Clear
        glViewport(0, 0, WIDTH, HEIGHT);
        glClearColor(0.4f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build view / projection
        glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);

        // Render terrain (note: Terrain::render expects view + projection)
        terrain.render(view, projection);
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            terrain.setType(TerrainType::ISLAND);
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            terrain.setType(TerrainType::RIDGED);
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
            terrain.setType(TerrainType::VORONOI);
        }
        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glfwTerminate();
    return 0;
}
