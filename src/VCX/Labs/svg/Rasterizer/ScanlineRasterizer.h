#pragma once

#include "Core/Math2D.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace VCX::Labs::SVG {

//=============================================================================
// Fill Rule Enum (SVG spec)
//=============================================================================
enum class FillRule {
    NonZero,    // Default: count direction of edge crossings
    EvenOdd     // Alternate: odd number of crossings = inside
};

//=============================================================================
// Edge structure for scanline rasterizer
//=============================================================================
struct Edge {
    float yMin;         // Top y coordinate (smaller)
    float yMax;         // Bottom y coordinate (larger)
    float xAtYMin;      // X coordinate at yMin
    float dxPerY;       // Change in x per unit y (inverse slope)
    int direction;      // +1 for downward edge, -1 for upward (for winding)

    Edge() = default;
    Edge(const Vec2& p0, const Vec2& p1) {
        // Ensure p0 is the top point (smaller y)
        if (p0.y <= p1.y) {
            yMin = p0.y;
            yMax = p1.y;
            xAtYMin = p0.x;
            direction = 1;  // Downward
        } else {
            yMin = p1.y;
            yMax = p0.y;
            xAtYMin = p1.x;
            direction = -1;  // Upward
        }
        
        float dy = yMax - yMin;
        if (dy > 1e-6f) {
            if (p0.y <= p1.y) {
                dxPerY = (p1.x - p0.x) / dy;
            } else {
                dxPerY = (p0.x - p1.x) / dy;
            }
        } else {
            dxPerY = 0;
        }
    }

    // Get x at given y
    float XAt(float y) const {
        return xAtYMin + (y - yMin) * dxPerY;
    }

    // Check if scanline y intersects this edge
    bool IntersectsScanline(float y) const {
        return y >= yMin && y < yMax;
    }
};

//=============================================================================
// Active Edge entry (for scanline processing)
//=============================================================================
struct ActiveEdge {
    float x;            // Current x position
    float dxPerY;       // X increment per scanline
    float yMax;         // Where this edge ends
    int direction;      // Winding direction
    int index;          // Original edge index

    bool operator<(const ActiveEdge& other) const {
        return x < other.x;
    }
};

//=============================================================================
// Scanline Rasterizer with Anti-Aliasing
//=============================================================================
class ScanlineRasterizer {
public:
    // AA sampling modes
    enum class AAMode {
        None,           // No anti-aliasing
        Coverage4x,     // 4x MSAA-style coverage
        Coverage8x,     // 8x MSAA-style coverage
        Coverage16x,    // 16x MSAA-style coverage
        Analytical      // Analytical edge coverage
    };

    ScanlineRasterizer() : _aaMode(AAMode::Coverage4x), _fillRule(FillRule::NonZero) {}

    void SetAAMode(AAMode mode) { _aaMode = mode; }
    void SetFillRule(FillRule rule) { _fillRule = rule; }

    // Rasterize a polygon to coverage values
    // Returns: for each pixel, a coverage value 0.0 - 1.0
    void Rasterize(const std::vector<Vec2>& polygon,
                   int width, int height,
                   std::vector<float>& coverage);
    
    // Rasterize multiple sub-paths (for complex paths with holes)
    void RasterizeSubPaths(const std::vector<std::vector<Vec2>>& subPaths,
                           int width, int height,
                           std::vector<float>& coverage);

    // Rasterize with bounding box optimization
    void RasterizeClipped(const std::vector<Vec2>& polygon,
                          const BBox& clipRect,
                          int width, int height,
                          std::vector<float>& coverage);

private:
    AAMode _aaMode;
    FillRule _fillRule;

    // Build edge table from polygon
    std::vector<Edge> BuildEdgeTable(const std::vector<Vec2>& polygon);
    
    // Build edge table from multiple sub-paths
    std::vector<Edge> BuildEdgeTableFromSubPaths(const std::vector<std::vector<Vec2>>& subPaths);

