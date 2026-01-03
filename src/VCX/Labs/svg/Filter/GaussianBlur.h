#pragma once

#include "Labs/Common/ImageRGB.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace VCX::Labs::SVG {

//=============================================================================
// Gaussian Blur Filter - Separable Implementation
// This is a high-quality implementation for SVG feGaussianBlur filter
//=============================================================================
class GaussianBlur {
public:
    //=========================================================================
    // Apply Gaussian blur with given sigma (standard deviation)
    // Uses separable 2-pass algorithm for O(n) per-pixel complexity
    //=========================================================================
    static void Blur(Common::ImageRGB& image, float sigma) {
        if (sigma <= 0.0f) return;

        auto [width, height] = image.GetSize();
        if (width == 0 || height == 0) return;

        // Generate 1D Gaussian kernel
        std::vector<float> kernel = GenerateKernel(sigma);
        int radius = static_cast<int>(kernel.size() / 2);

        // Temporary buffer for horizontal pass
        std::vector<glm::vec3> temp(width * height);

        // Horizontal pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                float weightSum = 0.0f;

                for (int k = -radius; k <= radius; ++k) {
                    int sx = static_cast<int>(x) + k;
                    // Clamp to edge (extend edge pixels)
                    sx = std::clamp(sx, 0, static_cast<int>(width) - 1);
                    
                    float weight = kernel[k + radius];
                    sum += image.At(sx, y) * weight;
                    weightSum += weight;
                }

                temp[y * width + x] = sum / weightSum;
            }
        }

        // Vertical pass
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 sum(0.0f);
                float weightSum = 0.0f;

                for (int k = -radius; k <= radius; ++k) {
                    int sy = static_cast<int>(y) + k;
                    // Clamp to edge
                    sy = std::clamp(sy, 0, static_cast<int>(height) - 1);
                    
                    float weight = kernel[k + radius];
                    sum += temp[sy * width + x] * weight;
                    weightSum += weight;
                }

                image.At(x, y) = sum / weightSum;
            }
        }
    }

    //=========================================================================
    // Apply blur with different sigma for X and Y axes
    //=========================================================================
    static void BlurXY(Common::ImageRGB& image, float sigmaX, float sigmaY) {
        if (sigmaX <= 0.0f && sigmaY <= 0.0f) return;

        auto [width, height] = image.GetSize();
        if (width == 0 || height == 0) return;

        // Horizontal pass with sigmaX
        if (sigmaX > 0.0f) {
            std::vector<float> kernelX = GenerateKernel(sigmaX);
            int radiusX = static_cast<int>(kernelX.size() / 2);

            std::vector<glm::vec3> temp(width * height);

            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    glm::vec3 sum(0.0f);
                    float weightSum = 0.0f;

                    for (int k = -radiusX; k <= radiusX; ++k) {
                        int sx = static_cast<int>(x) + k;
                        sx = std::clamp(sx, 0, static_cast<int>(width) - 1);
                        
                        float weight = kernelX[k + radiusX];
                        sum += image.At(sx, y) * weight;
                        weightSum += weight;
                    }

                    temp[y * width + x] = sum / weightSum;
                }
            }

            // Copy back
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    image.At(x, y) = temp[y * width + x];
                }
            }
        }

        // Vertical pass with sigmaY
        if (sigmaY > 0.0f) {
            std::vector<float> kernelY = GenerateKernel(sigmaY);
            int radiusY = static_cast<int>(kernelY.size() / 2);

            std::vector<glm::vec3> temp(width * height);

            // First copy current state
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    temp[y * width + x] = image.At(x, y);
                }
            }

            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    glm::vec3 sum(0.0f);
                    float weightSum = 0.0f;

                    for (int k = -radiusY; k <= radiusY; ++k) {
                        int sy = static_cast<int>(y) + k;
                        sy = std::clamp(sy, 0, static_cast<int>(height) - 1);
                        
                        float weight = kernelY[k + radiusY];
                        sum += temp[sy * width + x] * weight;
                        weightSum += weight;
                    }

                    image.At(x, y) = sum / weightSum;
                }
            }
        }
    }

    //=========================================================================
    // Box blur approximation (3-pass for Gaussian approximation)
    // Much faster for large sigma values
    //=========================================================================
    static void BlurFast(Common::ImageRGB& image, float sigma) {
        if (sigma <= 0.0f) return;

        // Use 3-pass box blur as Gaussian approximation
        // Box sizes based on sigma for Gaussian approximation
        int boxSize = static_cast<int>(std::sqrt(12.0f * sigma * sigma / 3.0f + 1.0f));
        if (boxSize % 2 == 0) boxSize++;  // Make odd

        // Apply 3 passes for good Gaussian approximation
        BoxBlur(image, boxSize);
        BoxBlur(image, boxSize);
        BoxBlur(image, boxSize);
    }

