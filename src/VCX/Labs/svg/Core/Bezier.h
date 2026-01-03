#pragma once

#include "Math2D.h"
#include <vector>
#include <functional>

namespace VCX::Labs::SVG {

//=============================================================================
// Bezier Curve Utilities - Adaptive Tessellation
//=============================================================================
class Bezier {
public:
    //=========================================================================
    // Quadratic Bézier evaluation: B(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
    //=========================================================================
    static Vec2 EvaluateQuadratic(const Vec2& p0, const Vec2& p1, const Vec2& p2, float t) {
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float t2 = t * t;
        return p0 * mt2 + p1 * (2.0f * mt * t) + p2 * t2;
    }

    //=========================================================================
    // Cubic Bézier evaluation: B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
    //=========================================================================
    static Vec2 EvaluateCubic(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t) {
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float mt3 = mt2 * mt;
        float t2 = t * t;
        float t3 = t2 * t;
        return p0 * mt3 + p1 * (3.0f * mt2 * t) + p2 * (3.0f * mt * t2) + p3 * t3;
    }

    //=========================================================================
    // Quadratic Bézier derivative: B'(t) = 2(1-t)(P₁-P₀) + 2t(P₂-P₁)
    //=========================================================================
    static Vec2 QuadraticDerivative(const Vec2& p0, const Vec2& p1, const Vec2& p2, float t) {
        float mt = 1.0f - t;
        return (p1 - p0) * (2.0f * mt) + (p2 - p1) * (2.0f * t);
    }

    //=========================================================================
    // Cubic Bézier derivative
    //=========================================================================
    static Vec2 CubicDerivative(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t) {
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float t2 = t * t;
        return (p1 - p0) * (3.0f * mt2) + 
               (p2 - p1) * (6.0f * mt * t) + 
               (p3 - p2) * (3.0f * t2);
    }

    //=========================================================================
    // Flatness Error Calculation
    // Measures how far control points deviate from the chord p0-p3
    //=========================================================================
    static float QuadraticFlatnessError(const Vec2& p0, const Vec2& p1, const Vec2& p2) {
        // For quadratic: distance from p1 to line p0-p2
        Vec2 d = p2 - p0;
        float len2 = d.LengthSquared();
        if (len2 < 1e-10f) {
            return (p1 - p0).Length();
        }
        
        // Distance from p1 to chord p0-p2
        Vec2 n = Vec2(-d.y, d.x);  // Perpendicular
        float dist = std::abs(Dot(p1 - p0, n)) / std::sqrt(len2);
        return dist;
    }

    static float CubicFlatnessError(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3) {
        // For cubic: max distance from p1 and p2 to line p0-p3
        Vec2 d = p3 - p0;
        float len2 = d.LengthSquared();
        if (len2 < 1e-10f) {
            return std::max((p1 - p0).Length(), (p2 - p0).Length());
        }
        
        Vec2 n = Vec2(-d.y, d.x);  // Perpendicular
        float invLen = 1.0f / std::sqrt(len2);
        float d1 = std::abs(Dot(p1 - p0, n)) * invLen;
        float d2 = std::abs(Dot(p2 - p0, n)) * invLen;
        return std::max(d1, d2);
    }

    //=========================================================================
    // De Casteljau Subdivision at t=0.5
    //=========================================================================
    static void SubdivideQuadraticMid(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                                      Vec2& left0, Vec2& left1, Vec2& left2,
                                      Vec2& right0, Vec2& right1, Vec2& right2) {
        Vec2 p01 = (p0 + p1) * 0.5f;
        Vec2 p12 = (p1 + p2) * 0.5f;
        Vec2 p012 = (p01 + p12) * 0.5f;
        
        left0 = p0;
        left1 = p01;
        left2 = p012;
        
        right0 = p012;
        right1 = p12;
        right2 = p2;
    }

    static void SubdivideCubicMid(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                                   Vec2& left0, Vec2& left1, Vec2& left2, Vec2& left3,
                                   Vec2& right0, Vec2& right1, Vec2& right2, Vec2& right3) {
        Vec2 p01 = (p0 + p1) * 0.5f;
        Vec2 p12 = (p1 + p2) * 0.5f;
        Vec2 p23 = (p2 + p3) * 0.5f;
        Vec2 p012 = (p01 + p12) * 0.5f;
        Vec2 p123 = (p12 + p23) * 0.5f;
        Vec2 p0123 = (p012 + p123) * 0.5f;
        
        left0 = p0;
        left1 = p01;
        left2 = p012;
        left3 = p0123;
        
        right0 = p0123;
        right1 = p123;
        right2 = p23;
        right3 = p3;
    }

