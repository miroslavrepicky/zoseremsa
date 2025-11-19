#include <iostream>
#include <ppgso/ppgso.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../terrain/Terrain.h"
#include "../ocean/Ocean.h"

const unsigned int SIZE = 1024;

class OceanScene {
private:
    std::unique_ptr<Terrain> terrain;
    std::unique_ptr<Ocean> ocean;

    // Camera modes
    enum CameraMode { ORBIT, FREE };
    CameraMode cameraMode = ORBIT;

    // Orbit mode parameters
    glm::vec3 orbitTarget;
    float cameraDistance = 150.0f;
    float cameraAngle = 0.0f;
    float cameraHeight = 40.0f;
    float cameraPitch = 20.0f;

    // Free mode parameters
    glm::vec3 cameraPosition;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 cameraRight;
    float yaw = -90.0f;   // Horizontal rotation
    float pitch = 0.0f;   // Vertical rotation

    // Camera movement speeds
    float moveSpeed = 100.0f;
    float sprintMultiplier = 2.5f;
    float rotateSpeed = 90.0f;
    float mouseSensitivity = 0.1f;

    // Auto-rotation (orbit mode only)
    bool autoRotate = false;
    float autoRotateSpeed = 0.2f;

    // Matrices
    glm::mat4 projection;
    glm::mat4 view;

    // Track key states for smooth movement
    bool keys[GLFW_KEY_LAST] = {false};

public:
    OceanScene() {
        // Initialize terrain (island)
        terrain = std::make_unique<Terrain>(
            512,                      // resolution
            1024.0f,          // size
            55.0f,                    // height
            TerrainType::ISLAND       // type
        );

        // Initialize ocean (larger than island)
        ocean = std::make_unique<Ocean>(
            1024.0f,          // size
            200,                      // resolution
            0.5f                      // wave height
        );

        // Configure ocean appearance
        ocean->setWaterColor(glm::vec3(0.05f, 0.2f, 0.4f));
        ocean->setFoamColor(glm::vec3(0.9f, 0.95f, 1.0f));
        ocean->setTransparency(0.85f);
        ocean->setWaveSpeed(1.0f);

        // Setup orbit mode
        orbitTarget = glm::vec3(0.0f, cameraHeight * 0.3f, 0.0f);

        // Setup free mode defaults
        cameraPosition = glm::vec3(0.0f, 50.0f, 200.0f);
        cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        updateFreeCameraVectors();

        updateCamera();

        // Setup projection with extended far plane
        projection = glm::perspective(
            glm::radians(60.0f),
            (float)SIZE / (float)SIZE,
            0.1f,
            2000.0f  // Increased from 500.0f for better visibility
        );
    }

    void updateFreeCameraVectors() {
        // Calculate direction vector
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(direction);

        // Calculate right and up vectors
        cameraRight = glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    }

