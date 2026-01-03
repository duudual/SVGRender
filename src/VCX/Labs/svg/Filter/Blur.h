#pragma once

#include "Labs/Common/ImageRGB.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace VCX::Labs::SVG {

//=============================================================================
// Filter Base Class
//=============================================================================
class Filter {
public:
    virtual ~Filter() = default;
    virtual void Apply(Common::ImageRGB& image) = 0;
};

//=============================================================================
// Gaussian Blur Filter - Separable implementation
//=============================================================================
class GaussianBlur : public Filter {
public:
    GaussianBlur(float sigma = 1.0f) : _sigma(sigma) {
        GenerateKernel();
    }

    void SetSigma(float sigma) {
        _sigma = sigma;
        GenerateKernel();
    }

    float GetSigma() const { return _sigma; }

    void Apply(Common::ImageRGB& image) override {
        if (_sigma < 0.1f || _kernel.empty()) return;

        auto [width, height] = image.GetSize();
        if (width == 0 || height == 0) return;

        // Create temporary buffer
        std::vector<glm::vec3> buffer(width * height);

        // Horizontal pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sx = std::clamp(static_cast<int>(x) + k, 0, static_cast<int>(width) - 1);
                    sum += image.At(sx, y) * _kernel[k + _radius];
                }
                buffer[y * width + x] = sum;
            }
        }

        // Vertical pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sy = std::clamp(static_cast<int>(y) + k, 0, static_cast<int>(height) - 1);
                    sum += buffer[sy * width + x] * _kernel[k + _radius];
                }
                image.At(x, y) = sum;
            }
        }
    }

    // Apply to a region only
    void ApplyRegion(Common::ImageRGB& image, int x0, int y0, int x1, int y1) {
        if (_sigma < 0.1f || _kernel.empty()) return;

        auto [width, height] = image.GetSize();
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(static_cast<int>(width) - 1, x1);
        y1 = std::min(static_cast<int>(height) - 1, y1);

        if (x0 >= x1 || y0 >= y1) return;

        int regionW = x1 - x0 + 1;
        int regionH = y1 - y0 + 1;

        // Create buffer for region
        std::vector<glm::vec3> buffer(regionW * regionH);

        // Horizontal pass
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sx = std::clamp(x + k, 0, static_cast<int>(width) - 1);
                    sum += image.At(sx, y) * _kernel[k + _radius];
                }
                buffer[(y - y0) * regionW + (x - x0)] = sum;
            }
        }

        // Vertical pass
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sy = std::clamp(y + k, y0, y1);
                    sum += buffer[(sy - y0) * regionW + (x - x0)] * _kernel[k + _radius];
                }
                image.At(x, y) = sum;
            }
        }
    }

private:
    float _sigma;
    int _radius;
    std::vector<float> _kernel;

    void GenerateKernel() {
        // Kernel radius: 3 sigma covers 99.7% of distribution
        _radius = static_cast<int>(std::ceil(_sigma * 3.0f));
        if (_radius < 1) _radius = 1;

        int size = 2 * _radius + 1;
        _kernel.resize(size);

        float sum = 0.0f;
        float inv2sigma2 = 1.0f / (2.0f * _sigma * _sigma);

        for (int i = 0; i < size; ++i) {
            int x = i - _radius;
            float val = std::exp(-x * x * inv2sigma2);
            _kernel[i] = val;
            sum += val;
        }

        // Normalize
        float invSum = 1.0f / sum;
        for (float& k : _kernel) {
            k *= invSum;
        }
    }
};

//=============================================================================
// Box Blur Filter - Fast approximation
//=============================================================================
class BoxBlur : public Filter {
public:
    BoxBlur(int radius = 1) : _radius(radius) {}

    void SetRadius(int radius) { _radius = std::max(1, radius); }
    int GetRadius() const { return _radius; }

