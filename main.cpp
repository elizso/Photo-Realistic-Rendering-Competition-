#include "Renderer.hpp"
#include "Scene.hpp"
#include "Triangle.hpp"
#include "Sphere.hpp"
#include "Vector.hpp"
#include "global.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>

// Dear ImGui + GLFW/OpenGL backend
// This gives the project a lightweight GUI instead of the earlier terminal-only setup.
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

float TASK_N = 2; // 1.1, 1.2, 1.3, 2
bool waveGlint = false;
bool photonMapEnabled = false;
bool glintEnabled = false;
bool heroSampleEnabled = false;

// Object currently selected in the GUI.
// These correspond to the scene elements that can be adjusted interactively.
enum class SelectedObject {
    Diamond = 0,
    Stone = 1,
    Camera = 2,
    MainLight = 3,
    BackLight = 4
};

// Current editing mode.
// Move changes position, Rotate changes object rotation, and Camera changes camera navigation/FOV.
enum class TransformMode {
    Move = 0,
    Rotate = 1,
    Camera = 2
};

// 1. Default parameters
// These replace the old interactive terminal variables. The GUI directly edits this struct.
struct RenderParams {
    int width = 1024;
    int height = 1024;
    int spp = 256;

    Vector3f cameraPos = Vector3f(278, 273, -800);
    Vector3f cameraLookAt = Vector3f(278, 273, 0);
    float cameraFov = 40.0f;

    // Vector3f cameraPos(278, 273, -800);
    Vector3f diamondPos = Vector3f(350, 0, 250);
    float diamondRotX = -12.0f;
    float diamondRotZ = 40.0f;
    float diamondRotY = 180.0f;

    Vector3f stonePos = Vector3f(180, 0, 300);
    float stoneRotX = 0.0f;
    float stoneRotY = 0.0f;
    float stoneRotZ = 0.0f;

    bool enableShadows = true;

    int numLights = 2;
    Vector3f lightPos = Vector3f(0, -5, 0);
    Vector3f lightBackPos = Vector3f(0, -5, -500);
    float lightSize = 1.0f;

    int photonCount = 500000;
    float russianRoulette = 0.8f;

    std::string rockTexturePath = "../models/rock/initialShadingGroup_Base_color.png";
    std::string rockNormalPath = "../models/rock/rock_normals.jpg";

    // std::string rockTexturePath = "../models/spot/spot_texture.png";
    // std::string rockNormalPath = "../models/spot/hmap.jpg";

    std::string envMap = "none";
    std::string textureMap = "../models/rock/initialShadingGroup_Base_color.png";
    // std::string textureMap = "../models/spot/spot_texture.png";


    bool waveGlint = false;
    bool photonMapEnabled = false;
    bool glintEnabled = false;
    bool heroSampleEnabled = false;
};

// GUI state that is separate from render parameters.
// This keeps interface-only values away from the actual scene setup.
struct GuiState {
    SelectedObject selectedObject = SelectedObject::Diamond;
    TransformMode transformMode = TransformMode::Rotate;

    float moveStep = 10.0f;
    float rotateStep = 5.0f;
    float cameraStep = 20.0f;
    float fovStep = 2.0f;

    std::atomic<bool> isRendering = false;

    std::mutex statusMutex;
    std::string status = "Ready.";
    GLuint renderedTexture = 0;
    int renderedImageWidth = 0;
    int renderedImageHeight = 0;
    bool imageReady = false;
};

static void setStatus(GuiState& state, const std::string& text) {
    std::lock_guard<std::mutex> lock(state.statusMutex);
    state.status = text;
}

static std::string getStatus(GuiState& state) {
    std::lock_guard<std::mutex> lock(state.statusMutex);
    return state.status;
}

static bool loadTextureFromFile(const std::string& filename, GLuint& outTexture, int& outWidth, int& outHeight) {
    cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);

    if (image.empty()) {
        return false;
    }

    cv::cvtColor(image, image, cv::COLOR_BGR2RGBA);

    outWidth = image.cols;
    outHeight = image.rows;

    if (outTexture != 0) {
        glDeleteTextures(1, &outTexture);
    }

    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        outWidth,
        outHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.data
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

// Helper for editing Vector3f values through ImGui.
// This replaces the old readVec3 terminal helper.
static void inputVec3(const char* label, Vector3f& value) {
    float values[3] = { value.x, value.y, value.z };

    if (ImGui::InputFloat3(label, values)) {
        value.x = values[0];
        value.y = values[1];
        value.z = values[2];
    }
}