    // Check if point is inside using fill rule
    bool IsInside(int windingNumber) const;

    // Coverage calculation methods
    float ComputePixelCoverage(int px, int py, const std::vector<Edge>& edges);
    float ComputeAnalyticalCoverage(int px, int py, const std::vector<Edge>& edges);
    
    // Sample points for MSAA
    static const std::vector<Vec2>& GetSamplePattern(AAMode mode);
};

//=============================================================================
// Implementation
//=============================================================================

inline std::vector<Edge> ScanlineRasterizer::BuildEdgeTable(const std::vector<Vec2>& polygon) {
    std::vector<Edge> edges;
    size_t n = polygon.size();
    if (n < 2) return edges;

    edges.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        const Vec2& p0 = polygon[i];
        const Vec2& p1 = polygon[j];
        
        // Skip horizontal edges
        if (std::abs(p0.y - p1.y) > 1e-6f) {
            edges.emplace_back(p0, p1);
        }
    }

    return edges;
}

inline bool ScanlineRasterizer::IsInside(int windingNumber) const {
    switch (_fillRule) {
        case FillRule::NonZero:
            return windingNumber != 0;
        case FillRule::EvenOdd:
            return (windingNumber & 1) != 0;
    }
    return false;
}

inline const std::vector<Vec2>& ScanlineRasterizer::GetSamplePattern(AAMode mode) {
    // 4x4 rotated grid pattern (good for edges at various angles)
    static const std::vector<Vec2> pattern4x = {
        {0.375f, 0.125f},
        {0.875f, 0.375f},
        {0.125f, 0.625f},
        {0.625f, 0.875f}
    };

    // 8x pattern
    static const std::vector<Vec2> pattern8x = {
        {0.5625f, 0.3125f},
        {0.4375f, 0.6875f},
        {0.8125f, 0.5625f},
        {0.3125f, 0.1875f},
        {0.1875f, 0.8125f},
        {0.0625f, 0.4375f},
        {0.6875f, 0.9375f},
        {0.9375f, 0.0625f}
    };

    // 16x pattern (4x4 jittered grid)
    static const std::vector<Vec2> pattern16x = {
        {0.0625f, 0.0625f}, {0.1875f, 0.3125f}, {0.3125f, 0.1875f}, {0.4375f, 0.4375f},
        {0.5625f, 0.0625f}, {0.6875f, 0.3125f}, {0.8125f, 0.1875f}, {0.9375f, 0.4375f},
        {0.0625f, 0.5625f}, {0.1875f, 0.8125f}, {0.3125f, 0.6875f}, {0.4375f, 0.9375f},
        {0.5625f, 0.5625f}, {0.6875f, 0.8125f}, {0.8125f, 0.6875f}, {0.9375f, 0.9375f}
    };

    static const std::vector<Vec2> pattern1x = {{0.5f, 0.5f}};

    switch (mode) {
        case AAMode::Coverage4x: return pattern4x;
        case AAMode::Coverage8x: return pattern8x;
        case AAMode::Coverage16x: return pattern16x;
        default: return pattern1x;
    }
}

inline float ScanlineRasterizer::ComputePixelCoverage(int px, int py, const std::vector<Edge>& edges) {
    if (_aaMode == AAMode::None) {
        // Single sample at center
        float x = px + 0.5f;
        float y = py + 0.5f;
        int winding = 0;
        for (const auto& edge : edges) {
            if (edge.IntersectsScanline(y)) {
                float ex = edge.XAt(y);
                if (ex > x) {
                    winding += edge.direction;
                }
            }
        }
        return IsInside(winding) ? 1.0f : 0.0f;
    }

    if (_aaMode == AAMode::Analytical) {
        return ComputeAnalyticalCoverage(px, py, edges);
    }

    // Multi-sample coverage
    const auto& samples = GetSamplePattern(_aaMode);
    int hitCount = 0;

    for (const auto& sample : samples) {
        float x = px + sample.x;
        float y = py + sample.y;
        
        int winding = 0;
        for (const auto& edge : edges) {
            if (edge.IntersectsScanline(y)) {
                float ex = edge.XAt(y);
                if (ex > x) {
                    winding += edge.direction;
                }
            }
        }
        
        if (IsInside(winding)) {
            hitCount++;
        }
    }

    return static_cast<float>(hitCount) / samples.size();
}

