#pragma once

#include "Core/Math2D.h"
#include "Core/Bezier.h"
#include <vector>
#include <cmath>

namespace VCX::Labs::SVG {

//=============================================================================
// Stroke styling enums (matching SVG spec)
//=============================================================================
enum class LineCap {
    Butt,       // Default: no extension
    Round,      // Semicircle extension
    Square      // Half-stroke-width extension
};

enum class LineJoin {
    Miter,      // Default: sharp corner
    Round,      // Rounded corner
    Bevel       // Beveled (cut) corner
};

//=============================================================================
// Stroke Style Parameters
//=============================================================================
struct StrokeStyle {
    float width = 1.0f;
    LineCap lineCap = LineCap::Butt;
    LineJoin lineJoin = LineJoin::Miter;
    float miterLimit = 4.0f;        // SVG default is 4
    std::vector<float> dashArray;   // Empty = solid line
    float dashOffset = 0.0f;
    
    float HalfWidth() const { return width * 0.5f; }
};

//=============================================================================
// Stroke Expander - Converts stroke to fill path
// This generates the outline of a stroked path
//=============================================================================
class StrokeExpander {
public:
    StrokeExpander(const StrokeStyle& style = StrokeStyle()) : _style(style) {}

    void SetStyle(const StrokeStyle& style) { _style = style; }
    const StrokeStyle& GetStyle() const { return _style; }

    // Main entry: expand a polyline to a filled polygon
    // Input: list of vertices (open or closed polyline)
    // Output: list of vertices forming the stroke outline (closed polygon)
    std::vector<Vec2> ExpandPolyline(const std::vector<Vec2>& vertices, bool closed);

    // Generate a stroke path as triangles for direct rendering
    void ExpandToTriangles(const std::vector<Vec2>& vertices, bool closed,
                           std::vector<Vec2>& outVertices,
                           std::vector<uint32_t>& outIndices);

    // Apply dash pattern to a polyline, returning multiple dash segments
    std::vector<std::vector<Vec2>> ApplyDashPattern(const std::vector<Vec2>& vertices, bool closed);

private:
    StrokeStyle _style;

    // Arc generation for round caps/joins
    void GenerateArc(const Vec2& center, float radius,
                     const Vec2& start, const Vec2& end,
                     bool clockwise,
                     std::vector<Vec2>& output);

    // Generate line cap
    void GenerateStartCap(const Vec2& point, const Vec2& direction,
                          std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide);
    
    void GenerateEndCap(const Vec2& point, const Vec2& direction,
                        std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide);

    // Generate line join
    void GenerateJoin(const Vec2& point, const Vec2& inDir, const Vec2& outDir,
                      std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide);