    //=========================================================================
    // Adaptive Tessellation - Quadratic
    //=========================================================================
    static void TessellateQuadraticAdaptive(const Vec2& p0, const Vec2& p1, const Vec2& p2,
                                             float tolerance, std::vector<Vec2>& output,
                                             int depth = 0, int maxDepth = 10) {
        if (depth >= maxDepth || QuadraticFlatnessError(p0, p1, p2) <= tolerance) {
            output.push_back(p2);
            return;
        }
        
        Vec2 l0, l1, l2, r0, r1, r2;
        SubdivideQuadraticMid(p0, p1, p2, l0, l1, l2, r0, r1, r2);
        
        TessellateQuadraticAdaptive(l0, l1, l2, tolerance, output, depth + 1, maxDepth);
        TessellateQuadraticAdaptive(r0, r1, r2, tolerance, output, depth + 1, maxDepth);
    }

    //=========================================================================
    // Adaptive Tessellation - Cubic
    //=========================================================================
    static void TessellateCubicAdaptive(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                                         float tolerance, std::vector<Vec2>& output,
                                         int depth = 0, int maxDepth = 10) {
        if (depth >= maxDepth || CubicFlatnessError(p0, p1, p2, p3) <= tolerance) {
            output.push_back(p3);
            return;
        }
        
        Vec2 l0, l1, l2, l3, r0, r1, r2, r3;
        SubdivideCubicMid(p0, p1, p2, p3, l0, l1, l2, l3, r0, r1, r2, r3);
        
        TessellateCubicAdaptive(l0, l1, l2, l3, tolerance, output, depth + 1, maxDepth);
        TessellateCubicAdaptive(r0, r1, r2, r3, tolerance, output, depth + 1, maxDepth);
    }

    //=========================================================================
    // Arc to Cubic Bézier conversion
    // Converts an elliptical arc to cubic Bézier curves
    //=========================================================================
    static void ArcToCubics(const Vec2& start, float rx, float ry, float rotation,
                            bool largeArc, bool sweep, const Vec2& end,
                            std::vector<Vec2>& controlPoints);

    //=========================================================================
    // Bounding box calculation
    //=========================================================================
    static BBox QuadraticBBox(const Vec2& p0, const Vec2& p1, const Vec2& p2) {
        BBox bbox;
        bbox.Expand(p0);
        bbox.Expand(p2);
        
        // Check extrema at derivative = 0
        // d/dt[(1-t)²p0 + 2(1-t)t*p1 + t²p2] = 2(1-t)(p1-p0) + 2t(p2-p1) = 0
        // t = (p0 - p1) / (p0 - 2*p1 + p2)
        
        for (int i = 0; i < 2; ++i) {
            float p0i = (i == 0) ? p0.x : p0.y;
            float p1i = (i == 0) ? p1.x : p1.y;
            float p2i = (i == 0) ? p2.x : p2.y;
            
            float denom = p0i - 2.0f * p1i + p2i;
            if (std::abs(denom) > 1e-6f) {
                float t = (p0i - p1i) / denom;
                if (t > 0.0f && t < 1.0f) {
                    bbox.Expand(EvaluateQuadratic(p0, p1, p2, t));
                }
            }
        }
        
        return bbox;
    }