inline float ScanlineRasterizer::ComputeAnalyticalCoverage(int px, int py, const std::vector<Edge>& edges) {
    // Analytical coverage: compute exact intersection area
    // This is a simplified version that works well for most cases
    
    float left = static_cast<float>(px);
    float right = static_cast<float>(px + 1);
    float top = static_cast<float>(py);
    float bottom = static_cast<float>(py + 1);
    
    // For each edge that intersects this pixel, compute contribution
    float coverage = 0.0f;
    bool anyInside = false;
    
    // Sample at multiple y levels within the pixel
    const int ySteps = 8;
    for (int yi = 0; yi < ySteps; ++yi) {
        float y = top + (yi + 0.5f) / ySteps;
        
        std::vector<std::pair<float, int>> intersections;
        
        for (const auto& edge : edges) {
            if (edge.IntersectsScanline(y)) {
                float x = edge.XAt(y);
                if (x >= left - 0.5f && x <= right + 0.5f) {
                    intersections.push_back({x, edge.direction});
                }
            }
        }
        
        // Sort intersections by x
        std::sort(intersections.begin(), intersections.end());
        
        // Calculate coverage for this scanline within pixel
        int winding = 0;
        float lastX = left;
        float rowCoverage = 0.0f;
        
        for (const auto& [x, dir] : intersections) {
            float clampedX = std::clamp(x, left, right);
            
            if (IsInside(winding)) {
                rowCoverage += clampedX - lastX;
                anyInside = true;
            }
            
            winding += dir;
            lastX = clampedX;
        }
        
        // Handle remaining span
        if (IsInside(winding)) {
            rowCoverage += right - lastX;
            anyInside = true;
        }
        
        coverage += rowCoverage;
    }
    
    // If no edges intersect but we might be fully inside, check center
    if (!anyInside) {
        float cx = px + 0.5f;
        float cy = py + 0.5f;
        int winding = 0;
        for (const auto& edge : edges) {
            if (edge.IntersectsScanline(cy)) {
                float ex = edge.XAt(cy);
                if (ex > cx) {
                    winding += edge.direction;
                }
            }
        }
        return IsInside(winding) ? 1.0f : 0.0f;
    }
    
    // Normalize by pixel area and number of samples
    return std::clamp(coverage / ySteps, 0.0f, 1.0f);
}

inline void ScanlineRasterizer::Rasterize(const std::vector<Vec2>& polygon,
                                           int width, int height,
                                           std::vector<float>& coverage) {
    coverage.resize(width * height, 0.0f);
    
    if (polygon.size() < 3) return;
    
    // Build edge table
    std::vector<Edge> edges = BuildEdgeTable(polygon);
    if (edges.empty()) return;
    
    // Compute bounding box
    BBox bbox = Geometry::ComputeBBox(polygon);
    
    int yMin = std::max(0, static_cast<int>(std::floor(bbox.min.y)));
    int yMax = std::min(height - 1, static_cast<int>(std::ceil(bbox.max.y)));
    int xMin = std::max(0, static_cast<int>(std::floor(bbox.min.x)));
    int xMax = std::min(width - 1, static_cast<int>(std::ceil(bbox.max.x)));
    
    // Process each scanline
    for (int y = yMin; y <= yMax; ++y) {
        // Collect edges that intersect this scanline range
        std::vector<const Edge*> activeEdges;
        for (const auto& edge : edges) {
            if (edge.yMin <= y + 1 && edge.yMax >= y) {
                activeEdges.push_back(&edge);
            }
        }
        
        if (activeEdges.empty()) continue;
        
        // Compute coverage for each pixel in scanline
        for (int x = xMin; x <= xMax; ++x) {
            float cov = ComputePixelCoverage(x, y, edges);
            if (cov > 0) {
                coverage[y * width + x] = cov;
            }
        }
    }
}

