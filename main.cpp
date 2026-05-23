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
    int spp = 256;

    // Vector3f cameraPos(278, 273, -800);
    Vector3f diamondPos(350, 0, 250);
    float diamondRotX = -12.0f;
    float diamondRotZ = 18.0f;

    // bool enableShadows = true;
    int numLights = 2;
    Vector3f lightPos(0, -5, 0);
    Vector3f lightBackPos(0, -5, -500);
    // float lightSize = 1.0f;

    std::string rockTexturePath = "../models/rock/initialShadingGroup_Base_color.png";
    std::string rockNormalPath = "../models/rock/rock_normals.jpg";

    std::string envMap = "none";

    // 2. Override defaults from cmd
    // Example:
    // ./Render 2 256 350 0 250 -12 18 2 0 -5 0
    //
    // argv[1] = TASK_N
    // argv[2] = spp
    // argv[3] = diamond x
    // argv[4] = diamond y
    // argv[5] = diamond z
    // argv[6] = diamond rotation x
    // argv[7] = diamond rotation z
    // argv[8] = number of lights
    // argv[9] = main light x
    // argv[10] = main light y
    // argv[11] = main light z
    // argv[12] = rock texture path, optional
    // argv[13] = rock normal map path, optional

    // param 1
    if (argc >= 2)
        TASK_N=(float)atof(argv[1]);

    // param 2
    if (argc >= 3) 
        spp = atoi(argv[2]);

    // param 6
    if (argc >= 6)
        diamondPos = Vector3f(atof(argv[3]), atof(argv[4]), atof(argv[5]));

    // param 8
    if (argc >= 8) {
        diamondRotX = atof(argv[6]);
        diamondRotZ = atof(argv[7]);
    }

    // param 9
    if (argc >= 9)
        numLights = atoi(argv[8]);

    // param 12
    if (argc >= 12)
        lightPos = Vector3f(atof(argv[9]), atof(argv[10]), atof(argv[11]));

    // param 13
    if (argc >= 13)
        rockTexturePath = argv[12];

    // param 14
    if (argc >= 14)
        rockNormalPath = argv[13];

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
     MeshTriangle* diamond = new MeshTriangle("../models/diamond/diamond.obj", diamondPos, diamond_mat);
     // Add inclination to diamond so it leans more naturally
     // lean backwards
     //diamond->rotate(Vector3f(1, 0, 0), diamondRotX);
     // tip sideways
     //diamond->rotate(Vector3f(0, 0, 1), diamondRotZ);
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
