//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"
#include "SpectralSample.hpp"


void Scene::buildBVH() {
    printf(" - Generating BVH...\n\n");
    this->bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}


void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    float emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            pos.happened=true;  // area light that has emission exists
            emit_area_sum += objects[k]->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            emit_area_sum += objects[k]->getArea();
            if (p <= emit_area_sum){
                objects[k]->Sample(pos, pdf);
                break;
            }
        }
    }
}

// Implementation of Path Tracing
Vector3f Scene::castRay(const Ray &ray, int depth, SampledWavelengths &sw) const
{   
    // Prevent infinite recursion
    // Stop tracing if depth exceeds maxDepth.
    if (depth > maxDepth) {
        return Vector3f(0.0f); 
    }

    Vector3f hitColor = Vector3f(0);
    auto inter = intersect(ray);  // find the cloest intersection of the ray and the objects
    if (!inter.happened) return backgroundColor;  // if no intersection, return background color

    Vector3f hitPoint = inter.coords;  // the intersection point
    Vector3f N = inter.normal; // normal
    Vector2f st = inter.tcoords; // texture coordinates (u, v)
    Vector3f dir = ray.direction;  // ray direction

    auto getColor = [&](const Intersection& i) -> Vector3f {
         if (i.material->textured){
            float u, v;
            u = i.tcoords.x;
            v = i.tcoords.y;
            return i.material->diffuse_map->getColor(u, v);
        }

        return i.material->m_color;
    };
    

    // Apply Bump(Normal Mapping)
    if (inter.material->normal_map != nullptr) {
        float u = st.x;
        float v = st.y;

        // 1. Set up variables for TBN matrix calculation
        Vector3f n = normalize(N);
        float x = n.x, y = n.y, z = n.z;

        Vector3f t, b;
        float denominator = std::sqrt(x*x + z*z);
        
        // Prevent division by zero when the normal is perfectly vertical (x=0, z=0)
        if (denominator < 1e-6f) { 
            t = Vector3f(1.0f, 0.0f, 0.0f);
        } else {
            t = Vector3f(x*y/denominator, denominator, z*y/denominator);
        }
        b = crossProduct(n, t);

        // 2. Fetch value from texture
        // Since 'rock_normals.jpg' is a normal map with vector values in RGB,
        // I directly map the colors to the [-1, 1] range instead of using derivatives (dU, dV))
        Vector3f map_n = inter.material->normal_map->getColor(u, v);
        Vector3f ln = Vector3f(map_n.x * 2.0f - 1.0f,
                               map_n.y * 2.0f - 1.0f,
                               map_n.z * 2.0f - 1.0f);
        ln = normalize(ln);

        // 3. Transform to world space normal using the TBN matrix and overwrite N
        N = normalize(t * ln.x + b * ln.y + n * ln.z);
    }

    if (inter.material->m_type == EMIT) {  // if the object is a light source
        return inter.material->m_emission;  // return light color

    } else if (TASK_N == 2) {
            // TODO: task 2 Monte Carlo Path Tracing with Russian Roulette termination
            Vector3f Li = 1, Lo = 0, wi = ray.direction, curN = N, b1, b2;
            Vector3f tint = 1;
            Intersection curInter = inter;
            for (int i = 0; i < maxDepth; i++){
                if ((get_random_float() > RussianRoulette && i > 3) || !curInter.happened) break;
                if (curInter.material->m_type == EMIT) {Lo += Li * curInter.material->m_emission; break;}
                if (curInter.material -> m_type == MIRROR){
                    wi = normalize(reflect(wi, curN)); 
                    Li = Li * getColor(curInter);
                    curInter = intersect(Ray(curInter.coords + curN * EPSILON, wi));
                    if (!curInter.happened) {Lo += Li * backgroundColor; break;}
                    curN = curInter.normal; continue;
                }
                else if (curInter.material -> m_type == GLASS) {
                    float heroIOR = curInter.material->iorAtLambda(sw.lambda[0]);
                    float kr = fresnel(wi, curN, heroIOR);
                    Vector3f faceN = dotProduct(wi, curN) < 0 ? curN : -curN;
                    if (get_random_float() < kr){
                        wi = normalize(reflect(wi, faceN));
                    } else {
                        if (!sw.SecondaryTerminated()) {
                            sw.TerminateSecondary();
                            Li = Li * wavelengthToRGBGaussian(sw.lambda[0]);
                        }
                        wi = normalize(refract(wi, faceN, heroIOR));
                    }
                    curInter = intersect(Ray(curInter.coords + wi * EPSILON, wi));
                    if (!curInter.happened) { Lo += Li * backgroundColor; break; }
                    curN = curInter.normal; continue;
                }
                Intersection lightInter; float pdf;
                sampleLight(lightInter, pdf);
                Vector3f lightVec = lightInter.coords - curInter.coords, lightDir = normalize(lightVec);
                float dist2 = dotProduct(lightVec, lightVec), cosLight = std::max(0.0f, dotProduct(lightInter.normal, -lightDir));

                Vector3f offsetN = dotProduct(curN, lightDir) > 0 ? curN : -curN;
                Ray shadowRay(curInter.coords + offsetN * EPSILON + lightDir * EPSILON, lightDir);
                Intersection shadowInter = intersect(shadowRay);
                bool notBlocked = !shadowInter.happened || shadowInter.material == nullptr || 
                                shadowInter.tnear >= lightVec.norm() * 0.999f || shadowInter.material -> m_type == EMIT;
                Li = Li * getColor(curInter);
                if (notBlocked && pdf >= 1e-6) { 
                    Vector3f lightIntensity = lightInter.material->m_emission;
                    float cosine = std::max(0.0f, dotProduct(curN, lightDir));
                    Vector3f contrib= Li * (float)M_PI * lightIntensity * cosine * cosLight / (pdf * dist2) / RussianRoulette;
                    contrib.x = std::min(contrib.x, 5.0f), contrib.y = std::min(contrib.y, 5.0f), contrib.z = std::min(contrib.z, 5.0f);
                    Lo += contrib;
                } 

                // Caustic lookup — add after the existing direct light contribution
                if (causticMap.built) {
                    std::vector<const Photon*> nearby;
                    float radius = 10.f; // tune: larger = smoother, smaller = sharper
                    causticMap.query(curInter.coords, radius, 200, nearby);

                    if (!nearby.empty()) {
                        Vector3f caustic(0);
                        float area = M_PI * radius * radius;
                        for (const Photon* ph : nearby) {
                            // Cone filter — weights photons closer to centre more
                            float dist = (ph->position - curInter.coords).norm();
                            float w    = std::max(0.f, 1.f - dist / radius);
                            caustic   += ph->power * w;
                        }
                        // Scale by 1/area and surface BRDF
                        caustic = caustic * getColor(curInter) / area * 3.f;
                        caustic.x = std::min(caustic.x, 10.f);
                        caustic.y = std::min(caustic.y, 10.f);
                        caustic.z = std::min(caustic.z, 10.f);
                        Lo += caustic;
                    }
                }

                if (std::abs(curInter.normal.x) > 0.9f) 
                    b1 = normalize(Vector3f(0, 1, 0) - curInter.normal * dotProduct(curInter.normal, Vector3f(0, 1, 0)));
                else 
                    b1 = normalize(Vector3f(1, 0, 0) - curInter.normal * dotProduct(curInter.normal, Vector3f(1, 0, 0))); 
                b2 = crossProduct(curInter.normal, b1);

                float r1 = get_random_float(), r2 = get_random_float();
                float phi = 2 * M_PI * r2, sinTheta = std::sqrt(1 - r1 * r1);
                Vector3f localDir = Vector3f(cos(phi) * sinTheta, sinTheta * sin(phi), r1);

                wi = normalize(b1 * localDir.x + b2 * localDir.y + curInter.normal * localDir.z);
                curInter = intersect(Ray(curInter.coords + curInter.normal * EPSILON, wi));
                if (!curInter.happened) break; 
                curN = curInter.normal; 
            }
        return Lo;

    } 
    
    else if (inter.material->m_type == DIFFUSE ||
           ((inter.material->m_type == GLASS || inter.material->m_type == MIRROR) && TASK_N < 1.3f)) {
        
        // Flip the normal vector(N) when a camera ray hits the backface(inside of an object) 
        // To ensure lighting calculation preserving the original N for reflections.
        Vector3f N_diffuse = N;
        if (dotProduct(dir, N_diffuse) > 0.0f) {
            N_diffuse = -N_diffuse;
        }

        Vector3f diffuseColor = 0, specularColor = 0;
        int light_sample = 4;  // the number of samples on the area light

        for (int i = 0; i < light_sample; ++i) {
        // TODO: task 1.1 Basic ray tracing (Whitted style) with area light sampling
        // sample area light, basic shading, compute Phong illumination model

            // lightInter: store the exact intersection point on the light source
            // pdf_light: probability density function (PDF) value of that sampled point.
            Intersection lightInter; 
            float pdf_light = 0.0f;

            // Sample a random point on the area light to simulate soft shadows
            // Store the intersection point & PDF value of the random point
            sampleLight(lightInter, pdf_light);

            // Checks if the sampled point is mathematically valid
            if (pdf_light > 1e-6f) {
                // L: Direction towards the light
                // V: View direction
                Vector3f L = normalize(lightInter.coords - hitPoint); // L: Direction towards the light
                Vector3f V = normalize(-dir); // V: View direction

                // Skip calculation if the light is behind the surface
                if (dotProduct(N_diffuse, L) <= 0.0f) {
                    continue;
                }

                // Apply surface offset to prevent shadow acne
                Vector3f shadowOrig = hitPoint + N_diffuse * EPSILON;
                
                Intersection shadowInter = intersect(Ray(shadowOrig, L));

                // Distance from shadow to light source
                float distToLight = (lightInter.coords - shadowOrig).norm();
                // Distance to the object hit by the shadow ray
                float distToHit = (shadowInter.coords - shadowOrig).norm();

                // Determines visibility (occlusion)
                // When shadow ray intersection happened,
                // 1) If the object hit by the shadow ray is further than the light (minus EPSILON for avoiding the self shadowing)
                // 2) or if the hit object is the light source itself (EMIT),
                // the point is considered fully illuminated.
                if (shadowInter.happened && (distToHit > distToLight - EPSILON || shadowInter.material->m_type == EMIT)) {
                    Vector3f L_i = Vector3f(1.0f, 1.0f, 1.0f); // Light intensity(light color is considered as pure white)
                    Vector3f Kd = inter.obj->evalDiffuseColor(st); // Material diffuse color

                    // Compute Diffuse (Lambertian)
                    float diff = dotProduct(N_diffuse, L);
                    diffuseColor += L_i * Kd * diff;

                    // Compute Specular (Blinn-Phong)
                    Vector3f H = normalize(L + V); // Half ector
                    float shininess = 64.0f; // Shiness coefficient

                    float spec = std::pow(std::max(0.0f, dotProduct(N_diffuse, H)), shininess);
                    Vector3f Ks = Vector3f(0.2f); // Specular coefficient
                    specularColor += L_i * Ks * spec;
                }
            }
        }
        // Average the accumulated colors over the sample count
        return (diffuseColor + specularColor) / (float)light_sample;
    
    } else if (inter.material->m_type == GLASS && TASK_N >= 1.3f) {  // if the object is glass
        // TODO: task 1.3 Glass Material
        // if the depth exceeds the maximum depth, return the hitColor to avoid infinite recursion

        // 1. Calculate Fresnel coefficient
        // kr: Ratio of reflected light
        // ior: refractive index of the medium (initially 2)
        float kr = fresnel(dir, N, inter.material->ior);
        Vector3f reflectionColor = 0, refractionColor = 0;

        // 2. Calculates reflection vector 
        // where the indicident light (dir) bounces off 
        // based on the normal (N)
        Vector3f reflectionDir = normalize(reflect(dir, N));

        // To prevent the "self-intersection" error where the ray hits its own surface immediately upon starting, 
        // a new origin is set by slightly offsetting it from the surface by EPSILON.
        Vector3f reflectionRayOrig = (dotProduct(reflectionDir, N) < 0) ? hitPoint - N * EPSILON : hitPoint + N * EPSILON;

        // Recursive call: ray is cast in the reflection direction to get the color of the reflected scene.
        // depth is increased by 1 to prevent an infinite loop.
        reflectionColor = castRay(Ray(reflectionRayOrig, reflectionDir), depth + 1, sw);

        // 3. Calculate refraction component
        // kr==1 means 100% of the light is reflected.
        // Refraction is only calculated when kr < 1.
        if (kr < 1.0f) {
            // Calculates the direction of the light refracting when it passes through the glass
            // applying the glass's refractive index(ior).
            Vector3f refractionDir = normalize(refract(dir, N, inter.material->ior));
            // Similar to reflectionRayOrig
            // Origin with offset is set for preventing "self-intersection" error
            Vector3f refractionRayOrig = (dotProduct(refractionDir, N) < 0) ? hitPoint - N * EPSILON : hitPoint + N * EPSILON;
            // ray is cast in the refraction direction to get the color of the scene visible through the glass
            refractionColor = castRay(Ray(refractionRayOrig, refractionDir), depth + 1, sw);
        }

        // Return weighted sum of reflected and refracted light using Fresnel coefficient(reflection: kr, refraction: 1-kr)
        return reflectionColor * kr + refractionColor * (1.0f - kr);
    } 
    
    else if (inter.material->m_type == MIRROR && TASK_N >= 1.3f) {  // if the object is mirror
        // TODO: task 1.3 Mirror Refection

        // Mirror reflects perfectly without losing energy.
        // 1. Calculate the Reflection Direction
        Vector3f reflectionDir = normalize(reflect(dir, N));
        // 2. Apply the Origin Offset(EPSILON)
        Vector3f reflectionRayOrig = (dotProduct(reflectionDir, N) < 0) ? hitPoint - N * EPSILON : hitPoint + N * EPSILON;
        // Call castRay again in the reflected direction to get the color.
        return castRay(Ray(reflectionRayOrig, reflectionDir), depth + 1, sw);
    }

    return hitColor;
}