// Helper for editing file paths without requiring imgui_stdlib.h.
static void inputString(const char* label, std::string& value) {
    char buffer[512];

    std::strncpy(buffer, value.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
        value = buffer;
    }
}

static void drawHelpPanel() {
    ImGui::Begin("Controls Help");

    ImGui::TextWrapped("This GUI works like a lightweight scene editor.");
    ImGui::TextWrapped("It replaces the previous terminal setup where parameters were entered manually.");
    ImGui::Separator();

    ImGui::Text("Move mode");
    ImGui::BulletText("Left / Right: move X");
    ImGui::BulletText("Up / Down: move Z");
    ImGui::BulletText("Q / E: move Y");

    ImGui::Separator();

    ImGui::Text("Rotate mode");
    ImGui::BulletText("Left / Right: rotate Y");
    ImGui::BulletText("Up / Down: rotate X");
    ImGui::BulletText("Q / E: rotate Z");

    ImGui::Separator();

    ImGui::Text("Camera mode");
    ImGui::BulletText("Left / Right: move camera X");
    ImGui::BulletText("Up / Down: move camera Z");
    ImGui::BulletText("Q / E: move camera Y");
    ImGui::BulletText("W / S: narrow / widen FOV");

    ImGui::End();
}

// Interactive setup
// Instead of asking for parameters with std::getline, the GUI keeps parameters live.
// Keyboard controls provide a Blender-like way to move/rotate the selected object.
static void applyKeyboardControls(RenderParams& params, GuiState& state) {
    // Avoid moving objects while typing into text/input fields.
    // This prevents arrow keys from changing the scene while editing a number box.
    if (ImGui::IsAnyItemActive()) {
        return;
    }

    bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true);
    bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, true);
    bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);
    bool down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);

    bool q = ImGui::IsKeyPressed(ImGuiKey_Q, true);
    bool e = ImGui::IsKeyPressed(ImGuiKey_E, true);
    bool w = ImGui::IsKeyPressed(ImGuiKey_W, true);
    bool s = ImGui::IsKeyPressed(ImGuiKey_S, true);

    float moveStep = state.moveStep;
    float rotateStep = state.rotateStep;
    float cameraStep = state.cameraStep;

    if (state.transformMode == TransformMode::Move) {
        Vector3f* target = nullptr;

        if (state.selectedObject == SelectedObject::Diamond) {
            target = &params.diamondPos;
        } else if (state.selectedObject == SelectedObject::Stone) {
            target = &params.stonePos;
        } else if (state.selectedObject == SelectedObject::MainLight) {
            target = &params.lightPos;
        } else if (state.selectedObject == SelectedObject::BackLight) {
            target = &params.lightBackPos;
        } else if (state.selectedObject == SelectedObject::Camera) {
            target = &params.cameraPos;
        }

        if (target != nullptr) {
            if (left) target->x -= moveStep;
            if (right) target->x += moveStep;
            if (up) target->z += moveStep;
            if (down) target->z -= moveStep;
            if (q) target->y += moveStep;
            if (e) target->y -= moveStep;
        }
    }

    if (state.transformMode == TransformMode::Rotate) {
        float* rotX = nullptr;
        float* rotY = nullptr;
        float* rotZ = nullptr;

        if (state.selectedObject == SelectedObject::Diamond) {
            rotX = &params.diamondRotX;
            rotY = &params.diamondRotY;
            rotZ = &params.diamondRotZ;
        } else if (state.selectedObject == SelectedObject::Stone) {
            rotX = &params.stoneRotX;
            rotY = &params.stoneRotY;
            rotZ = &params.stoneRotZ;
        }

        if (rotX != nullptr && rotY != nullptr && rotZ != nullptr) {
            if (left) *rotY -= rotateStep;
            if (right) *rotY += rotateStep;
            if (up) *rotX += rotateStep;
            if (down) *rotX -= rotateStep;
            if (q) *rotZ -= rotateStep;
            if (e) *rotZ += rotateStep;
        }
    }

    if (state.transformMode == TransformMode::Camera) {
        // Camera navigation moves both the camera position and look-at point together.
        // This behaves like panning through the scene.
        if (left) {
            params.cameraPos.x -= cameraStep;
            params.cameraLookAt.x -= cameraStep;
        }

        if (right) {
            params.cameraPos.x += cameraStep;
            params.cameraLookAt.x += cameraStep;
        }

        if (up) {
            params.cameraPos.z += cameraStep;
            params.cameraLookAt.z += cameraStep;
        }

        if (down) {
            params.cameraPos.z -= cameraStep;
            params.cameraLookAt.z -= cameraStep;
        }

        if (q) {
            params.cameraPos.y += cameraStep;
            params.cameraLookAt.y += cameraStep;
        }

        if (e) {
            params.cameraPos.y -= cameraStep;
            params.cameraLookAt.y -= cameraStep;
        }

        if (w) {
            params.cameraFov -= state.fovStep;
            if (params.cameraFov < 10.0f) {
                params.cameraFov = 10.0f;
            }
        }

        if (s) {
            params.cameraFov += state.fovStep;
            if (params.cameraFov > 100.0f) {
                params.cameraFov = 100.0f;
            }
        }
    }
}

