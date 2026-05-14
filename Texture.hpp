
#ifndef RASTERIZER_TEXTURE_H
#define RASTERIZER_TEXTURE_H
#include "global.hpp"
#include "Vector.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>

class Texture {
private:
    cv::Mat image_data;

public:
    Texture(const std::string& name)
    {
        image_data = cv::imread(name);
        if (image_data.empty()) {
            std::cerr << "[OpenCV Error] Can't find the image: " << name << std::endl;
            exit(1);
        }
        cv::cvtColor(image_data, image_data, cv::COLOR_RGB2BGR);
        width = image_data.cols;
        height = image_data.rows;
    }

    int width, height;

    Vector3f getColor(float u, float v)
    {
        // 1.clam the uv coordinates
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        // 2. Use (width - 1), (height - 1) to prevent exceeding index
        auto u_img = u * (width - 1);
        auto v_img = (1 - v) * (height - 1);

        auto color = image_data.at<cv::Vec3b>(v_img, u_img);
        
        // 3. Ray tracer calculate color within 0-1, so divide to 255.
        return Vector3f(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f);
    }
};
#endif //RASTERIZER_TEXTURE_H