void Scene::buildPhotonMap(int numPhotons) {
    int stored = 0;
    int attempts = 0;
    Vector3f diamondCenter(350, 0, 250); // match your main.cpp

    while (stored < numPhotons && attempts < numPhotons * 20) {
        attempts++;

        // Sample point on light
        Intersection lightPos; float lightPdf;
        sampleLight(lightPos, lightPdf);
        if (!lightPos.happened) continue;

        // Random wavelength
        float lambda = 400.f + get_random_float() * 300.f;

        // AIM toward diamond with a cone spread instead of random hemisphere
        // This is the key fix — random hemisphere misses the diamond almost always
        Vector3f towardDiamond = normalize(diamondCenter - lightPos.coords);
        float spread = 0.15f; // cone half-angle in radians roughly
        Vector3f right, up;
        if (std::fabs(towardDiamond.x) > std::fabs(towardDiamond.y))
            right = normalize(crossProduct(towardDiamond, Vector3f(0,1,0)));
        else
            right = normalize(crossProduct(towardDiamond, Vector3f(1,0,0)));
        up = crossProduct(right, towardDiamond);

        float r1 = get_random_float(), r2 = get_random_float();
        float angle = spread * std::sqrt(r1);
        float phi   = 2.f * M_PI * r2;
        Vector3f dir = normalize(
            towardDiamond
            + right * (std::cos(phi) * angle)
            + up    * (std::sin(phi) * angle)
        );

        // Photon power — scale up to compensate for targeted sampling
        float lightArea = 0;
        for (auto obj : objects)
            if (obj->hasEmit()) lightArea += obj->getArea();
        Vector3f power = lightPos.material->m_emission * lightArea / (float)numPhotons;

        Ray photonRay(lightPos.coords + dir * EPSILON, dir);
        bool hitGlass = false;

        for (int bounce = 0; bounce < 10; bounce++) {
            Intersection hit = intersect(photonRay);
            if (!hit.happened) break;
            if (hit.material->m_type == EMIT)   break;
            if (hit.material->m_type == MIRROR) break; // skip mirror entirely

            if (hit.material->m_type == GLASS) {
                float ior = hit.material->iorAtLambda(lambda);
                float kr  = fresnel(photonRay.direction, hit.normal, ior);
                Vector3f faceN = dotProduct(photonRay.direction, hit.normal) < 0
                               ? hit.normal : -hit.normal;
                Vector3f newDir;
                if (get_random_float() < kr)
                    newDir = normalize(reflect(photonRay.direction, faceN));
                else {
                    newDir   = normalize(refract(photonRay.direction, faceN, ior));
                    hitGlass = true;
                }
                photonRay = Ray(hit.coords + newDir * EPSILON, newDir);
                continue;
            }

            // Diffuse hit
            if (hitGlass) {
                float r = expf(-0.5f * powf((lambda - 650.f) / 50.f, 2.f));
                float g = expf(-0.5f * powf((lambda - 550.f) / 50.f, 2.f));
                float b = expf(-0.5f * powf((lambda - 450.f) / 50.f, 2.f));
                float s = r + g + b;
                if (s > 1e-4f) { r /= s; g /= s; b /= s; }

                Photon p;
                p.position  = hit.coords;
                p.direction = photonRay.direction;
                p.lambda    = lambda;
                p.power     = power * Vector3f(r, g, b);
                causticMap.store(p);
                stored++;
                break;
            }

            // Non-caustic diffuse — bounce
            if (get_random_float() > RussianRoulette) break;
            power = power * hit.material->m_color / RussianRoulette;
            float r1b = get_random_float(), r2b = get_random_float();
            float phib = 2.f * M_PI * r2b;
            float sT = std::sqrt(r1b);
            Vector3f ld(sT*std::cos(phib), sT*std::sin(phib),
                        std::sqrt(std::max(0.f, 1.f - r1b)));
            Vector3f Nb = hit.normal, Bb, Cb;
            if (std::fabs(Nb.x) > std::fabs(Nb.y))
                Cb = normalize(Vector3f(Nb.z, 0.f, -Nb.x));
            else
                Cb = normalize(Vector3f(0.f, Nb.z, -Nb.y));
            Bb = crossProduct(Cb, Nb);
            Vector3f nd = normalize(Bb*ld.x + Cb*ld.y + Nb*ld.z);
            photonRay = Ray(hit.coords + nd * EPSILON, nd);
        }
    }

    printf("Photon map: %d stored, %d attempts\n", stored, attempts);
    causticMap.build();
}