inline std::vector<Edge> ScanlineRasterizer::BuildEdgeTableFromSubPaths(const std::vector<std::vector<Vec2>>& subPaths) {
    std::vector<Edge> edges;
    
    for (const auto& polygon : subPaths) {
        size_t n = polygon.size();
        if (n < 2) continue;
        
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            const Vec2& p0 = polygon[i];
            const Vec2& p1 = polygon[j];
            
            // Skip horizontal edges
            if (std::abs(p0.y - p1.y) > 1e-6f) {
                edges.emplace_back(p0, p1);
            }
        }
    }
    
    return edges;
}

inline void ScanlineRasterizer::RasterizeSubPaths(const std::vector<std::vector<Vec2>>& subPaths,
                                                   int width, int height,
                                                   std::vector<float>& coverage) {
    coverage.resize(width * height, 0.0f);
    
    if (subPaths.empty()) return;
    
    // Build edge table from all sub-paths
    std::vector<Edge> edges = BuildEdgeTableFromSubPaths(subPaths);
    if (edges.empty()) return;
    
    // Compute combined bounding box
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    
    for (const auto& polygon : subPaths) {
        for (const auto& p : polygon) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    
    int yMin = std::max(0, static_cast<int>(std::floor(minY)));
    int yMax = std::min(height - 1, static_cast<int>(std::ceil(maxY)));
    int xMin = std::max(0, static_cast<int>(std::floor(minX)));
    int xMax = std::min(width - 1, static_cast<int>(std::ceil(maxX)));
    
    // Process each scanline
    for (int y = yMin; y <= yMax; ++y) {
        // Collect edges that intersect this scanline range
        std::vector<const Edge*> activeEdges;
        for (const auto& edge : edges) {
            if (edge.yMin <= y + 1 && edge.yMax >= y) {
                activeEdges.push_back(&edge);
            }
        }
        
        if (activeEdges.empty()) continue;
        
        // Compute coverage for each pixel in scanline
        for (int x = xMin; x <= xMax; ++x) {
            float cov = ComputePixelCoverage(x, y, edges);
            if (cov > 0) {
                coverage[y * width + x] = cov;
            }
        }
    }
}

inline void ScanlineRasterizer::RasterizeClipped(const std::vector<Vec2>& polygon,
                                                  const BBox& clipRect,
                                                  int width, int height,
                                                  std::vector<float>& coverage) {
    // Same as Rasterize but clipped to given rectangle
    coverage.resize(width * height, 0.0f);
    
    if (polygon.size() < 3) return;
    
    std::vector<Edge> edges = BuildEdgeTable(polygon);
    if (edges.empty()) return;
    
    BBox bbox = Geometry::ComputeBBox(polygon);
    bbox = bbox.Intersection(clipRect);
    
    if (bbox.IsEmpty()) return;
    
    int yMin = std::max(0, static_cast<int>(std::floor(bbox.min.y)));
    int yMax = std::min(height - 1, static_cast<int>(std::ceil(bbox.max.y)));
    int xMin = std::max(0, static_cast<int>(std::floor(bbox.min.x)));
    int xMax = std::min(width - 1, static_cast<int>(std::ceil(bbox.max.x)));
    
    for (int y = yMin; y <= yMax; ++y) {
        for (int x = xMin; x <= xMax; ++x) {
            float cov = ComputePixelCoverage(x, y, edges);
            if (cov > 0) {
                coverage[y * width + x] = cov;
            }
        }
    }
}

} // namespace VCX::Labs::SVG
