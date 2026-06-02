#include "Renderer.hpp"
#include "Scene.hpp"
#include "Triangle.hpp"
#include "Sphere.hpp"
#include "Vector.hpp"
#include "global.hpp"
#include <chrono>
#include <sstream>

float TASK_N = 2; // 1.1, 1.2, 1.3, 2
bool waveGlint = false;
bool photonMapEnabled = false;

int main(int argc, char** argv)
{

    // 1. Default parameters
    int width = 1024;
    int height = 1024;
    int spp = 256;


    Vector3f cameraPos(278, 273, -800);
    Vector3f cameraLookAt(278, 273, 0);
    float cameraFov = 40.0f;

    // Vector3f cameraPos(278, 273, -800);
    Vector3f diamondPos(350, 0, 250);
    float diamondRotX = -12.0f;
    float diamondRotZ = 40.0f;
    float diamondRotY = 180.0f;

    // bool enableShadows = true;
    int numLights = 2;
    Vector3f lightPos(0, -5, 0);
    Vector3f lightBackPos(0, -5, -500);


    std::string rockTexturePath = "../models/rock/initialShadingGroup_Base_color.png";
    std::string rockNormalPath = "../models/rock/rock_normals.jpg";

    std::string envMap = "none";

    // Interactive Set up

    std::cout << "Interactive render setup\n";
    std::cout << "(Press Enter to keep default. For 3D values, enter: x y z)\n\n";

    std::string input;

    auto readVec3 = [](const std::string& label, Vector3f& value) {
        std::string input;

        std::cout << label
                << " [default "
                << value.x << " "
                << value.y << " "
                << value.z << "]: ";

        std::getline(std::cin, input);

        if (!input.empty()) {
            std::stringstream ss(input);
            ss >> value.x >> value.y >> value.z;
        }
    };

    auto readFloat3 = [](const std::string& label,
                        float& x,
                        float& y,
                        float& z) {
        std::string input;

        std::cout << label
                << " [default "
                << x << " "
                << y << " "
                << z << "]: ";

        std::getline(std::cin, input);

        if (!input.empty()) {
            std::stringstream ss(input);
            ss >> x >> y >> z;
        }
    };

    std::cout << "Samples per pixel [default " << spp << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) spp = std::stoi(input);

    readVec3("Diamond position x y z", diamondPos);

    readFloat3("Diamond rotation x y z",
            diamondRotX,
            diamondRotY,
            diamondRotZ);

    std::cout << "Number of lights [default " << numLights << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) numLights = std::stoi(input);

    readVec3("Main light position x y z", lightPos);

    readVec3("Camera position x y z", cameraPos);

    readVec3("Camera look-at x y z", cameraLookAt);

    std::cout << "Camera FOV [default " << cameraFov << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) cameraFov = std::stof(input);

    // 3. Create a scene using params
    Scene scene(width, height);
    scene.spp = spp;
    scene.RussianRoulette = 0.8;
    

    
    // change the resolution for quick debugging if rendering is slow
    // Scene scene(64, 64);
    // Scene scene(128, 128);
    // Scene scene(256, 256);
    // Scene scene(512, 512);
    // Scene scene(1024, 1024);

    // scene.RussianRoulette = 0.8;
    // scene.spp = 256;  

    scene.cameraPos = cameraPos;
    scene.cameraLookAt = cameraLookAt;
    scene.fov = cameraFov;

    Material* white = new Material(DIFFUSE, Vector3f(0.48f, 0.45f, 0.4f));
    Material* light = new Material(EMIT, Vector3f(1));
    light->m_emission=10;    
    // Add the square floor mesh in the scene(But in the code, floor made by the c++ code.)
    // Add mirror in the scene
    // Mapping texture on the mirror
    Material* mirrorMat = new Material(MIRROR, Vector3f(1.0f));
    Material* glintFloor = new Material(GLINT, Vector3f(0.9f));
    glintFloor -> textured = false;
    glintFloor -> roughness = 0.008f;
    // Add temporary light.obj(You can change the light source location as you wish!)
    //MeshTriangle floor("../models/floor/floor.obj", Vector3f(0), white); 
    MeshTriangle mirror("../models/mirror/mirror.obj",Vector3f(278, 0, 540), mirrorMat);
    // Lights (experimental)
    if (numLights >= 1){
        scene.Add(new MeshTriangle("../models/light/light.obj", lightPos, light));
    }

    if (numLights >= 2){
        scene.Add(new MeshTriangle("../models/light/light.obj", lightBackPos, light));
    }



    // MeshTriangle light_("../models/light/light.obj",Vector3f(0,-5,0), light);
    // MeshTriangle light_back("../models/light/light.obj", Vector3f(0, -5, -500), light);

    //scene.Add(&floor);
    scene.Add(&mirror);
    // scene.Add(&light_);
    // scene.Add(&light_back);

    // Mapping the texture on the rock
    Material* rock_mat = new Material(DIFFUSE, Vector3f(1.0f));

    rock_mat->diffuse_map = new Texture(rockTexturePath); // jpg or png texture img adjust

    // Normal Mapping(Bump Shading)
    rock_mat->normal_map = new Texture(rockNormalPath);

    rock_mat->textured = true;

    // Add the rock mesh
    scene.Add(new MeshTriangle("../models/rock/rock.obj", Vector3f(180, 0, 300),rock_mat));
    
    Material* diamond_mat = new Material(GLASS, Vector3f(1));
    diamond_mat->ior = 2.4;
    diamond_mat->cauchyA = 2.3849f;
    diamond_mat->cauchyB = 0.01942f;
     //diamond mesh with glass material
     MeshTriangle* diamond = new MeshTriangle("../models/diamond/diamond.obj", diamondPos, diamond_mat, diamondRotX, diamondRotY, diamondRotZ);
     scene.Add(diamond);

    //floor code
    Vector3f verts[4] = {{0,0,0}, {552.8,0,0}, {549.6, 0,559.2}, {0,0,559.2}};
    Vector2f st[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    uint32_t vertIndex[6] = {0, 2, 1, 2,0,3};
    Material* mfloor=new Material(DIFFUSE, Vector3f(1));
    // No texture for floor
    mfloor->textured=false;
    scene.Add(new MeshTriangle(verts, vertIndex, 2,st,glintFloor));

    scene.buildBVH();

    if (photonMapEnabled) scene.buildPhotonMap(500000);

    Renderer r;

    auto start = std::chrono::system_clock::now();
    r.Render(scene);
    auto stop = std::chrono::system_clock::now();

    std::cout << "Render complete: \n";
    std::cout << "Time taken: " << std::chrono::duration_cast<std::chrono::hours>(stop - start).count() << " hours\n";
    std::cout << "          : " << std::chrono::duration_cast<std::chrono::minutes>(stop - start).count() << " minutes\n";
    std::cout << "          : " << std::chrono::duration_cast<std::chrono::seconds>(stop - start).count() << " seconds\n";

    return 0;
}