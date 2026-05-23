#include "Renderer.hpp"
#include "Scene.hpp"
#include "Triangle.hpp"
#include "Sphere.hpp"
#include "Vector.hpp"
#include "global.hpp"
#include <chrono>

float TASK_N = 2; // 1.1, 1.2, 1.3, 2

int main(int argc, char** argv)
{

    // 1. Default parameters
    int width = 512;
    int height = 512;
    int spp = 1024;

    // Vector3f cameraPos(278, 273, -800);
    Vector3f diamondPos(350, 0, 250);
    float diamondRotX = -12.0f;
    float diamondRotZ = 30.0f;
    float diamondRotY = 180.0f;

    // bool enableShadows = true;
    int numLights = 2;
    Vector3f lightPos(0, -5, 0);
    Vector3f lightBackPos(0, -5, -500);
    // float lightSize = 1.0f;

    std::string rockTexturePath = "../models/rock/initialShadingGroup_Base_color.png";
    std::string rockNormalPath = "../models/rock/rock_normals.jpg";

    std::string envMap = "none";

    // Interactive Set up

    std::cout << "Interactive render setup\n";
    std::cout << "(Press Enter to keep default)\n\n";

    std::string input;

    std::cout << "Samples per pixel [default " << spp << "] ";
    std::getline(std::cin, input);
    if (!input.empty()) spp = std::stoi(input);

    std::cout << "Diamond X [default " << diamondPos.x << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondPos.x = std::stof(input);

    std::cout << "Diamond Y [default "<< diamondPos.y << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondPos.y = std::stof(input);

    std::cout << "Diamond Z [default " << diamondPos.z << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondPos.z = std::stof(input);

    std::cout << "Diamond rotation X [default "<< diamondRotX << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondRotX = std::stof(input);

    std::cout << "Diamond rotation Y [default " << diamondRotY << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondRotY = std::stof(input);

    std::cout << "Diamond rotation Z [default " << diamondRotZ << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) diamondRotZ = std::stof(input);

    std::cout << "Number of lights [default " << numLights << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) numLights = std::stoi(input);

    std::cout << "Main light X [default " << lightPos.x << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) lightPos.x = std::stof(input);
    
    std::cout << "Main light Y [default " << lightPos.y << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) lightPos.y = std::stof(input);
    
    std::cout << "Main light Z [default " << lightPos.z << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) lightPos.z = std::stof(input);

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

    Material* white = new Material(DIFFUSE, Vector3f(0.48f, 0.45f, 0.4f));
    Material* light = new Material(EMIT, Vector3f(1));
    light->m_emission=10;    
    // Add the square floor mesh in the scene(But in the code, floor made by the c++ code.)
    // Add mirror in the scene
    // Mapping texture on the mirror
    Material* mirrorMat = new Material(MIRROR, Vector3f(1.0f));
    // Add temporary light.obj(You can change the light source location as you wish!)
    MeshTriangle floor("../models/floor/floor.obj", Vector3f(0), white); 
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

    scene.Add(&floor);
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

    // floor code
    Vector3f verts[4] = {{0,0,0}, {552.8,0,0}, {549.6, 0,559.2}, {0,0,559.2}};
    Vector2f st[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    uint32_t vertIndex[6] = {0, 2, 1, 2,0,3};
    Material* mfloor=new Material(DIFFUSE, Vector3f(1));
    // No texture for floor
    mfloor->textured=false;
    scene.Add(new MeshTriangle(verts, vertIndex, 2,st,mfloor));

    scene.buildBVH();

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