    // Compute offset point
    Vec2 OffsetPoint(const Vec2& point, const Vec2& direction, float offset) const;
};

//=============================================================================
// Implementation
//=============================================================================

inline std::vector<Vec2> StrokeExpander::ExpandPolyline(const std::vector<Vec2>& vertices, bool closed) {
    std::vector<Vec2> result;
    
    if (vertices.size() < 2) {
        return result;
    }

    float halfWidth = _style.HalfWidth();
    if (halfWidth < 0.01f) {
        return result;
    }

    // Build two sides of the stroke
    std::vector<Vec2> leftSide;
    std::vector<Vec2> rightSide;

    size_t n = vertices.size();
    
    // For closed paths, we need to handle the join at the start/end
    size_t startIdx = 0;
    size_t endIdx = n - 1;
    
    // Calculate direction vectors for each segment
    std::vector<Vec2> directions;
    for (size_t i = 0; i < n - 1; ++i) {
        Vec2 d = (vertices[i + 1] - vertices[i]).Normalized();
        directions.push_back(d);
    }
    if (closed && n >= 3) {
        Vec2 d = (vertices[0] - vertices[n - 1]).Normalized();
        directions.push_back(d);
    }

    // Generate start cap (for open paths)
    if (!closed) {
        GenerateStartCap(vertices[0], directions[0], leftSide, rightSide);
    }

    // Process each vertex
    for (size_t i = 0; i < n; ++i) {
        const Vec2& p = vertices[i];
        
        Vec2 inDir, outDir;
        bool hasIn = false, hasOut = false;

        // Get incoming direction
        if (i > 0) {
            inDir = directions[i - 1];
            hasIn = true;
        } else if (closed && !directions.empty()) {
            inDir = directions.back();
            hasIn = true;
        }

        // Get outgoing direction
        if (i < directions.size()) {
            outDir = directions[i];
            hasOut = true;
        }

        if (hasIn && hasOut) {
            // Internal vertex - generate join
            GenerateJoin(p, inDir, outDir, leftSide, rightSide);
        } else if (hasIn && !hasOut) {
            // End vertex (open path)
            Vec2 perp = inDir.Perpendicular();
            leftSide.push_back(p + perp * halfWidth);
            rightSide.push_back(p - perp * halfWidth);
        } else if (!hasIn && hasOut) {
            // Start vertex (open path)
            Vec2 perp = outDir.Perpendicular();
            leftSide.push_back(p + perp * halfWidth);
            rightSide.push_back(p - perp * halfWidth);
        }
    }

    // Generate end cap (for open paths)
    if (!closed) {
        GenerateEndCap(vertices.back(), directions.back(), leftSide, rightSide);
    }

    // Combine sides into single closed polygon
    // Left side forward, then right side backward
    result.reserve(leftSide.size() + rightSide.size());
    result.insert(result.end(), leftSide.begin(), leftSide.end());
    for (auto it = rightSide.rbegin(); it != rightSide.rend(); ++it) {
        result.push_back(*it);
    }

    return result;
}

inline void StrokeExpander::ExpandToTriangles(const std::vector<Vec2>& vertices, bool closed,
                                               std::vector<Vec2>& outVertices,
                                               std::vector<uint32_t>& outIndices) {
    if (vertices.size() < 2) return;

    float halfWidth = _style.HalfWidth();
    if (halfWidth < 0.01f) return;

    outVertices.clear();
    outIndices.clear();

    size_t n = vertices.size();
    
    // Calculate direction vectors
    std::vector<Vec2> directions;
    for (size_t i = 0; i < n - 1; ++i) {
        Vec2 d = (vertices[i + 1] - vertices[i]).Normalized();
        directions.push_back(d);
    }
    if (closed && n >= 3) {
        directions.push_back((vertices[0] - vertices[n - 1]).Normalized());
    }

    // Generate quads for each segment
    for (size_t i = 0; i < n - 1; ++i) {
        const Vec2& p0 = vertices[i];
        const Vec2& p1 = vertices[i + 1];
        Vec2 perp = directions[i].Perpendicular();

        uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());

        // Four corners of the segment
        outVertices.push_back(p0 + perp * halfWidth);  // top-left
        outVertices.push_back(p0 - perp * halfWidth);  // bottom-left
        outVertices.push_back(p1 + perp * halfWidth);  // top-right
        outVertices.push_back(p1 - perp * halfWidth);  // bottom-right

        // Two triangles
        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 2);

        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 3);
        outIndices.push_back(baseIdx + 2);
    }

    // TODO: Add join geometry
    // TODO: Add cap geometry
}

inline void StrokeExpander::GenerateArc(const Vec2& center, float radius,
                                         const Vec2& start, const Vec2& end,
                                         bool clockwise,
                                         std::vector<Vec2>& output) {
    // Compute angles
    Vec2 startDir = (start - center).Normalized();
    Vec2 endDir = (end - center).Normalized();

    float startAngle = std::atan2(startDir.y, startDir.x);
    float endAngle = std::atan2(endDir.y, endDir.x);

    // Adjust angles for direction
    float angleDiff = endAngle - startAngle;
    if (clockwise) {
        if (angleDiff > 0) angleDiff -= 2.0f * 3.14159265359f;
    } else {
        if (angleDiff < 0) angleDiff += 2.0f * 3.14159265359f;
    }

    // Number of segments based on arc length
    float arcLength = std::abs(angleDiff) * radius;
    int segments = std::max(4, static_cast<int>(arcLength / 2.0f));

    float angleStep = angleDiff / segments;

    for (int i = 1; i <= segments; ++i) {
        float angle = startAngle + angleStep * i;
        output.push_back(center + Vec2(std::cos(angle), std::sin(angle)) * radius);
    }
}

