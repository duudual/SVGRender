#pragma once

#include "Core/Math2D.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace VCX::Labs::SVG {

//=============================================================================
// Color Stop for gradients
//=============================================================================
struct ColorStop {
    float offset;       // 0.0 - 1.0
    glm::vec4 color;    // RGBA

    ColorStop() : offset(0), color(0, 0, 0, 1) {}
    ColorStop(float offset, const glm::vec4& color) : offset(offset), color(color) {}

    bool operator<(const ColorStop& other) const {
        return offset < other.offset;
    }
};

//=============================================================================
// Spread Method (how gradient extends beyond its defined area)
//=============================================================================
enum class SpreadMethod {
    Pad,        // Extend edge colors (default)
    Repeat,     // Tile the gradient
    Reflect     // Mirror the gradient
};

//=============================================================================
// Gradient Unit Type
//=============================================================================
enum class GradientUnits {
    ObjectBoundingBox,  // Relative to object bounds (default)
    UserSpaceOnUse      // Absolute coordinates
};

//=============================================================================
// Linear Gradient
//=============================================================================
class LinearGradient {
public:
    Vec2 start;         // Start point
    Vec2 end;           // End point
    std::vector<ColorStop> stops;
    SpreadMethod spreadMethod = SpreadMethod::Pad;
    GradientUnits units = GradientUnits::ObjectBoundingBox;
    Matrix3x3 transform;

    LinearGradient() : start(0, 0), end(1, 0) {
        transform.SetIdentity();
    }

    LinearGradient(const Vec2& start, const Vec2& end) : start(start), end(end) {
        transform.SetIdentity();
    }

    void AddStop(float offset, const glm::vec4& color) {
        stops.emplace_back(offset, color);
        SortStops();
    }

    void SortStops() {
        std::sort(stops.begin(), stops.end());
    }

    // Sample gradient at a point
    glm::vec4 Sample(const Vec2& point) const {
        return Sample(point, BBox(0, 0, 1, 1));
    }

    // Sample with object bounding box for relative coordinates
    glm::vec4 Sample(const Vec2& point, const BBox& objectBounds) const {
        if (stops.empty()) {
            return glm::vec4(0, 0, 0, 1);
        }

        // Transform point to gradient space
        Vec2 p = transform.Inverse().TransformPoint(point);

        // Convert coordinates based on units
        Vec2 gradStart = start;
        Vec2 gradEnd = end;

        if (units == GradientUnits::ObjectBoundingBox) {
            // Relative to object bounds
            gradStart = Vec2(
                objectBounds.min.x + start.x * objectBounds.Width(),
                objectBounds.min.y + start.y * objectBounds.Height()
            );
            gradEnd = Vec2(
                objectBounds.min.x + end.x * objectBounds.Width(),
                objectBounds.min.y + end.y * objectBounds.Height()
            );
        }

        // Project point onto gradient axis
        Vec2 axis = gradEnd - gradStart;
        float axisLen2 = axis.LengthSquared();
        
        float t;
        if (axisLen2 < 1e-10f) {
            t = 0.0f;
        } else {
            t = Dot(p - gradStart, axis) / axisLen2;
        }

        // Apply spread method
        t = ApplySpread(t);

        // Interpolate color
        return InterpolateColor(t);
    }

private:
    float ApplySpread(float t) const {
        switch (spreadMethod) {
            case SpreadMethod::Pad:
                return std::clamp(t, 0.0f, 1.0f);

            case SpreadMethod::Repeat:
                return t - std::floor(t);

            case SpreadMethod::Reflect: {
                t = std::abs(t);
                int n = static_cast<int>(std::floor(t));
                t = t - n;
                if (n & 1) {
                    t = 1.0f - t;
                }
                return t;
            }
        }
        return t;
    }

    glm::vec4 InterpolateColor(float t) const {
        if (stops.empty()) return glm::vec4(0, 0, 0, 1);
        if (stops.size() == 1) return stops[0].color;

        // Find surrounding stops
        if (t <= stops.front().offset) return stops.front().color;
        if (t >= stops.back().offset) return stops.back().color;

        for (size_t i = 0; i < stops.size() - 1; ++i) {
            if (t >= stops[i].offset && t <= stops[i + 1].offset) {
                float localT = (t - stops[i].offset) / (stops[i + 1].offset - stops[i].offset);
                return glm::mix(stops[i].color, stops[i + 1].color, localT);
            }
        }

        return stops.back().color;
    }
};

//=============================================================================
// Radial Gradient
//=============================================================================
class RadialGradient {
public:
    Vec2 center;        // Center of outer circle
    Vec2 focal;         // Focal point (center of inner circle)
    float radius;       // Radius of outer circle
    float focalRadius;  // Radius at focal point (usually 0)
    std::vector<ColorStop> stops;
    SpreadMethod spreadMethod = SpreadMethod::Pad;
    GradientUnits units = GradientUnits::ObjectBoundingBox;
    Matrix3x3 transform;

    RadialGradient() : center(0.5f, 0.5f), focal(0.5f, 0.5f), radius(0.5f), focalRadius(0) {
        transform.SetIdentity();
    }

    RadialGradient(const Vec2& center, float radius)
        : center(center), focal(center), radius(radius), focalRadius(0) {
        transform.SetIdentity();
    }

    RadialGradient(const Vec2& center, float radius, const Vec2& focal)
        : center(center), focal(focal), radius(radius), focalRadius(0) {
        transform.SetIdentity();
    }

    void AddStop(float offset, const glm::vec4& color) {
        stops.emplace_back(offset, color);
        SortStops();
    }

    void SortStops() {
        std::sort(stops.begin(), stops.end());
    }

