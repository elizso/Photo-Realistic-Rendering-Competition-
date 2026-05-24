//
// Created by LEI XU on 5/16/19.
//

#ifndef RAYTRACING_MATERIAL_H
#define RAYTRACING_MATERIAL_H

#include "Texture.hpp"
#include "Vector.hpp"
#include "SpectralSample.hpp"

enum MaterialType { DIFFUSE, GLASS, MIRROR, EMIT, GLINT};


inline float fract(float x) { return x - std::floor(x); }

inline float GGX(const Vector3f& H, const Vector3f& N, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = std::max(0.f, dotProduct(N, H));
    float NdotH2 = NdotH * NdotH;
    float denom  = NdotH2 * (a2 - 1.f) + 1.f;
    return a2 / (M_PI * denom * denom);
}

inline float smithG1(const Vector3f& v, const Vector3f& N, float roughness) {
    float a    = roughness * roughness;
    float NdotV = std::max(0.f, dotProduct(N, v));
    float denom = NdotV + std::sqrt(a*a + (1.f - a*a) * NdotV * NdotV);
    return 2.f * NdotV / denom;
}

inline float smithG(const Vector3f& wo, const Vector3f& wi,
                    const Vector3f& N,  float roughness) {
    return smithG1(wo, N, roughness) * smithG1(wi, N, roughness);
}

inline float fresnelSchlick(float cosTheta, float F0) {
    return F0 + (1.f - F0) * std::pow(1.f - cosTheta, 5.f);
}

class Material{
private:



// Compute reflection direction
    Vector3f reflect(const Vector3f &I, const Vector3f &N) const
    {
        return I - 2 * dotProduct(I, N) * N;
    }

    // Compute refraction direction using Snell's law
    //
    // We need to handle with care the two possible situations:
    //
    //    - When the ray is inside the object
    //
    //    - When the ray is outside.
    //
    // If the ray is outside, you need to make cosi positive cosi = -N.I
    //
    // If the ray is inside, you need to invert the refractive indices and negate the normal N
    Vector3f refract(const Vector3f &I, const Vector3f &N, const float &ior) const
    {
        float cosi = clamp(-1, 1, dotProduct(I, N));
        float etai = 1, etat = ior;
        Vector3f n = N;
        if (cosi < 0) { cosi = -cosi; } else { std::swap(etai, etat); n= -N; }
        float eta = etai / etat;
        float k = 1 - eta * eta * (1 - cosi * cosi);
        return k < 0 ? 0 : eta * I + (eta * cosi - sqrtf(k)) * n;
    }

    // Compute Fresnel equation
    //
    // \param I is the incident view direction
    //
    // \param N is the normal at the intersection point
    //
    // \param ior is the material refractive index
    //
    // \param[out] kr is the amount of light reflected
    void fresnel(const Vector3f &I, const Vector3f &N, const float &ior, float &kr) const
    {
        float cosi = clamp(-1, 1, dotProduct(I, N));
        float etai = 1, etat = ior;
        if (cosi > 0) {  std::swap(etai, etat); }
        // Compute sini using Snell's law
        float sint = etai / etat * sqrtf(std::max(0.f, 1 - cosi * cosi));
        // Total internal reflection
        if (sint >= 1) {
            kr = 1;
        }
        else {
            float cost = sqrtf(std::max(0.f, 1 - sint * sint));
            cosi = fabsf(cosi);
            float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
            float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
            kr = (Rs * Rs + Rp * Rp) / 2;
        }
        // As a consequence of the conservation of energy, transmittance is given by:
        // kt = 1 - kr;
    }

    Vector3f toWorld(const Vector3f &a, const Vector3f &N){
        Vector3f B, C;
        if (std::fabs(N.x) > std::fabs(N.y)){
            float invLen = 1.0f / std::sqrt(N.x * N.x + N.z * N.z);
            C = Vector3f(N.z * invLen, 0.0f, -N.x *invLen);
        }
        else {
            float invLen = 1.0f / std::sqrt(N.y * N.y + N.z * N.z);
            C = Vector3f(0.0f, N.z * invLen, -N.y *invLen);
        }
        B = crossProduct(C, N);
        return a.x * B + a.y * C + a.z * N;
    }

public:
    MaterialType m_type;
    Vector3f m_color;
    Vector3f m_emission;
    float ior;  // index of refraction
    float Kd, Ks;  // coefficient of diffuse and specular
    float specularExponent;
    bool textured;
    // default is glass 
    float cauchyA = 1.458f;
    float cauchyB = 0.00354f;
    float roughness;
    