inline void StrokeExpander::GenerateStartCap(const Vec2& point, const Vec2& direction,
                                              std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide) {
    float halfWidth = _style.HalfWidth();
    Vec2 perp = direction.Perpendicular();
    Vec2 left = point + perp * halfWidth;
    Vec2 right = point - perp * halfWidth;

    switch (_style.lineCap) {
        case LineCap::Butt:
            leftSide.push_back(left);
            rightSide.push_back(right);
            break;

        case LineCap::Square:
            // Extend by half width
            leftSide.push_back(left - direction * halfWidth);
            rightSide.push_back(right - direction * halfWidth);
            leftSide.push_back(left);
            rightSide.push_back(right);
            break;

        case LineCap::Round:
            // Generate semicircle
            {
                std::vector<Vec2> arc;
                GenerateArc(point, halfWidth, right, left, false, arc);
                // Add arc to left side (in reverse for proper winding)
                for (auto it = arc.rbegin(); it != arc.rend(); ++it) {
                    leftSide.push_back(*it);
                }
                rightSide.push_back(right);
            }
            break;
    }
}

inline void StrokeExpander::GenerateEndCap(const Vec2& point, const Vec2& direction,
                                            std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide) {
    float halfWidth = _style.HalfWidth();
    Vec2 perp = direction.Perpendicular();
    Vec2 left = point + perp * halfWidth;
    Vec2 right = point - perp * halfWidth;

    switch (_style.lineCap) {
        case LineCap::Butt:
            leftSide.push_back(left);
            rightSide.push_back(right);
            break;

        case LineCap::Square:
            leftSide.push_back(left);
            rightSide.push_back(right);
            leftSide.push_back(left + direction * halfWidth);
            rightSide.push_back(right + direction * halfWidth);
            break;

        case LineCap::Round:
            {
                leftSide.push_back(left);
                std::vector<Vec2> arc;
                GenerateArc(point, halfWidth, left, right, false, arc);
                leftSide.insert(leftSide.end(), arc.begin(), arc.end());
                rightSide.push_back(right);
            }
            break;
    }
}

