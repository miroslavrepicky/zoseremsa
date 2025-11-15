// src/examples/island_demo.cpp
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ppgso/ppgso.h>
#include "terrain/Terrain.h"

const unsigned int WIDTH  = 1280;
const unsigned int HEIGHT = 720;

// Camera state
struct Camera {
    glm::vec3 position = glm::vec3(80.0f, 40.0f, 80.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    float yaw = -135.0f;
    float pitch = -20.0f;
    float distance = 100.0f;
    float rotationSpeed = 50.0f;
    float zoomSpeed = 10.0f;

    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, target, glm::vec3(0, 1, 0));
    }

    void update(float dt) {
        // Calculate camera position based on spherical coordinates
        float radYaw = glm::radians(yaw);
        float radPitch = glm::radians(pitch);

        position.x = target.x + distance * cos(radPitch) * cos(radYaw);
        position.y = target.y + distance * sin(radPitch);
        position.z = target.z + distance * cos(radPitch) * sin(radYaw);
    }
};

// Input state
struct InputState {
    bool leftMousePressed = false;
    bool rightMousePressed = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool wireframeMode = false;
    bool showNormals = false;
    float heightScale = 12.0f;
    float noiseFrequency = 1.0f;
};

// Global state for callbacks
Camera camera;
InputState input;

// Mouse callback
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        input.leftMousePressed = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        input.rightMousePressed = (action == GLFW_PRESS);
    }
}

// Scroll callback for zoom
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.distance -= yoffset * camera.zoomSpeed;
    camera.distance = glm::clamp(camera.distance, 10.0f, 500.0f);
}

// Mouse movement callback
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    double deltaX = xpos - input.lastMouseX;
    double deltaY = ypos - input.lastMouseY;
    input.lastMouseX = xpos;
    input.lastMouseY = ypos;

    if (input.leftMousePressed) {
        camera.yaw += deltaX * 0.5f;
        camera.pitch -= deltaY * 0.5f;
        camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);
    }
}

// Draw text overlay (simple console output)
void printControls() {
    static bool printed = false;
    if (!printed) {
        std::cout << "\n=== TERRAIN DEMO CONTROLS ===\n";
        std::cout << "1-5: Switch terrain types\n";
        std::cout << "  1: Island\n";
        std::cout << "  2: Ridged Mountains\n";
        std::cout << "  3: Voronoi Cells\n";
        std::cout << "  4: Canyon\n";
        std::cout << "  5: Plateaus\n";
        std::cout << "\nCamera:\n";
        std::cout << "  Left Mouse + Drag: Rotate camera\n";
        std::cout << "  Mouse Wheel: Zoom in/out\n";
        std::cout << "  A: Rotate left\n";
        std::cout << "  D: Rotate right\n";
        std::cout << "\nTerrain:\n";
        std::cout << "  W: Wireframe toggle\n";
        std::cout << "  R: Regenerate terrain\n";
        std::cout << "  +/-: Adjust height scale\n";
        std::cout << "  [/]: Adjust noise frequency\n";
        std::cout << "  SPACE: Reset camera\n";
        std::cout << "  ESC: Exit\n";
        std::cout << "============================\n\n";
        printed = true;
    }
}

int main() {
    // Init GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // Enable MSAA

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Interactive Terrain Demo", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Setup callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    // Get initial mouse position
    glfwGetCursorPos(window, &input.lastMouseX, &input.lastMouseY);

    // Init GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        glfwTerminate();
        return -1;
    }

    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE); // Enable MSAA
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Create terrain
    Terrain terrain(200, 80.0f, 12.0f, TerrainType::ISLAND);

    printControls();

    // Timing
    float lastTime = glfwGetTime();
    float deltaTime = 0.0f;

    // Animation
    float timeAccumulator = 0.0f;
    bool autoRotate = false;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        timeAccumulator += deltaTime;

        // Handle input
        glfwPollEvents();

        // Terrain type switching
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            terrain.setType(TerrainType::ISLAND);
            std::cout << "Terrain: Island\n";
            glfwWaitEventsTimeout(0.2); // Debounce
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            terrain.setType(TerrainType::RIDGED);
            std::cout << "Terrain: Ridged Mountains\n";
            glfwWaitEventsTimeout(0.2);
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
            terrain.setType(TerrainType::VORONOI);
            std::cout << "Terrain: Voronoi Cells\n";
            glfwWaitEventsTimeout(0.2);
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
            terrain.setType(TerrainType::CANYON);
            std::cout << "Terrain: Canyon\n";
            glfwWaitEventsTimeout(0.2);
        }
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
            terrain.setType(TerrainType::PLATEAUS);
            std::cout << "Terrain: Plateaus\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Wireframe toggle
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && !input.wireframeMode) {
            input.wireframeMode = true;
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            std::cout << "Wireframe: ON\n";
            glfwWaitEventsTimeout(0.2);
        } else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && input.wireframeMode) {
            input.wireframeMode = false;
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            std::cout << "Wireframe: OFF\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Regenerate terrain
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            terrain.regenerate();
            std::cout << "Terrain regenerated\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Height adjustment
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
            input.heightScale += 2.0f * deltaTime * 10.0f;
            terrain.setHeightScale(input.heightScale);
            std::cout << "Height scale: " << input.heightScale << "\n";
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
            input.heightScale = glm::max(1.0f, input.heightScale - 2.0f * deltaTime * 10.0f);
            terrain.setHeightScale(input.heightScale);
            std::cout << "Height scale: " << input.heightScale << "\n";
        }

        // Noise frequency adjustment
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            input.noiseFrequency = glm::max(0.1f, input.noiseFrequency - 0.1f * deltaTime * 5.0f);
            terrain.setNoiseFrequency(input.noiseFrequency);
            std::cout << "Noise frequency: " << input.noiseFrequency << "\n";
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            input.noiseFrequency = glm::min(5.0f, input.noiseFrequency + 0.1f * deltaTime * 5.0f);
            terrain.setNoiseFrequency(input.noiseFrequency);
            std::cout << "Noise frequency: " << input.noiseFrequency << "\n";
        }

        // Camera rotation with keys
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.yaw -= camera.rotationSpeed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.yaw += camera.rotationSpeed * deltaTime;
        }

        // Auto-rotate toggle
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
            autoRotate = !autoRotate;
            std::cout << "Auto-rotate: " << (autoRotate ? "ON" : "OFF") << "\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Auto rotation
        if (autoRotate) {
            camera.yaw += 15.0f * deltaTime;
        }

        // Reset camera
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            camera.yaw = -135.0f;
            camera.pitch = -20.0f;
            camera.distance = 100.0f;
            std::cout << "Camera reset\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Exit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Update camera
        camera.update(deltaTime);

        // Clear
        glViewport(0, 0, WIDTH, HEIGHT);
        glClearColor(0.4f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build matrices
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f),
            (float)WIDTH / (float)HEIGHT,
            0.1f,
            1000.0f
        );

        // Render terrain
        terrain.render(view, projection);

        // Test height query at camera target (optional debug)
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
            float height = terrain.getHeightAt(camera.target.x, camera.target.z);
            std::cout << "Height at camera target: " << height << "\n";
            glfwWaitEventsTimeout(0.2);
        }

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    glfwTerminate();
    std::cout << "\nTerrain demo closed successfully!\n";
    return 0;
}