// 3. Create a scene using params
// This is the original render setup moved out of main() so the GUI can call it.
static void renderScene(RenderParams params, GuiState& guiState) {
    guiState.isRendering = true;

    auto start = std::chrono::system_clock::now();

    setStatus(guiState, "Building scene...");

    Scene scene(params.width, params.height);
    scene.spp = params.spp;
    scene.RussianRoulette = params.russianRoulette;
    scene.enableShadows = params.enableShadows;

    // These are the existing global controls used by the renderer/material code.
    // They are set from the GUI before the scene is rendered.
    waveGlint = params.waveGlint;
    photonMapEnabled = params.photonMapEnabled;
    glintEnabled = params.glintEnabled;
    heroSampleEnabled = params.heroSampleEnabled;

    // change the resolution for quick debugging if rendering is slow
    // Scene scene(64, 64);
    // Scene scene(128, 128);
    // Scene scene(256, 256);
    // Scene scene(512, 512);
    // Scene scene(1024, 1024);

    // scene.RussianRoulette = 0.8;
    // scene.spp = 256;

    scene.cameraPos = params.cameraPos;
    scene.cameraLookAt = params.cameraLookAt;
    scene.fov = params.cameraFov;

    Material* white = new Material(DIFFUSE, Vector3f(0.48f, 0.45f, 0.4f));

    Material* light = new Material(EMIT, Vector3f(1));
    light->m_emission = 10;

    // Add the square floor mesh in the scene(But in the code, floor made by the c++ code.)
    // Add mirror in the scene
    // Mapping texture on the mirror
    Material* mirrorMat = new Material(MIRROR, Vector3f(1.0f));

    Material* glintFloor = new Material(GLINT, Vector3f(0.9f));
    glintFloor->textured = false;
    glintFloor->roughness = glintEnabled ? 0.008f : 0.03f;

    // Add temporary light.obj(You can change the light source location as you wish!)
    //MeshTriangle floor("../models/floor/floor.obj", Vector3f(0), white);
    MeshTriangle mirror("../models/mirror/mirror.obj", Vector3f(278, 0, 540), mirrorMat);

    // Lights (experimental)
    if (params.numLights >= 1) {
        scene.Add(new MeshTriangle("../models/light/light.obj", params.lightPos, light));
    }

    if (params.numLights >= 2) {
        scene.Add(new MeshTriangle("../models/light/light.obj", params.lightBackPos, light));
    }

    // MeshTriangle light_("../models/light/light.obj",Vector3f(0,-5,0), light);
    // MeshTriangle light_back("../models/light/light.obj", Vector3f(0, -5, -500), light);

    //scene.Add(&floor);
    scene.Add(&mirror);
    // scene.Add(&light_);
    // scene.Add(&light_back);

    // Mapping the texture on the rock
    Material* rock_mat = new Material(DIFFUSE, Vector3f(1.0f));

    rock_mat->diffuse_map = new Texture(params.rockTexturePath); // jpg or png texture img adjust

    // Normal Mapping(Bump Shading)
    rock_mat->normal_map = new Texture(params.rockNormalPath);

    rock_mat->textured = true;

    // Add the rock mesh
    scene.Add(new MeshTriangle(
        "../models/rock/rock.obj",
        params.stonePos,
        rock_mat,
        params.stoneRotX,
        params.stoneRotY,
        params.stoneRotZ
    ));

    Material* diamond_mat = new Material(GLASS, Vector3f(1));
    diamond_mat->ior = 2.4;
    diamond_mat->cauchyA = 2.3849f;
    diamond_mat->cauchyB = 0.01942f;

    //diamond mesh with glass material
    MeshTriangle* diamond = new MeshTriangle(
        "../models/diamond/diamond.obj",
        params.diamondPos,
        diamond_mat,
        params.diamondRotX,
        params.diamondRotY,
        params.diamondRotZ
    );

    scene.Add(diamond);

    //floor code
    Vector3f verts[4] = {
        {0, 0, 0},
        {552.8, 0, 0},
        {549.6, 0, 559.2},
        {0, 0, 559.2}
    };

    Vector2f st[4] = {
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1}
    };

    uint32_t vertIndex[6] = {0, 2, 1, 2, 0, 3};

    Material* mfloor = new Material(DIFFUSE, Vector3f(1));

    // No texture for floor
    mfloor->textured = false;

    scene.Add(new MeshTriangle(verts, vertIndex, 2, st, glintFloor));

    setStatus(guiState, "Building BVH...");
    scene.buildBVH();

    if (photonMapEnabled) {
        setStatus(guiState, "Building photon map...");
        scene.buildPhotonMap(params.photonCount);
    }

    setStatus(guiState, "Rendering final image...");

    Renderer r;
    r.Render(scene);
    guiState.imageReady = true;

    auto stop = std::chrono::system_clock::now();

    std::cout << "Render complete: \n";
    std::cout << "Time taken: "
              << std::chrono::duration_cast<std::chrono::hours>(stop - start).count()
              << " hours\n";
    std::cout << "          : "
              << std::chrono::duration_cast<std::chrono::minutes>(stop - start).count()
              << " minutes\n";
    std::cout << "          : "
              << std::chrono::duration_cast<std::chrono::seconds>(stop - start).count()
              << " seconds\n";

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(stop - start).count();

    std::stringstream ss;
    ss << "Render complete in " << seconds << " seconds.";

    setStatus(guiState, ss.str());

    guiState.isRendering = false;
}