private:
    //=========================================================================
    // Generate 1D Gaussian kernel
    //=========================================================================
    static std::vector<float> GenerateKernel(float sigma) {
        // Kernel radius is typically 3*sigma (99.7% of values)
        int radius = static_cast<int>(std::ceil(sigma * 3.0f));
        radius = std::max(1, std::min(radius, 100));  // Limit kernel size
        
        int size = 2 * radius + 1;
        std::vector<float> kernel(size);
        
        float sigma2 = sigma * sigma;
        float sum = 0.0f;
        
        for (int i = 0; i < size; ++i) {
            int x = i - radius;
            float g = std::exp(-static_cast<float>(x * x) / (2.0f * sigma2));
            kernel[i] = g;
            sum += g;
        }
        
        // Normalize
        for (float& k : kernel) {
            k /= sum;
        }
        
        return kernel;
    }

    //=========================================================================
    // Box blur (single pass)
    //=========================================================================
    static void BoxBlur(Common::ImageRGB& image, int size) {
        if (size <= 1) return;

        auto [width, height] = image.GetSize();
        int radius = size / 2;

        std::vector<glm::vec3> temp(width * height);

        // Horizontal pass with running sum
        for (std::uint32_t y = 0; y < height; ++y) {
            glm::vec3 sum(0.0f);
            
            // Initialize sum for first pixel
            for (int k = -radius; k <= radius; ++k) {
                int sx = std::clamp(k, 0, static_cast<int>(width) - 1);
                sum += image.At(sx, y);
            }
            temp[y * width] = sum / static_cast<float>(size);
            
            // Slide window
            for (std::uint32_t x = 1; x < width; ++x) {
                int removeX = static_cast<int>(x) - radius - 1;
                int addX = static_cast<int>(x) + radius;
                
                removeX = std::clamp(removeX, 0, static_cast<int>(width) - 1);
                addX = std::clamp(addX, 0, static_cast<int>(width) - 1);
                
                sum -= image.At(removeX, y);
                sum += image.At(addX, y);
                
                temp[y * width + x] = sum / static_cast<float>(size);
            }
        }

        // Vertical pass with running sum
        for (std::uint32_t x = 0; x < width; ++x) {
            glm::vec3 sum(0.0f);
            
            // Initialize sum for first pixel
            for (int k = -radius; k <= radius; ++k) {
                int sy = std::clamp(k, 0, static_cast<int>(height) - 1);
                sum += temp[sy * width + x];
            }
            image.At(x, 0) = sum / static_cast<float>(size);
            
            // Slide window
            for (std::uint32_t y = 1; y < height; ++y) {
                int removeY = static_cast<int>(y) - radius - 1;
                int addY = static_cast<int>(y) + radius;
                
                removeY = std::clamp(removeY, 0, static_cast<int>(height) - 1);
                addY = std::clamp(addY, 0, static_cast<int>(height) - 1);
                
                sum -= temp[removeY * width + x];
                sum += temp[addY * width + x];
                
                image.At(x, y) = sum / static_cast<float>(size);
            }
        }
    }
};

//=============================================================================
// SVG Filter Effect Base
//=============================================================================
class SVGFilterEffect {
public:
    virtual ~SVGFilterEffect() = default;
    virtual void Apply(Common::ImageRGB& image) = 0;
};

//=============================================================================
// feGaussianBlur filter effect
//=============================================================================
class FeGaussianBlur : public SVGFilterEffect {
public:
    float stdDeviationX = 0.0f;
    float stdDeviationY = 0.0f;

    FeGaussianBlur() = default;
    FeGaussianBlur(float stdDev) : stdDeviationX(stdDev), stdDeviationY(stdDev) {}
    FeGaussianBlur(float stdX, float stdY) : stdDeviationX(stdX), stdDeviationY(stdY) {}

    void Apply(Common::ImageRGB& image) override {
        if (stdDeviationX == stdDeviationY) {
            GaussianBlur::Blur(image, stdDeviationX);
        } else {
            GaussianBlur::BlurXY(image, stdDeviationX, stdDeviationY);
        }
    }
};

//=============================================================================
// Drop Shadow Effect (common SVG pattern)
//=============================================================================
class DropShadow {
public:
    static void Apply(Common::ImageRGB& image, 
                      float offsetX, float offsetY, 
                      float blurRadius,
                      const glm::vec3& shadowColor,
                      float opacity = 0.5f) {
        auto [width, height] = image.GetSize();
        
        // Create shadow layer
        Common::ImageRGB shadow(width, height);
        
        // Copy and offset image as shadow base
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                int srcX = static_cast<int>(x) - static_cast<int>(offsetX);
                int srcY = static_cast<int>(y) - static_cast<int>(offsetY);
                
                if (srcX >= 0 && srcX < static_cast<int>(width) &&
                    srcY >= 0 && srcY < static_cast<int>(height)) {
                    // Use luminance as alpha
                    glm::vec3 pixel = image.At(srcX, srcY);
                    float lum = 0.299f * pixel.r + 0.587f * pixel.g + 0.114f * pixel.b;
                    shadow.At(x, y) = shadowColor * lum;
                } else {
                    shadow.At(x, y) = glm::vec3(1.0f);  // White (no shadow)
                }
            }
        }
        
        // Blur shadow
        if (blurRadius > 0) {
            GaussianBlur::Blur(shadow, blurRadius);
        }
        
        // Composite: shadow behind original
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                glm::vec3 shadowPixel = shadow.At(x, y);
                glm::vec3 originalPixel = image.At(x, y);
                
                // Simple blending
                image.At(x, y) = originalPixel * (1.0f - opacity) + 
                                 shadowPixel * opacity * (1.0f - (originalPixel.r + originalPixel.g + originalPixel.b) / 3.0f) +
                                 originalPixel * (originalPixel.r + originalPixel.g + originalPixel.b) / 3.0f;
            }
        }
    }
};

} // namespace VCX::Labs::SVG
