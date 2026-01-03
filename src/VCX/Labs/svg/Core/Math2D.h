#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <glm/glm.hpp>

namespace VCX::Labs::SVG {

//=============================================================================
// Vec2 - 2D Vector class with extended operations
//=============================================================================
struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2(const glm::vec2& v) : x(v.x), y(v.y) {}

    // Operators
    Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
    Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    Vec2 operator-() const { return Vec2(-x, -y); }
    
    Vec2& operator+=(const Vec2& v) { x += v.x; y += v.y; return *this; }
    Vec2& operator-=(const Vec2& v) { x -= v.x; y -= v.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(float s) { x /= s; y /= s; return *this; }
    
    bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vec2& v) const { return !(*this == v); }

    // Methods
    float Length() const { return std::sqrt(x * x + y * y); }
    float LengthSquared() const { return x * x + y * y; }
    
    Vec2 Normalized() const {
        float len = Length();
        return len > 1e-8f ? *this / len : Vec2(0, 0);
    }
    
    Vec2 Perpendicular() const { return Vec2(-y, x); }
    Vec2 PerpendicularCW() const { return Vec2(y, -x); }
    
    // Convert to glm
    glm::vec2 ToGlm() const { return glm::vec2(x, y); }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }
inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
inline float Cross(const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; }
inline float Distance(const Vec2& a, const Vec2& b) { return (a - b).Length(); }
inline float DistanceSquared(const Vec2& a, const Vec2& b) { return (a - b).LengthSquared(); }

inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
    return a + (b - a) * t;
}

inline Vec2 Reflect(const Vec2& v, const Vec2& n) {
    return v - 2.0f * Dot(v, n) * n;
}

//=============================================================================
// BBox - Axis-Aligned Bounding Box
//=============================================================================
struct BBox {
    Vec2 min, max;

    BBox() : min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
             max(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()) {}
    
    BBox(const Vec2& min, const Vec2& max) : min(min), max(max) {}
    BBox(float minX, float minY, float maxX, float maxY) 
        : min(minX, minY), max(maxX, maxY) {}

    bool IsValid() const { return min.x <= max.x && min.y <= max.y; }
    bool IsEmpty() const { return min.x > max.x || min.y > max.y; }

    float Width() const { return max.x - min.x; }
    float Height() const { return max.y - min.y; }
    Vec2 Size() const { return max - min; }
    Vec2 Center() const { return (min + max) * 0.5f; }

    void Expand(const Vec2& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
    }

    void Expand(const BBox& other) {
        Expand(other.min);
        Expand(other.max);
    }

    void Expand(float padding) {
        min.x -= padding;
        min.y -= padding;
        max.x += padding;
        max.y += padding;
    }

    bool Contains(const Vec2& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
    }

    bool Intersects(const BBox& other) const {
        return !(other.max.x < min.x || other.min.x > max.x ||
                 other.max.y < min.y || other.min.y > max.y);
    }

    BBox Intersection(const BBox& other) const {
        return BBox(
            std::max(min.x, other.min.x), std::max(min.y, other.min.y),
            std::min(max.x, other.max.x), std::min(max.y, other.max.y)
        );
    }
};

//=============================================================================
// Matrix3x3 - 2D Transformation Matrix (3x3 affine)
//=============================================================================
struct Matrix3x3 {
    // Column-major storage: m[col][row]
    std::array<std::array<float, 3>, 3> m;

    Matrix3x3() {
        SetIdentity();
    }

    void SetIdentity() {
        m[0] = {1, 0, 0};
        m[1] = {0, 1, 0};
        m[2] = {0, 0, 1};
    }

    static Matrix3x3 Identity() {
        return Matrix3x3();
    }

    static Matrix3x3 Translation(float tx, float ty) {
        Matrix3x3 mat;
        mat.m[2][0] = tx;
        mat.m[2][1] = ty;
        return mat;
    }

    static Matrix3x3 Scale(float sx, float sy) {
        Matrix3x3 mat;
        mat.m[0][0] = sx;
        mat.m[1][1] = sy;
        return mat;
    }

