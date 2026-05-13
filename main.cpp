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
    if (argc >= 2)
        TASK_N=(float)atof(argv[1]);
    // change the resolution for quick debugging if rendering is slow
    // Scene scene(64, 64);
    // Scene scene(128, 128);
    // Scene scene(256, 256);
    Scene scene(512, 512);
    // Scene scene(1024, 1024);

    scene.RussianRoulette = 0.8;
    scene.spp = 64;
    // scene.spp = 1;  

    Material* white = new Material(DIFFUSE, Vector3f(0.48f, 0.45f, 0.4f));
    Material* light = new Material(EMIT, Vector3f(1));
    light->m_emission=100;
    
    // Add the square floor mesh in the scene(But in the code, floor made by the c++ code.)
    // Add mirror in the scene
    // Add temporary light.obj(You can change the light source location as you wish!)
    MeshTriangle floor("../models/floor/floor.obj", Vector3f(0), white); 
    MeshTriangle mirror("../models/mirror/mirror.obj",Vector3f(278, 0, 540), new Material(MIRROR, Vector3f(1)));
    MeshTriangle light_("../models/light/light.obj",Vector3f(0,-5,0), light);
    MeshTriangle light_back("../models/light/light.obj", Vector3f(0, -5, -500), light);

    scene.Add(&floor);
    scene.Add(&mirror);
    scene.Add(&light_);
    scene.Add(&light_back);

    // Add the rock mesh
    scene.Add(new MeshTriangle("../models/rock/rock.obj", Vector3f(180, 0, 300),new Material(DIFFUSE, Vector3f(1))));
    
     // Mapping the texture on the rock

     // Add the diamond mesh with glass material
    scene.Add(new MeshTriangle("../models/diamond/diamond.obj", Vector3f(350, 0, 250), new Material(GLASS, Vector3f(1))));

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