inline void StrokeExpander::GenerateJoin(const Vec2& point, const Vec2& inDir, const Vec2& outDir,
                                          std::vector<Vec2>& leftSide, std::vector<Vec2>& rightSide) {
    float halfWidth = _style.HalfWidth();
    
    Vec2 inPerp = inDir.Perpendicular();
    Vec2 outPerp = outDir.Perpendicular();

    // Determine if this is a left or right turn
    float cross = Cross(inDir, outDir);
    bool isLeftTurn = cross > 0;

    // Points on the outer and inner edges
    Vec2 inLeft = point + inPerp * halfWidth;
    Vec2 inRight = point - inPerp * halfWidth;
    Vec2 outLeft = point + outPerp * halfWidth;
    Vec2 outRight = point - outPerp * halfWidth;

    // For very shallow angles, just add the points
    if (std::abs(cross) < 1e-4f) {
        leftSide.push_back(outLeft);
        rightSide.push_back(outRight);
        return;
    }

    switch (_style.lineJoin) {
        case LineJoin::Bevel:
            if (isLeftTurn) {
                // Left turn: outer is on right, inner is on left
                // Inner side: intersect
                Vec2 intersection;
                float t1, t2;
                if (Geometry::LineIntersection(inLeft - inDir, inLeft,
                                               outLeft, outLeft + outDir,
                                               intersection, t1, t2)) {
                    leftSide.push_back(intersection);
                } else {
                    leftSide.push_back(inLeft);
                    leftSide.push_back(outLeft);
                }
                // Outer side: bevel
                rightSide.push_back(inRight);
                rightSide.push_back(outRight);
            } else {
                // Right turn: outer is on left, inner is on right
                leftSide.push_back(inLeft);
                leftSide.push_back(outLeft);
                // Inner side: intersect
                Vec2 intersection;
                float t1, t2;
                if (Geometry::LineIntersection(inRight - inDir, inRight,
                                               outRight, outRight + outDir,
                                               intersection, t1, t2)) {
                    rightSide.push_back(intersection);
                } else {
                    rightSide.push_back(inRight);
                    rightSide.push_back(outRight);
                }
            }
            break;

        case LineJoin::Round:
            if (isLeftTurn) {
                // Inner: intersect
                Vec2 intersection;
                float t1, t2;
                if (Geometry::LineIntersection(inLeft - inDir, inLeft,
                                               outLeft, outLeft + outDir,
                                               intersection, t1, t2)) {
                    leftSide.push_back(intersection);
                }
                // Outer: arc
                rightSide.push_back(inRight);
                std::vector<Vec2> arc;
                GenerateArc(point, halfWidth, inRight, outRight, true, arc);
                rightSide.insert(rightSide.end(), arc.begin(), arc.end());
            } else {
                // Outer: arc
                leftSide.push_back(inLeft);
                std::vector<Vec2> arc;
                GenerateArc(point, halfWidth, inLeft, outLeft, false, arc);
                leftSide.insert(leftSide.end(), arc.begin(), arc.end());
                // Inner: intersect
                Vec2 intersection;
                float t1, t2;
                if (Geometry::LineIntersection(inRight - inDir, inRight,
                                               outRight, outRight + outDir,
                                               intersection, t1, t2)) {
                    rightSide.push_back(intersection);
                }
            }
            break;

        case LineJoin::Miter:
            {
                // Compute miter length
                Vec2 miterDir = (inPerp + outPerp).Normalized();
                float cosHalfAngle = Dot(miterDir, inPerp);
                if (std::abs(cosHalfAngle) < 0.01f) cosHalfAngle = 0.01f;
                float miterLength = halfWidth / cosHalfAngle;
                float miterRatio = miterLength / halfWidth;

                if (miterRatio > _style.miterLimit) {
                    // Exceeds miter limit - fall back to bevel
                    if (isLeftTurn) {
                        Vec2 intersection;
                        float t1, t2;
                        if (Geometry::LineIntersection(inLeft - inDir, inLeft,
                                                       outLeft, outLeft + outDir,
                                                       intersection, t1, t2)) {
                            leftSide.push_back(intersection);
                        }
                        rightSide.push_back(inRight);
                        rightSide.push_back(outRight);
                    } else {
                        leftSide.push_back(inLeft);
                        leftSide.push_back(outLeft);
                        Vec2 intersection;
                        float t1, t2;
                        if (Geometry::LineIntersection(inRight - inDir, inRight,
                                                       outRight, outRight + outDir,
                                                       intersection, t1, t2)) {
                            rightSide.push_back(intersection);
                        }
                    }
                } else {
                    // Normal miter
                    if (isLeftTurn) {
                        Vec2 intersection;
                        float t1, t2;
                        if (Geometry::LineIntersection(inLeft - inDir, inLeft,
                                                       outLeft, outLeft + outDir,
                                                       intersection, t1, t2)) {
                            leftSide.push_back(intersection);
                        }
                        Vec2 miterPoint = point - miterDir * miterLength;
                        rightSide.push_back(miterPoint);
                    } else {
                        Vec2 miterPoint = point + miterDir * miterLength;
                        leftSide.push_back(miterPoint);
                        Vec2 intersection;
                        float t1, t2;
                        if (Geometry::LineIntersection(inRight - inDir, inRight,
                                                       outRight, outRight + outDir,
                                                       intersection, t1, t2)) {
                            rightSide.push_back(intersection);
                        }
                    }
                }
            }
            break;
    }
}

inline Vec2 StrokeExpander::OffsetPoint(const Vec2& point, const Vec2& direction, float offset) const {
    return point + direction.Perpendicular() * offset;
}