    Texture* diffuse_map = nullptr;
    Texture* normal_map = nullptr;

    inline Material(MaterialType t=DIFFUSE, Vector3f e=Vector3f(0,0,0));
    inline MaterialType getType();
    inline Vector3f getColor();
    inline Vector3f getColorAt(double u, double v);
    inline Vector3f getEmission();
    inline bool hasEmission();

    // sample a ray by Material properties
    inline Vector3f sample(const Vector3f &wi, const Vector3f &N);
    // given a ray, calculate the PdF of this ray
    inline float pdf(const Vector3f &wi, const Vector3f &wo, const Vector3f &N);
    // given a ray direction and normal, calculate the contribution of this ray
    inline Vector3f eval(const Vector3f &dir, const Vector3f &N);
    float iorAtLambda(float lambda_nm) const {
        return cauchyA + cauchyB / (lambda_nm * lambda_nm * 1e-6f);
    }

    

    // In Material.hpp — add glint evaluation
    Vector3f evalGlint(const Vector3f& wo,
                   const Vector3f& wi,
                   const Vector3f& N,
                   const Vector2f& uv,
                   const Vector3f& worldPos,
                   float rough) const {

        float patchSize = 2.f;
        float seed = std::floor(worldPos.x / patchSize) * 73856093.f
                + std::floor(worldPos.z / patchSize) * 19349663.f;
        float r1 = fract(std::sin(seed)        * 43758.5453f);
        float r2 = fract(std::cos(seed * 1.7f) * 12345.6789f);

        if (r1 > 0.3f) return Vector3f(0.f);

        float lambda   = 400.f + r2 * 300.f;
        Vector3f color = wavelengthToRGBGaussian(lambda) * 10.f;

        Vector3f H      = normalize(wo + wi);
        float NdotH     = std::max(0.f, dotProduct(N, H));
        float highlight = std::pow(NdotH, 20.f);  // very broad — fires most of floor

        if (highlight < 0.05f) return Vector3f(0.f);

        return color * highlight;
    }

};

Material::Material(MaterialType t, Vector3f color){
    m_type = t;
    m_color = color;
    Kd=0.8;
    Ks=0.2;
    specularExponent=25;
    m_emission=0;
    textured=false;
    ior=1.52;
}

MaterialType Material::getType(){return m_type;}
Vector3f Material::getColor(){return m_color;}
Vector3f Material::getEmission() {return m_emission;}
bool Material::hasEmission() {
    if (m_emission.norm() > EPSILON) return true;
    else return false;
}

Vector3f Material::getColorAt(double u, double v) {
    return Vector3f();
}


Vector3f Material::sample(const Vector3f &wi, const Vector3f &N){
    switch(m_type){
        default:
        {
            // cosine-weighted hemisphere sample (Malley's method): map uniform
            // disk samples onto the hemisphere via z = sqrt(1 - r^2). This
            // concentrates samples near the pole where cos(theta) is large.
            float x_1 = get_random_float(), x_2 = get_random_float();
            float r = std::sqrt(x_1);
            float phi = 2.0f * M_PI * x_2;
            float x = r * std::cos(phi);
            float y = r * std::sin(phi);
            float z = std::sqrt(std::max(0.0f, 1.0f - x_1));
            Vector3f localRay(x, y, z);
            return toWorld(localRay, N);
        }
    }
}

float Material::pdf(const Vector3f &wi, const Vector3f &wo, const Vector3f &N){
    switch(m_type){
        default:
        {
            // cosine-weighted hemisphere pdf = cos(theta) / pi
            float cosTheta = dotProduct(wo, N);
            if (cosTheta > 0.0f)
                return cosTheta / M_PI;
            else
                return 0.0f;
        }
    }
}

Vector3f Material::eval(const Vector3f &dir, const Vector3f &N){
    switch(m_type){
        default:
        {
            // calculate the contribution of diffuse   model
            float cosalpha = dotProduct(N, dir);
            if (cosalpha > 0.0f) {
                Vector3f diffuse = Kd / M_PI;
                return diffuse;
            }
            else
                return Vector3f(0.0f);
            break;
        }
    }
}





#endif //RAYTRACING_MATERIAL_H