    static Matrix3x3 Scale(float s) {
        return Scale(s, s);
    }

    static Matrix3x3 Rotation(float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        Matrix3x3 mat;
        mat.m[0][0] = c;  mat.m[0][1] = s;
        mat.m[1][0] = -s; mat.m[1][1] = c;
        return mat;
    }

    static Matrix3x3 SkewX(float radians) {
        Matrix3x3 mat;
        mat.m[1][0] = std::tan(radians);
        return mat;
    }

    static Matrix3x3 SkewY(float radians) {
        Matrix3x3 mat;
        mat.m[0][1] = std::tan(radians);
        return mat;
    }

    // Transform a point
    Vec2 TransformPoint(const Vec2& p) const {
        float x = m[0][0] * p.x + m[1][0] * p.y + m[2][0];
        float y = m[0][1] * p.x + m[1][1] * p.y + m[2][1];
        return Vec2(x, y);
    }

    // Transform a vector (ignores translation)
    Vec2 TransformVector(const Vec2& v) const {
        float x = m[0][0] * v.x + m[1][0] * v.y;
        float y = m[0][1] * v.x + m[1][1] * v.y;
        return Vec2(x, y);
    }

    // Transform a direction (normalized result)
    Vec2 TransformDirection(const Vec2& d) const {
        return TransformVector(d).Normalized();
    }

    // Matrix multiplication
    Matrix3x3 operator*(const Matrix3x3& other) const {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = 0;
                for (int k = 0; k < 3; ++k) {
                    result.m[i][j] += m[k][j] * other.m[i][k];
                }
            }
        }
        return result;
    }

    Matrix3x3& operator*=(const Matrix3x3& other) {
        *this = *this * other;
        return *this;
    }

    // Determinant
    float Determinant() const {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1])
             + m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
    }

    // Inverse
    Matrix3x3 Inverse() const {
        float det = Determinant();
        if (std::abs(det) < 1e-10f) {
            return Identity();
        }
        float invDet = 1.0f / det;
        
        Matrix3x3 result;
        result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
        result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
        result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
        result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
        result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
        result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
        result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
        result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
        result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
        return result;
    }

    // Get scale factor (for stroke width scaling)
    float GetScaleFactor() const {
        // Use the average of the two axis scale factors
        Vec2 sx = TransformVector(Vec2(1, 0));
        Vec2 sy = TransformVector(Vec2(0, 1));
        return (sx.Length() + sy.Length()) * 0.5f;
    }

    // Convert to glm::mat3
    glm::mat3 ToGlm() const {
        return glm::mat3(
            m[0][0], m[0][1], m[0][2],
            m[1][0], m[1][1], m[1][2],
            m[2][0], m[2][1], m[2][2]
        );
    }

    // Create from glm::mat3
    static Matrix3x3 FromGlm(const glm::mat3& mat) {
        Matrix3x3 result;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = mat[i][j];
            }
        }
        return result;
    }
};

//=============================================================================
// TransformStack - Hierarchical transform management
//=============================================================================
class TransformStack {
public:
    TransformStack() {
        _stack.push_back(Matrix3x3::Identity());
    }

    void Push() {
        _stack.push_back(_stack.back());
    }

    void Pop() {
        if (_stack.size() > 1) {
            _stack.pop_back();
        }
    }

    void Reset() {
        _stack.clear();
        _stack.push_back(Matrix3x3::Identity());
    }

    Matrix3x3& Current() { return _stack.back(); }
    const Matrix3x3& Current() const { return _stack.back(); }

    void Translate(float tx, float ty) {
        _stack.back() = _stack.back() * Matrix3x3::Translation(tx, ty);
    }

    void Scale(float sx, float sy) {
        _stack.back() = _stack.back() * Matrix3x3::Scale(sx, sy);
    }

    void Rotate(float radians) {
        _stack.back() = _stack.back() * Matrix3x3::Rotation(radians);
    }

