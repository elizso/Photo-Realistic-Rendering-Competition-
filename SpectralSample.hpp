#pragma once 
#include "Vector.hpp" 
#include "global.hpp" 

static constexpr int N_LAMBDA = 4;
static constexpr float LAMBDA_MIN = 400.0f;
static constexpr float LAMBDA_MAX = 700.0f;

struct SampledWavelengths{

    float lambda[N_LAMBDA]; // wavelength in nm per sample 
    float pdf[N_LAMBDA];
    bool terminated[N_LAMBDA];

    // Stratified sampling with random phase
    static SampledWavelengths sample(float u){
        SampledWavelengths result;
        for (int i = 0; i < N_LAMBDA; ++i) {
            float offset = u + float(i) / float(N_LAMBDA);
            if (offset >= 1.f) offset -= 1.f;
            result.lambda[i] = LAMBDA_MIN + offset * (LAMBDA_MAX - LAMBDA_MIN);
            result.pdf[i] = 1.f / (LAMBDA_MAX - LAMBDA_MIN);
            result.terminated[i] = false;
        }
        return result;
    }

    // only hero continues after when dispersive refraction occurs. 
    void TerminateSecondary(){
        for (int i = 1; i < N_LAMBDA; i++) terminated[i] = true;
    }

    bool SecondaryTerminated() const {return terminated[1];}

};

// Diamond:  A=2.3849, B=0.01942
// Glass:    A=1.4580, B=0.00354
// Water:    A=1.3320, B=0.00310
inline float cauchyIOR(float lambda_nm, float A, float B){
    float l = lambda_nm * 1e-3f;
    return A + B / (l * l);
}

// Wyman et al. 2013 analytic Gaussian fit
inline Vector3f wavelengthToXYZ(float l){
    float x  =  0.362f * expf(-0.5f * powf((l - 442.0f) / 16.0f, 2.f))
              + 1.056f * expf(-0.5f * powf((l - 599.8f) / 37.9f, 2.f))
              - 0.065f * expf(-0.5f * powf((l - 501.1f) / 20.4f, 2.f));
    // ȳ
    float y  =  0.821f * expf(-0.5f * powf((l - 568.8f) / 46.9f, 2.f))
              + 0.286f * expf(-0.5f * powf((l - 530.9f) / 16.3f, 2.f));
    // z̄
    float z  =  1.217f * expf(-0.5f * powf((l - 437.0f) / 11.8f, 2.f))
              + 0.681f * expf(-0.5f * powf((l - 459.0f) / 26.0f, 2.f));
    return Vector3f(x, y, z);
}

// sRGB from XYZ (D65 whitepoint)
inline Vector3f XYZToRGB(const Vector3f &xyz){
    float r =  3.2406f * xyz.x - 1.5372f * xyz.y - 0.4986f * xyz.z;
    float g = -0.9689f * xyz.x + 1.8758f * xyz.y + 0.0415f * xyz.z;
    float b =  0.0557f * xyz.x - 0.2040f * xyz.y + 1.0570f * xyz.z;
    return Vector3f(r, g, b);
}

// Call once per wavelength per path, divide by N_LAMBDA after all paths 
inline Vector3f spectralToXYZ(float lambda, float radiance, float pdf){
    Vector3f xyz = wavelengthToXYZ(lambda);
    // Monte Carlo estimator: f(lambda) * w(lambda) / p(lambda) 

    return xyz * (radiance / (pdf * 106.856895f)); 
    // 106.856895 is the normalization factor for the XYZ color matching functions

}

inline Vector3f wavelengthToRGB(float lambda_nm){
    float hue = (700.f - lambda_nm) / 300.f * 240.f;
    float h = hue / 60.f;
    int   i = (int)h % 6;
    float f = h - (float)(int)h;
    Vector3f rgb;
    switch (i) {
        case 0: rgb = Vector3f(1.f, f,   0.f);  break;
        case 1: rgb = Vector3f(1.f-f, 1.f, 0.f);break;
        case 2: rgb = Vector3f(0.f, 1.f, f);    break;
        case 3: rgb = Vector3f(0.f, 1.f-f, 1.f);break;
        case 4: rgb = Vector3f(f,   0.f, 1.f);  break;
        default: rgb = Vector3f(1.f, 0.f, 1.f-f);break;
    }
    rgb.x /= 0.375f;
    rgb.y /= 0.750f;
    rgb.z /= 0.375f;
    // Clamp to prevent any channel exceeding 3.0
    rgb.x = std::min(rgb.x, 3.0f);
    rgb.y = std::min(rgb.y, 3.0f);
    rgb.z = std::min(rgb.z, 3.0f);
    return rgb;
}

inline Vector3f wavelengthToRGBGaussian(float lambda_nm) {
    float r = expf(-0.5f * powf((lambda_nm - 650.f) / 50.f, 2.f));
    float g = expf(-0.5f * powf((lambda_nm - 550.f) / 50.f, 2.f));
    float b = expf(-0.5f * powf((lambda_nm - 450.f) / 50.f, 2.f));
    // Always non-negative — no clamping needed
    float sum = r + g + b; 
    if (sum > 1e-4f) {
        r /= sum;
        g /= sum;
        b /= sum;
    }

    r *= 2.5f; g *= 2.5f; b *= 2.5f;
    return Vector3f(r, g, b);
}