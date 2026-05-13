//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"


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
Vector3f Scene::castRay(const Ray &ray, int depth) const
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

    if (inter.material->m_type == EMIT) {  // if the object is a light source
        return inter.material->m_emission;  // return light color

    } else if (inter.material->m_type == DIFFUSE ||
           ((inter.material->m_type == GLASS || inter.material->m_type == MIRROR) && TASK_N < 1.3f)) {
        
        // Flip the normal vector(N) when a camera ray hits the backface(inside of an object) 
        // To ensure lighting calculation preserving the original N for reflections.
        Vector3f N_diffuse = N;
        if (dotProduct(dir, N_diffuse) > 0.0f) {
            N_diffuse = -N_diffuse;
        }

        if (TASK_N == 2) {
            // TODO: task 2 Monte Carlo Path Tracing with Russian Roulette termination

            // 1. Calculate Direct Illumination(light arriving directly from the light source)
            Vector3f L_dir(0.0f);
            Intersection lightInter;
            float lightPdf = 0.0f;
            
            // Sample a random point on the area light
            sampleLight(lightInter, lightPdf);
            // Checks if the sampled point is mathematically valid
            if (lightPdf > 1e-6f) {
                Vector3f L = lightInter.coords - hitPoint;
                float distToLight = L.norm();
                Vector3f lightDir = normalize(L);
                
                // Cosine terms for the surface normal and light normal
                float cos_theta = dotProduct(N_diffuse, lightDir);
                float cos_theta_prime = dotProduct(lightInter.normal, -lightDir);

                // Calculate only if the surface and the light are facing each other
                if (cos_theta > 0.0f && cos_theta_prime > 0.0f) {
                    // Apply an offset along the normal to prevent shadow acne (self-intersection)
                    Vector3f shadowOrig = hitPoint + N_diffuse * EPSILON;
                    Intersection shadowInter = intersect(Ray(shadowOrig, lightDir));
                    float distToHit = (shadowInter.coords - shadowOrig).norm();

                    // Add direct light if not occluded by another object
                    if (shadowInter.happened && (distToHit > distToLight - 0.05f || shadowInter.material->m_type == EMIT)) {
                        Vector3f f_r = inter.obj->evalDiffuseColor(st) / M_PI; // BRDF (Diffuse)
                        Vector3f L_i = lightInter.material->m_emission;

                        // Prevent the r^2(in 1/r^2) close to 0
                        // when the ceiling too close to light
                        float dist2 = std::max(distToLight * distToLight, 1.0f);
                        
                        // Apply rendering equation (includes geometric term for area light integration)
                        L_dir = L_i * f_r * cos_theta * cos_theta_prime / (distToLight * distToLight * lightPdf);
                    }
                }
            }

            // 2. Calculate Indirect Illumination
            Vector3f L_indir(0.0f);
            float P_RR = RussianRoulette; // Russian Roulette survival probability
            
            // Decide whether to bounce using Russian Roulette
            if (get_random_float() < P_RR) {
                // Sample a random direction on the hemisphere
                Vector3f wi = inter.material->sample(-dir, N_diffuse);
                float pdf = inter.material->pdf(-dir, wi, N_diffuse);
                
                // Checks if the sampled point is mathematically valid
                if (pdf > 1e-6f) {
                    Vector3f bounceOrig = hitPoint + N_diffuse * EPSILON;
                    Ray bounceRay(bounceOrig, wi);
                    Intersection bounceInter = intersect(bounceRay);

                    // Skip if the bounced ray hits the light source directly to avoid double counting
                    if (bounceInter.happened && bounceInter.material->m_type != EMIT) {
                        Vector3f f_r = inter.obj->evalDiffuseColor(st) / M_PI; // BRDF (Diffuse)
                        float cos_theta_indir = std::max(0.0f, dotProduct(N_diffuse, wi));

                        // Recursively call castRay to gather incoming light from the bounced direction
                        Vector3f bounceColor = castRay(bounceRay, depth + 1);
                        
                        // To prevent the fireflies effect.
                        // Clamp the spot with too much exposure of the light.
                        bounceColor.x = clamp(0.0f, 1.0f, bounceColor.x);
                        bounceColor.y = clamp(0.0f, 1.0f, bounceColor.y);
                        bounceColor.z = clamp(0.0f, 1.0f, bounceColor.z);

                        // Calculate the indirect light contribution
                        L_indir = bounceColor * f_r * cos_theta_indir / (pdf * P_RR);
                    }
                }
            }

            // Return total illumination
            return L_dir + L_indir;

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
        reflectionColor = castRay(Ray(reflectionRayOrig, reflectionDir), depth + 1);

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
            refractionColor = castRay(Ray(refractionRayOrig, refractionDir), depth + 1);
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
        return castRay(Ray(reflectionRayOrig, reflectionDir), depth + 1);
    }

    return hitColor;
}