    void Multiply(const Matrix3x3& mat) {
        _stack.back() = _stack.back() * mat;
    }

    Vec2 TransformPoint(const Vec2& p) const {
        return _stack.back().TransformPoint(p);
    }

    Vec2 TransformVector(const Vec2& v) const {
        return _stack.back().TransformVector(v);
    }

    size_t Depth() const { return _stack.size(); }

private:
    std::vector<Matrix3x3> _stack;
};

//=============================================================================
// Geometry utilities
//=============================================================================
namespace Geometry {

// Point-to-line distance
inline float PointToLineDistance(const Vec2& point, const Vec2& lineStart, const Vec2& lineEnd) {
    Vec2 d = lineEnd - lineStart;
    float len2 = d.LengthSquared();
    if (len2 < 1e-10f) {
        return Distance(point, lineStart);
    }
    
    float t = std::clamp(Dot(point - lineStart, d) / len2, 0.0f, 1.0f);
    Vec2 projection = lineStart + d * t;
    return Distance(point, projection);
}

// Point-to-segment distance
inline float PointToSegmentDistance(const Vec2& point, const Vec2& a, const Vec2& b) {
    return PointToLineDistance(point, a, b);
}

// Line-line intersection
inline bool LineIntersection(const Vec2& p1, const Vec2& p2, 
                             const Vec2& p3, const Vec2& p4,
                             Vec2& intersection, float& t1, float& t2) {
    Vec2 d1 = p2 - p1;
    Vec2 d2 = p4 - p3;
    float cross = Cross(d1, d2);
    
    if (std::abs(cross) < 1e-10f) {
        return false;  // Parallel or coincident
    }
    
    Vec2 d3 = p3 - p1;
    t1 = Cross(d3, d2) / cross;
    t2 = Cross(d3, d1) / cross;
    
    intersection = p1 + d1 * t1;
    return true;
}

// Segment-segment intersection
inline bool SegmentIntersection(const Vec2& p1, const Vec2& p2, 
                                const Vec2& p3, const Vec2& p4,
                                Vec2& intersection) {
    float t1, t2;
    if (!LineIntersection(p1, p2, p3, p4, intersection, t1, t2)) {
        return false;
    }
    return t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f;
}

// Triangle signed area (positive = CCW)
inline float TriangleSignedArea(const Vec2& a, const Vec2& b, const Vec2& c) {
    return 0.5f * Cross(b - a, c - a);
}

// Point in triangle test
inline bool PointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    float sign1 = Cross(b - a, p - a);
    float sign2 = Cross(c - b, p - b);
    float sign3 = Cross(a - c, p - c);
    
    bool hasNeg = (sign1 < 0) || (sign2 < 0) || (sign3 < 0);
    bool hasPos = (sign1 > 0) || (sign2 > 0) || (sign3 > 0);
    
    return !(hasNeg && hasPos);
}

// Polygon signed area
inline float PolygonSignedArea(const std::vector<Vec2>& polygon) {
    if (polygon.size() < 3) return 0;
    
    float area = 0;
    size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2& p1 = polygon[i];
        const Vec2& p2 = polygon[(i + 1) % n];
        area += Cross(p1, p2);
    }
    return area * 0.5f;
}

// Check if polygon is clockwise
inline bool IsClockwise(const std::vector<Vec2>& polygon) {
    return PolygonSignedArea(polygon) < 0;
}

// Compute polygon centroid
inline Vec2 PolygonCentroid(const std::vector<Vec2>& polygon) {
    if (polygon.empty()) return Vec2(0, 0);
    
    Vec2 sum(0, 0);
    for (const auto& p : polygon) {
        sum += p;
    }
    return sum / static_cast<float>(polygon.size());
}

// Compute bounding box of points
inline BBox ComputeBBox(const std::vector<Vec2>& points) {
    BBox bbox;
    for (const auto& p : points) {
        bbox.Expand(p);
    }
    return bbox;
}

} // namespace Geometry

} // namespace VCX::Labs::SVG