    void updateOrbitCamera() {
        // Convert spherical coordinates to cartesian
        float angleRad = glm::radians(cameraAngle);
        float pitchRad = glm::radians(cameraPitch);

        cameraPosition = glm::vec3(
            sin(angleRad) * cos(pitchRad) * cameraDistance,
            sin(pitchRad) * cameraDistance + cameraHeight,
            cos(angleRad) * cos(pitchRad) * cameraDistance
        );

        view = glm::lookAt(cameraPosition, orbitTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void updateFreeCamera() {
        view = glm::lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
    }

    void updateCamera() {
        if (cameraMode == ORBIT) {
            updateOrbitCamera();
        } else {
            updateFreeCamera();
        }
    }

    void update(float dt) {
        if (cameraMode == ORBIT) {
            // Orbit mode controls
            if (keys[GLFW_KEY_A]) {
                cameraAngle -= rotateSpeed * dt;
            }
            if (keys[GLFW_KEY_D]) {
                cameraAngle += rotateSpeed * dt;
            }
            if (keys[GLFW_KEY_W]) {
                cameraPitch += rotateSpeed * dt * 0.5f;
                cameraPitch = glm::clamp(cameraPitch, -85.0f, 85.0f);
            }
            if (keys[GLFW_KEY_S]) {
                cameraPitch -= rotateSpeed * dt * 0.5f;
                cameraPitch = glm::clamp(cameraPitch, -85.0f, 85.0f);
            }
            if (keys[GLFW_KEY_Q]) {
                cameraDistance = glm::max(10.0f, cameraDistance - moveSpeed * dt);
            }
            if (keys[GLFW_KEY_E]) {
                cameraDistance = glm::min(500.0f, cameraDistance + moveSpeed * dt);
            }
            if (keys[GLFW_KEY_UP]) {
                cameraHeight += moveSpeed * dt;
            }
            if (keys[GLFW_KEY_DOWN]) {
                cameraHeight = glm::max(0.0f, cameraHeight - moveSpeed * dt);
            }
            if (keys[GLFW_KEY_LEFT]) {
                cameraDistance = glm::max(10.0f, cameraDistance - moveSpeed * dt);
            }
            if (keys[GLFW_KEY_RIGHT]) {
                cameraDistance = glm::min(500.0f, cameraDistance + moveSpeed * dt);
            }

            // Auto-rotation
            if (autoRotate) {
                cameraAngle += autoRotateSpeed * dt * 10.0f;
            }

            // Keep angle in 0-360 range
            while (cameraAngle >= 360.0f) cameraAngle -= 360.0f;
            while (cameraAngle < 0.0f) cameraAngle += 360.0f;
        }
        else { // FREE mode
            float speed = moveSpeed;
            if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) {
                speed *= sprintMultiplier;
            }

            // WASD movement
            if (keys[GLFW_KEY_W]) {
                cameraPosition += cameraFront * speed * dt;
            }
            if (keys[GLFW_KEY_S]) {
                cameraPosition -= cameraFront * speed * dt;
            }
            if (keys[GLFW_KEY_A]) {
                cameraPosition -= cameraRight * speed * dt;
            }
            if (keys[GLFW_KEY_D]) {
                cameraPosition += cameraRight * speed * dt;
            }

            // Up/Down movement
            if (keys[GLFW_KEY_SPACE]) {
                cameraPosition += cameraUp * speed * dt;
            }
            if (keys[GLFW_KEY_LEFT_CONTROL] || keys[GLFW_KEY_RIGHT_CONTROL]) {
                cameraPosition -= cameraUp * speed * dt;
            }

            // Arrow key rotation
            if (keys[GLFW_KEY_LEFT]) {
                yaw -= rotateSpeed * dt;
                updateFreeCameraVectors();
            }
            if (keys[GLFW_KEY_RIGHT]) {
                yaw += rotateSpeed * dt;
                updateFreeCameraVectors();
            }
            if (keys[GLFW_KEY_UP]) {
                pitch += rotateSpeed * dt;
                pitch = glm::clamp(pitch, -89.0f, 89.0f);
                updateFreeCameraVectors();
            }
            if (keys[GLFW_KEY_DOWN]) {
                pitch -= rotateSpeed * dt;
                pitch = glm::clamp(pitch, -89.0f, 89.0f);
                updateFreeCameraVectors();
            }
        }

        updateCamera();

        // Update ocean waves
        ocean->update(dt);
        terrain->update(dt);
    }

    void render() {
        // Clear screen with sky color
        glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        // Render terrain first (opaque)
        terrain->render(view, projection);

        // Render ocean last (transparent)
        ocean->render(view, projection);
    }

    void handleKeyboard(int key, int action) {
        // Track key states for continuous movement
        if (action == GLFW_PRESS) {
            keys[key] = true;
        } else if (action == GLFW_RELEASE) {
            keys[key] = false;
        }

        // Handle single-press actions
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_1:
                    terrain->setType(TerrainType::ISLAND);
                    std::cout << "Terrain: ISLAND\n";
                    break;
                case GLFW_KEY_2:
                    terrain->setType(TerrainType::RIDGED);
                    std::cout << "Terrain: RIDGED\n";
                    break;
                case GLFW_KEY_3:
                    terrain->setType(TerrainType::VORONOI);
                    std::cout << "Terrain: VORONOI\n";
                    break;
                case GLFW_KEY_4:
                    terrain->setType(TerrainType::CANYON);
                    std::cout << "Terrain: CANYON\n";
                    break;
                case GLFW_KEY_5:
                    terrain->setType(TerrainType::PLATEAUS);
                    std::cout << "Terrain: PLATEAUS\n";
                    break;
                case GLFW_KEY_TAB:
                    // Toggle camera mode
                    if (cameraMode == ORBIT) {
                        cameraMode = FREE;
                        // Set free camera to current orbit position
                        updateOrbitCamera();
                        glm::vec3 dir = glm::normalize(orbitTarget - cameraPosition);
                        yaw = glm::degrees(atan2(dir.z, dir.x));
                        pitch = glm::degrees(asin(dir.y));
                        updateFreeCameraVectors();
                        std::cout << "Camera Mode: FREE (FPS-style)\n";
                        std::cout << "  WASD: Move, SHIFT: Sprint, SPACE: Up, CTRL: Down\n";
                        std::cout << "  Arrow Keys: Look around\n";
                    } else {
                        cameraMode = ORBIT;
                        std::cout << "Camera Mode: ORBIT\n";
                    }
                    break;
                case GLFW_KEY_R:
                    // Reset camera
                    if (cameraMode == ORBIT) {
                        cameraDistance = 150.0f;
                        cameraAngle = 0.0f;
                        cameraHeight = 40.0f;
                        cameraPitch = 20.0f;
                        std::cout << "Camera reset (Orbit)\n";
                    } else {
                        cameraPosition = glm::vec3(0.0f, 50.0f, 200.0f);
                        yaw = -90.0f;
                        pitch = 0.0f;
                        updateFreeCameraVectors();
                        std::cout << "Camera reset (Free)\n";
                    }
                    break;
                case GLFW_KEY_F:
                    // Toggle auto-rotate (orbit mode only)
                    if (cameraMode == ORBIT) {
                        autoRotate = !autoRotate;
                        std::cout << "Auto-rotate: " << (autoRotate ? "ON" : "OFF") << "\n";
                    }
                    break;
                case GLFW_KEY_Z:
                    ocean->setWaveHeight(ocean->getHeightAt(0, 0, 0) + 0.5f);
                    std::cout << "Wave height increased\n";
                    break;
                case GLFW_KEY_X:
                    ocean->setWaveSpeed(1.5f);
                    std::cout << "Wave speed increased\n";
                    break;
                case GLFW_KEY_C:
                    // Print camera info
                    if (cameraMode == ORBIT) {
                        std::cout << "Camera (Orbit) - Angle: " << cameraAngle
                                  << "° Pitch: " << cameraPitch
                                  << "° Distance: " << cameraDistance
                                  << " Height: " << cameraHeight << "\n";
                    } else {
                        std::cout << "Camera (Free) - Pos: (" << cameraPosition.x << ", "
                                  << cameraPosition.y << ", " << cameraPosition.z
                                  << ") Yaw: " << yaw << "° Pitch: " << pitch << "°\n";
                    }
                    break;
            }
        }
    }
};

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return EXIT_FAILURE;
    }

    // Create window
    auto window = glfwCreateWindow(SIZE, SIZE, "Ocean Island Scene - Enhanced Controls", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Create scene
    OceanScene scene;

    // Keyboard callback
    glfwSetWindowUserPointer(window, &scene);
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        auto* s = static_cast<OceanScene*>(glfwGetWindowUserPointer(win));
        s->handleKeyboard(key, action);
    });

    // Print controls
    std::cout << "=== OCEAN ISLAND SCENE - DUAL CAMERA MODES ===\n\n";
    std::cout << "CAMERA MODES:\n";
    std::cout << "  TAB:        Switch between ORBIT and FREE camera\n";
    std::cout << "  R:          Reset camera position\n";
    std::cout << "  C:          Print camera info\n\n";
    std::cout << "ORBIT MODE (default):\n";
    std::cout << "  A/D:        Rotate left/right around island\n";
    std::cout << "  W/S:        Rotate up/down\n";
    std::cout << "  Q/E:        Zoom in/out\n";
    std::cout << "  Arrow Keys: Adjust distance & height\n";
    std::cout << "  F:          Toggle auto-rotation\n\n";
    std::cout << "FREE MODE (FPS-style):\n";
    std::cout << "  W/A/S/D:    Move forward/left/back/right\n";
    std::cout << "  SHIFT:      Sprint (hold)\n";
    std::cout << "  SPACE:      Move up\n";
    std::cout << "  CTRL:       Move down\n";
    std::cout << "  Arrow Keys: Look around\n\n";
    std::cout << "TERRAIN:\n";
    std::cout << "  1-5:        Change terrain type\n\n";
    std::cout << "OCEAN:\n";
    std::cout << "  Z:          Increase wave height\n";
    std::cout << "  X:          Increase wave speed\n\n";
    std::cout << "OTHER:\n";
    std::cout << "  ESC:        Exit\n";
    std::cout << "==============================================\n\n";

    // Main loop
    auto lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        auto currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        // Update scene
        scene.update(dt);

        // Render scene
        scene.render();

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Exit on ESC
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}