//=============================================================================
// Dash Pattern Implementation
//=============================================================================
inline std::vector<std::vector<Vec2>> StrokeExpander::ApplyDashPattern(
    const std::vector<Vec2>& vertices, bool closed) {
    
    std::vector<std::vector<Vec2>> result;
    
    // If no dash pattern, return the original path as a single segment
    if (_style.dashArray.empty()) {
        result.push_back(vertices);
        return result;
    }
    
    if (vertices.size() < 2) {
        return result;
    }
    
    // Normalize dash array (SVG spec: if odd length, duplicate to make even)
    std::vector<float> dashArray = _style.dashArray;
    if (dashArray.size() % 2 != 0) {
        size_t n = dashArray.size();
        dashArray.resize(n * 2);
        for (size_t i = 0; i < n; ++i) {
            dashArray[n + i] = dashArray[i];
        }
    }
    
    // Calculate total path length
    float totalLength = 0.0f;
    std::vector<float> segmentLengths;
    for (size_t i = 0; i < vertices.size() - 1; ++i) {
        float len = (vertices[i + 1] - vertices[i]).Length();
        segmentLengths.push_back(len);
        totalLength += len;
    }
    if (closed && vertices.size() >= 2) {
        float len = (vertices[0] - vertices.back()).Length();
        segmentLengths.push_back(len);
        totalLength += len;
    }
    
    if (totalLength < 1e-6f) {
        return result;
    }
    
    // Calculate total dash pattern length
    float dashPatternLength = 0.0f;
    for (float d : dashArray) {
        dashPatternLength += d;
    }
    if (dashPatternLength < 1e-6f) {
        result.push_back(vertices);
        return result;
    }
    
    // Apply dash offset
    float dashOffset = std::fmod(_style.dashOffset, dashPatternLength);
    if (dashOffset < 0) dashOffset += dashPatternLength;
    
    // Find starting dash index and position within that dash
    size_t dashIndex = 0;
    float dashRemaining = dashArray[0];
    while (dashOffset > dashRemaining) {
        dashOffset -= dashRemaining;
        dashIndex = (dashIndex + 1) % dashArray.size();
        dashRemaining = dashArray[dashIndex];
    }
    dashRemaining -= dashOffset;
    
    // Track whether we're drawing (even indices) or not (odd indices)
    bool isDrawing = (dashIndex % 2 == 0);
    
    // Walk along the path
    std::vector<Vec2> currentDash;
    size_t segIdx = 0;
    float segProgress = 0.0f;
    
    // Start point
    if (isDrawing) {
        currentDash.push_back(vertices[0]);
    }
    
    size_t numSegments = closed ? segmentLengths.size() : segmentLengths.size();
    float distanceWalked = 0.0f;
    
    while (segIdx < numSegments) {
        float segLen = segmentLengths[segIdx];
        float segRemaining = segLen - segProgress;
        
        const Vec2& segStart = vertices[segIdx];
        const Vec2& segEnd = (segIdx == vertices.size() - 1) ? vertices[0] : vertices[segIdx + 1];
        Vec2 segDir = (segEnd - segStart);
        if (segLen > 1e-6f) {
            segDir = segDir * (1.0f / segLen);
        }
        
        while (segRemaining > 1e-6f) {
            if (dashRemaining <= segRemaining) {
                // Finish this dash segment
                float t = (segProgress + dashRemaining) / segLen;
                Vec2 point = segStart + (segEnd - segStart) * t;
                
                if (isDrawing) {
                    currentDash.push_back(point);
                    if (currentDash.size() >= 2) {
                        result.push_back(currentDash);
                    }
                    currentDash.clear();
                }
                
                segProgress += dashRemaining;
                segRemaining -= dashRemaining;
                distanceWalked += dashRemaining;
                
                // Move to next dash
                dashIndex = (dashIndex + 1) % dashArray.size();
                dashRemaining = dashArray[dashIndex];
                isDrawing = !isDrawing;
                
                if (isDrawing && segRemaining > 1e-6f) {
                    float t2 = segProgress / segLen;
                    currentDash.push_back(segStart + (segEnd - segStart) * t2);
                }
            } else {
                // Dash extends beyond this segment
                dashRemaining -= segRemaining;
                distanceWalked += segRemaining;
                
                if (isDrawing) {
                    currentDash.push_back(segEnd);
                }
                
                segProgress = 0.0f;
                segRemaining = 0.0f;
                segIdx++;
                
                if (segIdx < numSegments) {
                    if (isDrawing) {
                        // Continue the dash in next segment
                        const Vec2& nextStart = vertices[segIdx];
                        // Don't add nextStart if it's same as last point
                        if (currentDash.empty() || (currentDash.back() - nextStart).LengthSquared() > 1e-6f) {
                            // currentDash already has segEnd which equals nextStart
                        }
                    }
                }
            }
        }
        
        if (segRemaining <= 1e-6f) {
            segProgress = 0.0f;
            segIdx++;
        }
    }
    
    // Finish any remaining dash
    if (isDrawing && currentDash.size() >= 2) {
        result.push_back(currentDash);
    }
    
    return result;
}

} // namespace VCX::Labs::SVG