static void drawMainGui(RenderParams& params, GuiState& guiState) {
    ImGui::Begin("Diamond Renderer Scene Editor");

    ImGui::TextWrapped(
        "Select an object, choose a mode, then use the arrow keys, Q/E, and W/S to adjust the scene."
    );

    ImGui::Separator();

    const char* objectNames[] = {
        "Diamond",
        "Stone",
        "Camera",
        "Main Light",
        "Back Light"
    };

    int selectedObjectIndex = static_cast<int>(guiState.selectedObject);

    if (ImGui::Combo("Selected object", &selectedObjectIndex, objectNames, IM_ARRAYSIZE(objectNames))) {
        guiState.selectedObject = static_cast<SelectedObject>(selectedObjectIndex);
    }

    const char* modeNames[] = {
        "Move",
        "Rotate",
        "Camera"
    };

    int modeIndex = static_cast<int>(guiState.transformMode);

    if (ImGui::Combo("Transform mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
        guiState.transformMode = static_cast<TransformMode>(modeIndex);
    }

    ImGui::InputFloat("Move step", &guiState.moveStep);
    ImGui::InputFloat("Rotate step", &guiState.rotateStep);
    ImGui::InputFloat("Camera step", &guiState.cameraStep);

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Render Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Width", &params.width);
        ImGui::InputInt("Height", &params.height);

        if (params.width < 1) params.width = 1;
        if (params.height < 1) params.height = 1;

        // Typed input instead of a slider, so users can enter the exact SPP value they want.
        ImGui::InputInt("Samples per pixel", &params.spp);
        if (params.spp < 1) params.spp = 1;

        ImGui::Checkbox("Enable shadows", &params.enableShadows);
        ImGui::Checkbox("Enable photon map", &params.photonMapEnabled);

        ImGui::InputInt("Photon count", &params.photonCount);

        if (params.photonCount < 0) {
            params.photonCount = 0;
        }

        ImGui::SliderFloat("Russian roulette", &params.russianRoulette, 0.1f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Diamond", ImGuiTreeNodeFlags_DefaultOpen)) {
        inputVec3("Diamond position", params.diamondPos);
        ImGui::SliderFloat("Diamond rotation X", &params.diamondRotX, -180.0f, 180.0f);
        ImGui::SliderFloat("Diamond rotation Y", &params.diamondRotY, -180.0f, 180.0f);
        ImGui::SliderFloat("Diamond rotation Z", &params.diamondRotZ, -180.0f, 180.0f);

        ImGui::SeparatorText("Diamond custom controls");
        ImGui::Checkbox("Wave glint", &params.waveGlint);
        ImGui::Checkbox("Glint", &params.glintEnabled);
        ImGui::Checkbox("Hero wavelength sample", &params.heroSampleEnabled);
    }

    if (ImGui::CollapsingHeader("Stone", ImGuiTreeNodeFlags_DefaultOpen)) {
        inputVec3("Stone position", params.stonePos);
        ImGui::SliderFloat("Stone rotation X", &params.stoneRotX, -180.0f, 180.0f);
        ImGui::SliderFloat("Stone rotation Y", &params.stoneRotY, -180.0f, 180.0f);
        ImGui::SliderFloat("Stone rotation Z", &params.stoneRotZ, -180.0f, 180.0f);
        inputString("Stone texture map", params.rockTexturePath);
        inputString("Stone normal map", params.rockNormalPath);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        inputVec3("Camera position", params.cameraPos);
        inputVec3("Camera look-at", params.cameraLookAt);
        ImGui::SliderFloat("Camera FOV", &params.cameraFov, 10.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Number of lights", &params.numLights, 0, 2);
        inputVec3("Main light position", params.lightPos);
        inputVec3("Back light position", params.lightBackPos);
        ImGui::InputFloat("Light size", &params.lightSize);
    }

    if (ImGui::CollapsingHeader("Environment and Texture Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
        inputString("Environment map", params.envMap);
        inputString("Texture map", params.textureMap);
    }

    ImGui::Separator();

    ImGui::TextWrapped("Status: %s", getStatus(guiState).c_str());

    if (ImGui::CollapsingHeader("Rendered Image", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (guiState.imageReady) {
        if (loadTextureFromFile("binary_task2.ppm", guiState.renderedTexture, guiState.renderedImageWidth, guiState.renderedImageHeight)) {
            float previewWidth = ImGui::GetContentRegionAvail().x;
            float aspect = (float)guiState.renderedImageHeight / (float)guiState.renderedImageWidth;

            ImGui::Image(
                (ImTextureID)(intptr_t)guiState.renderedTexture,
                ImVec2(previewWidth, previewWidth * aspect)
            );
        } else {
            ImGui::TextWrapped("Rendered image file not found. Check the renderer output filename.");
        }
    } else {
        ImGui::TextWrapped("No image rendered yet.");
    }
}

    bool canRender = !guiState.isRendering;

    if (!canRender) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Render final image")) {
        std::thread renderThread(renderScene, params, std::ref(guiState));
        renderThread.detach();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset defaults")) {
        params = RenderParams();
        setStatus(guiState, "Defaults restored.");
    }

    if (!canRender) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

int main(int argc, char** argv) {
    RenderParams params;
    GuiState guiState;

    // debugging.
    std::cout << "Starting GUI..." << std::endl;

    glfwSetErrorCallback([](int error, const char* description) {
        std::cerr << "GLFW error " << error << ": " << description << std::endl;
    });

    if (!glfwInit()) {
        std::cerr << "Failed to initialise GLFW." << std::endl;
        return 1;
    }

    const char* glslVersion = "#version 120";

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(
        900,
        600,
        "Interactive Renderer",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg]        = ImVec4(0.16f, 0.04f, 0.12f, 1.00f); // deep pink
    colors[ImGuiCol_TitleBg]         = ImVec4(0.45f, 0.10f, 0.32f, 1.00f);
    colors[ImGuiCol_TitleBgActive]   = ImVec4(0.95f, 0.38f, 0.70f, 1.00f);

    colors[ImGuiCol_Header]          = ImVec4(0.95f, 0.55f, 0.20f, 0.75f); // yellow-orange
    colors[ImGuiCol_HeaderHovered]   = ImVec4(1.00f, 0.70f, 0.25f, 0.90f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(1.00f, 0.80f, 0.30f, 1.00f);

    colors[ImGuiCol_Button]          = ImVec4(0.95f, 0.35f, 0.68f, 1.00f); // pink
    colors[ImGuiCol_ButtonHovered]   = ImVec4(1.00f, 0.55f, 0.78f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(1.00f, 0.75f, 0.25f, 1.00f); // yellow

    colors[ImGuiCol_FrameBg]         = ImVec4(0.35f, 0.10f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.65f, 0.20f, 0.45f, 1.00f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.95f, 0.55f, 0.20f, 1.00f);

    colors[ImGuiCol_CheckMark]       = ImVec4(1.00f, 0.85f, 0.20f, 1.00f);
    colors[ImGuiCol_SliderGrab]      = ImVec4(1.00f, 0.75f, 0.25f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]= ImVec4(1.00f, 0.90f, 0.35f, 1.00f);

    colors[ImGuiCol_Text]            = ImVec4(1.00f, 0.92f, 0.75f, 1.00f); // warm cream
    colors[ImGuiCol_Separator]       = ImVec4(1.00f, 0.70f, 0.25f, 0.80f);

    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        applyKeyboardControls(params, guiState);

        drawMainGui(params, guiState);
        drawHelpPanel();

        ImGui::Render();

        int displayW;
        int displayH;

        glfwGetFramebufferSize(window, &displayW, &displayH);

        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