    glm::vec4 Sample(const Vec2& point) const {
        return Sample(point, BBox(0, 0, 1, 1));
    }

    glm::vec4 Sample(const Vec2& point, const BBox& objectBounds) const {
        if (stops.empty()) {
            return glm::vec4(0, 0, 0, 1);
        }

        // Transform point to gradient space
        Vec2 p = transform.Inverse().TransformPoint(point);

        // Convert coordinates based on units
        Vec2 gradCenter = center;
        Vec2 gradFocal = focal;
        float gradRadius = radius;

        if (units == GradientUnits::ObjectBoundingBox) {
            float w = objectBounds.Width();
            float h = objectBounds.Height();
            gradCenter = Vec2(
                objectBounds.min.x + center.x * w,
                objectBounds.min.y + center.y * h
            );
            gradFocal = Vec2(
                objectBounds.min.x + focal.x * w,
                objectBounds.min.y + focal.y * h
            );
            // Use average of width/height for radius
            gradRadius = radius * (w + h) * 0.5f;
        }

        // Compute gradient parameter
        float t = ComputeGradientT(p, gradCenter, gradFocal, gradRadius);

        // Apply spread method
        t = ApplySpread(t);

        // Interpolate color
        return InterpolateColor(t);
    }

private:
    float ComputeGradientT(const Vec2& p, const Vec2& c, const Vec2& f, float r) const {
        // Simple radial: distance from center normalized by radius
        if ((c - f).LengthSquared() < 1e-10f) {
            // No focal offset
            float dist = (p - c).Length();
            return dist / std::max(r, 0.001f);
        }

        // With focal point - solve for t in parametric circle equation
        // This is more complex; using simplified version
        Vec2 d = p - f;
        float dist = d.Length();
        
        // Approximate: use distance from focal normalized by (radius - focal distance)
        float focalDist = (c - f).Length();
        float effectiveRadius = r - focalDist * (1.0f - focalRadius / r);
        
        return dist / std::max(effectiveRadius, 0.001f);
    }

    float ApplySpread(float t) const {
        switch (spreadMethod) {
            case SpreadMethod::Pad:
                return std::clamp(t, 0.0f, 1.0f);

            case SpreadMethod::Repeat:
                return t - std::floor(t);

            case SpreadMethod::Reflect: {
                t = std::abs(t);
                int n = static_cast<int>(std::floor(t));
                t = t - n;
                if (n & 1) {
                    t = 1.0f - t;
                }
                return t;
            }
        }
        return t;
    }

    glm::vec4 InterpolateColor(float t) const {
        if (stops.empty()) return glm::vec4(0, 0, 0, 1);
        if (stops.size() == 1) return stops[0].color;

        if (t <= stops.front().offset) return stops.front().color;
        if (t >= stops.back().offset) return stops.back().color;

        for (size_t i = 0; i < stops.size() - 1; ++i) {
            if (t >= stops[i].offset && t <= stops[i + 1].offset) {
                float localT = (t - stops[i].offset) / (stops[i + 1].offset - stops[i].offset);
                return glm::mix(stops[i].color, stops[i + 1].color, localT);
            }
        }

        return stops.back().color;
    }
};

//=============================================================================
// Paint class - abstraction over solid color, gradients, and patterns
//=============================================================================
class Paint {
public:
    enum class Type {
        None,
        SolidColor,
        LinearGradient,
        RadialGradient,
        Pattern
    };

    Paint() : _type(Type::None) {}
    
    Paint(const glm::vec4& color) : _type(Type::SolidColor), _solidColor(color) {}
    
    Paint(const LinearGradient& gradient) 
        : _type(Type::LinearGradient), _linearGradient(gradient) {}
    
    Paint(const RadialGradient& gradient) 
        : _type(Type::RadialGradient), _radialGradient(gradient) {}

    Type GetType() const { return _type; }
    bool IsNone() const { return _type == Type::None; }

    // Sample the paint at a given point
    glm::vec4 Sample(const Vec2& point, const BBox& objectBounds = BBox(0, 0, 1, 1)) const {
        switch (_type) {
            case Type::None:
                return glm::vec4(0, 0, 0, 0);
            case Type::SolidColor:
                return _solidColor;
            case Type::LinearGradient:
                return _linearGradient.Sample(point, objectBounds);
            case Type::RadialGradient:
                return _radialGradient.Sample(point, objectBounds);
            case Type::Pattern:
                // TODO: Implement pattern sampling
                return glm::vec4(0, 0, 0, 1);
        }
        return glm::vec4(0, 0, 0, 1);
    }

    // Accessors
    const glm::vec4& GetSolidColor() const { return _solidColor; }
    glm::vec4& GetSolidColor() { return _solidColor; }
    
    const LinearGradient& GetLinearGradient() const { return _linearGradient; }
    LinearGradient& GetLinearGradient() { return _linearGradient; }
    
    const RadialGradient& GetRadialGradient() const { return _radialGradient; }
    RadialGradient& GetRadialGradient() { return _radialGradient; }

    // Factory methods
    static Paint Solid(const glm::vec4& color) { return Paint(color); }
    static Paint Solid(float r, float g, float b, float a = 1.0f) { 
        return Paint(glm::vec4(r, g, b, a)); 
    }
    static Paint Linear(const Vec2& start, const Vec2& end) {
        return Paint(LinearGradient(start, end));
    }
    static Paint Radial(const Vec2& center, float radius) {
        return Paint(RadialGradient(center, radius));
    }

private:
    Type _type;
    glm::vec4 _solidColor;
    LinearGradient _linearGradient;
    RadialGradient _radialGradient;
};

} // namespace VCX::Labs::SVG