    static BBox CubicBBox(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3) {
        BBox bbox;
        bbox.Expand(p0);
        bbox.Expand(p3);
        
        // Find extrema by solving derivative = 0 for each axis
        // d/dt[cubic] = 3(1-t)²(p1-p0) + 6(1-t)t(p2-p1) + 3t²(p3-p2)
        // This is quadratic: at² + bt + c = 0
        
        for (int axis = 0; axis < 2; ++axis) {
            float p0v = (axis == 0) ? p0.x : p0.y;
            float p1v = (axis == 0) ? p1.x : p1.y;
            float p2v = (axis == 0) ? p2.x : p2.y;
            float p3v = (axis == 0) ? p3.x : p3.y;
            
            float a = -p0v + 3.0f * p1v - 3.0f * p2v + p3v;
            float b = 2.0f * p0v - 4.0f * p1v + 2.0f * p2v;
            float c = -p0v + p1v;
            
            float roots[2];
            int numRoots = SolveQuadratic(a, b, c, roots);
            
            for (int i = 0; i < numRoots; ++i) {
                if (roots[i] > 0.0f && roots[i] < 1.0f) {
                    bbox.Expand(EvaluateCubic(p0, p1, p2, p3, roots[i]));
                }
            }
        }
        
        return bbox;
    }

private:
    // Solve quadratic equation ax² + bx + c = 0
    static int SolveQuadratic(float a, float b, float c, float roots[2]) {
        if (std::abs(a) < 1e-10f) {
            // Linear equation
            if (std::abs(b) < 1e-10f) {
                return 0;
            }
            roots[0] = -c / b;
            return 1;
        }
        
        float disc = b * b - 4.0f * a * c;
        if (disc < 0) {
            return 0;
        }
        
        float sqrtDisc = std::sqrt(disc);
        float inv2a = 0.5f / a;
        roots[0] = (-b - sqrtDisc) * inv2a;
        roots[1] = (-b + sqrtDisc) * inv2a;
        return (disc > 0) ? 2 : 1;
    }
};

//=============================================================================
// Arc to Cubic implementation
//=============================================================================
inline void Bezier::ArcToCubics(const Vec2& start, float rx, float ry, float rotation,
                                 bool largeArc, bool sweep, const Vec2& end,
                                 std::vector<Vec2>& controlPoints) {
    // Handle degenerate cases
    if ((start - end).LengthSquared() < 1e-10f) {
        return;
    }
    
    if (rx < 1e-6f || ry < 1e-6f) {
        // Degenerate to line
        controlPoints.push_back(start);
        controlPoints.push_back(end);
        controlPoints.push_back(end);
        return;
    }
    
    // Compute the center parameterization
    float cosRot = std::cos(rotation);
    float sinRot = std::sin(rotation);
    
    // Transform to unit circle space
    Vec2 mid = (start - end) * 0.5f;
    Vec2 midPrime(
        cosRot * mid.x + sinRot * mid.y,
        -sinRot * mid.x + cosRot * mid.y
    );
    
    float px = midPrime.x / rx;
    float py = midPrime.y / ry;
    float pl2 = px * px + py * py;
    
    // Scale radii if necessary
    if (pl2 > 1.0f) {
        float scale = std::sqrt(pl2);
        rx *= scale;
        ry *= scale;
        px = midPrime.x / rx;
        py = midPrime.y / ry;
        pl2 = 1.0f;
    }
    
    // Compute center
    float sq = std::sqrt(std::max(0.0f, (1.0f - pl2) / pl2));
    if (largeArc == sweep) {
        sq = -sq;
    }
    
    Vec2 centerPrime(sq * ry * py / rx, -sq * rx * px / ry);
    
    // Transform back
    Vec2 centerOffset(
        cosRot * centerPrime.x - sinRot * centerPrime.y,
        sinRot * centerPrime.x + cosRot * centerPrime.y
    );
    Vec2 center = (start + end) * 0.5f + centerOffset;
    
    // Compute start and end angles
    auto angleVec = [](const Vec2& u, const Vec2& v) -> float {
        float cross = Cross(u, v);
        float dot = Dot(u, v);
        return std::atan2(cross, dot);
    };
    
    Vec2 v1((px - centerPrime.x), (py - centerPrime.y));
    Vec2 v2((-px - centerPrime.x), (-py - centerPrime.y));
    
    float theta1 = angleVec(Vec2(1, 0), v1);
    float dtheta = angleVec(v1, v2);
    
    // Adjust sweep direction
    if (sweep && dtheta < 0) {
        dtheta += 2.0f * 3.14159265359f;
    } else if (!sweep && dtheta > 0) {
        dtheta -= 2.0f * 3.14159265359f;
    }
    
    // Split into cubic segments (max 90 degrees each)
    int numSegments = static_cast<int>(std::ceil(std::abs(dtheta) / (3.14159265359f * 0.5f)));
    numSegments = std::max(1, numSegments);
    
    float segmentAngle = dtheta / numSegments;
    
    // Magic number for cubic approximation of arc
    float k = 4.0f / 3.0f * std::tan(segmentAngle * 0.25f);
    
    Vec2 currentPoint = start;
    float currentAngle = theta1;
    
    for (int i = 0; i < numSegments; ++i) {
        float nextAngle = currentAngle + segmentAngle;
        
        // Compute control points on unit circle
        float cos1 = std::cos(currentAngle);
        float sin1 = std::sin(currentAngle);
        float cos2 = std::cos(nextAngle);
        float sin2 = std::sin(nextAngle);
        
        Vec2 p1Prime(cos1 - k * sin1, sin1 + k * cos1);
        Vec2 p2Prime(cos2 + k * sin2, sin2 - k * cos2);
        Vec2 p3Prime(cos2, sin2);
        
        // Transform back to original space
        auto transformPoint = [&](Vec2 p) -> Vec2 {
            Vec2 scaled(p.x * rx, p.y * ry);
            return Vec2(
                cosRot * scaled.x - sinRot * scaled.y + center.x,
                sinRot * scaled.x + cosRot * scaled.y + center.y
            );
        };
        
        Vec2 p1 = transformPoint(p1Prime);
        Vec2 p2 = transformPoint(p2Prime);
        Vec2 p3 = transformPoint(p3Prime);
        
        controlPoints.push_back(p1);
        controlPoints.push_back(p2);
        controlPoints.push_back(p3);
        
        currentPoint = p3;
        currentAngle = nextAngle;
    }
}

} // namespace VCX::Labs::SVG