    void Apply(Common::ImageRGB& image) override {
        auto [width, height] = image.GetSize();
        if (width == 0 || height == 0) return;

        std::vector<glm::vec3> buffer(width * height);
        int size = 2 * _radius + 1;
        float invSize = 1.0f / static_cast<float>(size);

        // Horizontal pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sx = std::clamp(static_cast<int>(x) + k, 0, static_cast<int>(width) - 1);
                    sum += image.At(sx, y);
                }
                buffer[y * width + x] = sum * invSize;
            }
        }

        // Vertical pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                for (int k = -_radius; k <= _radius; ++k) {
                    int sy = std::clamp(static_cast<int>(y) + k, 0, static_cast<int>(height) - 1);
                    sum += buffer[sy * width + x];
                }
                image.At(x, y) = sum * invSize;
            }
        }
    }

private:
    int _radius;
};

//=============================================================================
// Drop Shadow Filter
//=============================================================================
class DropShadow {
public:
    DropShadow(float offsetX = 2.0f, float offsetY = 2.0f, float blur = 2.0f,
               const glm::vec4& color = glm::vec4(0, 0, 0, 0.5f))
        : _offsetX(offsetX), _offsetY(offsetY), _blur(blur), _color(color) {}

    void SetOffset(float x, float y) { _offsetX = x; _offsetY = y; }
    void SetBlur(float blur) { _blur = blur; }
    void SetColor(const glm::vec4& color) { _color = color; }

    // Apply drop shadow to a mask (alpha channel)
    void ApplyToMask(const std::vector<float>& mask, int width, int height,
                     Common::ImageRGB& targetImage) {
        if (mask.empty() || width <= 0 || height <= 0) return;

        auto [imgW, imgH] = targetImage.GetSize();

        // Create shadow mask with offset
        std::vector<float> shadowMask(width * height, 0.0f);
        
        int dx = static_cast<int>(_offsetX);
        int dy = static_cast<int>(_offsetY);

        for (int y = 0; y < height; ++y) {
            int sy = y - dy;
            if (sy < 0 || sy >= height) continue;
            
            for (int x = 0; x < width; ++x) {
                int sx = x - dx;
                if (sx < 0 || sx >= width) continue;
                
                shadowMask[y * width + x] = mask[sy * width + sx];
            }
        }

        // Blur the shadow mask
        if (_blur > 0.1f) {
            BlurMask(shadowMask, width, height, _blur);
        }

        // Composite shadow onto image
        for (int y = 0; y < height && y < static_cast<int>(imgH); ++y) {
            for (int x = 0; x < width && x < static_cast<int>(imgW); ++x) {
                float alpha = shadowMask[y * width + x] * _color.a;
                if (alpha > 0.001f) {
                    glm::vec3& pixel = targetImage.At(x, y);
                    float invAlpha = 1.0f - alpha;
                    pixel = pixel * invAlpha + glm::vec3(_color.r, _color.g, _color.b) * alpha;
                }
            }
        }
    }

private:
    float _offsetX, _offsetY;
    float _blur;
    glm::vec4 _color;

    void BlurMask(std::vector<float>& mask, int width, int height, float sigma) {
        int radius = static_cast<int>(std::ceil(sigma * 3.0f));
        if (radius < 1) return;

        // Generate kernel
        int size = 2 * radius + 1;
        std::vector<float> kernel(size);
        float sum = 0.0f;
        float inv2sigma2 = 1.0f / (2.0f * sigma * sigma);

        for (int i = 0; i < size; ++i) {
            int x = i - radius;
            float val = std::exp(-x * x * inv2sigma2);
            kernel[i] = val;
            sum += val;
        }
        for (float& k : kernel) k /= sum;

        std::vector<float> buffer(width * height);

        // Horizontal
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float s = 0.0f;
                for (int k = -radius; k <= radius; ++k) {
                    int sx = std::clamp(x + k, 0, width - 1);
                    s += mask[y * width + sx] * kernel[k + radius];
                }
                buffer[y * width + x] = s;
            }
        }

        // Vertical
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float s = 0.0f;
                for (int k = -radius; k <= radius; ++k) {
                    int sy = std::clamp(y + k, 0, height - 1);
                    s += buffer[sy * width + x] * kernel[k + radius];
                }
                mask[y * width + x] = s;
            }
        }
    }
};

} // namespace VCX::Labs::SVG
