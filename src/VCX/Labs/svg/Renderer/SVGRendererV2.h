#pragma once

#include "SVG.h"
#include "Core/Math2D.h"
#include "Core/Bezier.h"
#include "Geometry/StrokeExpander.h"
#include "Rasterizer/ScanlineRasterizer.h"
#include "Paint/Gradient.h"
#include "Labs/Common/ImageRGB.h"
#include <memory>
#include <vector>

namespace VCX::Labs::SVG {

//=============================================================================
// Render Context - Holds state during rendering
//=============================================================================
struct RenderContext {
    Common::ImageRGB* targetImage = nullptr;
    int width = 0;
    int height = 0;
    TransformStack transformStack;
    float flatnessTolerance = 0.5f;  // Bézier tessellation tolerance
    bool enableAA = true;
    ScanlineRasterizer::AAMode aaMode = ScanlineRasterizer::AAMode::Coverage4x;
};

// 子路径结构（包含顶点和是否闭合）
struct SubPathV2 {
    std::vector<Vec2> points;
    bool closed = false;
};

//=============================================================================
// SVGRendererV2 - High-quality SVG Renderer
//=============================================================================
class SVGRendererV2 {
public:
    SVGRendererV2();
    ~SVGRendererV2();

    // Main rendering entry point
    Common::ImageRGB RenderSVG(const SVGDocument& document,
                               std::uint32_t width,
                               std::uint32_t height);

    // Settings
    void SetBackgroundColor(const glm::vec4& color) { _backgroundColor = color; }
    void SetAntiAliasing(bool enabled) { _enableAA = enabled; }
    void SetAAMode(ScanlineRasterizer::AAMode mode) { _aaMode = mode; }
    void SetFlatnessTolerance(float tolerance) { _flatnessTolerance = tolerance; }

private:
    glm::vec4 _backgroundColor;
    bool _enableAA;
    ScanlineRasterizer::AAMode _aaMode;
    float _flatnessTolerance;
    
    ScanlineRasterizer _rasterizer;
    StrokeExpander _strokeExpander;

    // Element rendering
    void RenderElement(const SVGElement& element, RenderContext& ctx);
    void RenderPath(const SVGPath& path, RenderContext& ctx);
    void RenderCircle(const SVGCircle& circle, RenderContext& ctx);
    void RenderEllipse(const SVGEllipse& ellipse, RenderContext& ctx);
    void RenderRect(const SVGRect& rect, RenderContext& ctx);
    void RenderLine(const SVGLine& line, RenderContext& ctx);
    void RenderText(const SVGText& text, RenderContext& ctx);

    // Path processing
    std::vector<Vec2> TessellatePath(const SVGPath& path, const Matrix3x3& transform);
    std::vector<std::vector<Vec2>> TessellatePathSubPaths(const SVGPath& path, const Matrix3x3& transform);
    std::vector<SubPathV2> TessellatePathSubPathsEx(const SVGPath& path, const Matrix3x3& transform);
    std::vector<Vec2> GenerateCircleVertices(const Vec2& center, float radius, int segments = 64);
    std::vector<Vec2> GenerateEllipseVertices(const Vec2& center, float rx, float ry, int segments = 64);
    std::vector<Vec2> GenerateRoundedRectVertices(const Vec2& pos, float w, float h, float rx, float ry);

    // Fill and stroke
    void FillPolygon(const std::vector<Vec2>& polygon, const glm::vec4& color, 
                     FillRule fillRule, RenderContext& ctx);
    void FillSubPaths(const std::vector<std::vector<Vec2>>& subPaths, const glm::vec4& color,
                      FillRule fillRule, RenderContext& ctx);
    void FillSubPathsEx(const std::vector<SubPathV2>& subPaths, const glm::vec4& color,
                        FillRule fillRule, RenderContext& ctx);
    void FillPolygonWithPaint(const std::vector<Vec2>& polygon, const Paint& paint,
                              FillRule fillRule, RenderContext& ctx);
    void StrokePath(const std::vector<Vec2>& vertices, bool closed,
                    const glm::vec4& color, const StrokeStyle& style, RenderContext& ctx);
    void StrokeSubPaths(const std::vector<std::vector<Vec2>>& subPaths,
                        const glm::vec4& color, const StrokeStyle& style, RenderContext& ctx);
    void StrokeSubPathsEx(const std::vector<SubPathV2>& subPaths,
                          const glm::vec4& color, const StrokeStyle& style, RenderContext& ctx);

    // Pixel operations
    void BlendPixel(Common::ImageRGB& image, int x, int y, 
                    const glm::vec4& color, float coverage);

    // Color/style extraction
    glm::vec4 GetFillColor(const SVGStyle& style);
    glm::vec4 GetStrokeColor(const SVGStyle& style);
    StrokeStyle GetStrokeStyle(const SVGStyle& style);
    FillRule GetFillRule(const SVGStyle& style);

    // Transform helpers
    Matrix3x3 ConvertTransform(const Transform2D& t);
};

//=============================================================================
// Implementation
//=============================================================================

inline SVGRendererV2::SVGRendererV2() 
    : _backgroundColor(1, 1, 1, 1)
    , _enableAA(true)
    , _aaMode(ScanlineRasterizer::AAMode::Coverage4x)
    , _flatnessTolerance(0.5f) {
}

inline SVGRendererV2::~SVGRendererV2() = default;

inline Common::ImageRGB SVGRendererV2::RenderSVG(const SVGDocument& document,
                                                  std::uint32_t width,
                                                  std::uint32_t height) {
    Common::ImageRGB image(width, height);

    // Initialize background
    glm::vec3 bgColor(_backgroundColor.r, _backgroundColor.g, _backgroundColor.b);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            image.At(x, y) = bgColor;
        }
    }

    // Setup render context
    RenderContext ctx;
    ctx.targetImage = &image;
    ctx.width = static_cast<int>(width);
    ctx.height = static_cast<int>(height);
    ctx.flatnessTolerance = _flatnessTolerance;
    ctx.enableAA = _enableAA;
    ctx.aaMode = _aaMode;

    // Apply viewBox transform if present
    float vbX, vbY, vbW, vbH;
    if (document.ParseViewBox(vbX, vbY, vbW, vbH)) {
        float scaleX = width / vbW;
        float scaleY = height / vbH;
        ctx.transformStack.Translate(-vbX * scaleX, -vbY * scaleY);
        ctx.transformStack.Scale(scaleX, scaleY);
    }

    // Render all elements
    for (const auto& element : document.elements) {
        RenderElement(element, ctx);
    }

    return image;
}

inline void SVGRendererV2::RenderElement(const SVGElement& element, RenderContext& ctx) {
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(element.transform));

    switch (element.type) {
        case SVGElement::Type::Path:
            RenderPath(element.path, ctx);
            break;
        case SVGElement::Type::Circle:
            RenderCircle(element.circle, ctx);
            break;
        case SVGElement::Type::Ellipse:
            RenderEllipse(element.ellipse, ctx);
            break;
        case SVGElement::Type::Rect:
            RenderRect(element.rect, ctx);
            break;
        case SVGElement::Type::Line:
            RenderLine(element.line, ctx);
            break;
        case SVGElement::Type::Text:
            RenderText(element.text, ctx);
            break;
        case SVGElement::Type::Group:
            for (const auto& child : element.children) {
                RenderElement(child, ctx);
            }
            break;
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderPath(const SVGPath& path, RenderContext& ctx) {
    // Apply path's own transform
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(path.transform));

    // Tessellate path into sub-paths with closed info
    std::vector<SubPathV2> subPaths = TessellatePathSubPathsEx(path, ctx.transformStack.Current());

    if (subPaths.empty()) {
        ctx.transformStack.Pop();
        return;
    }

    // Get fill color and render fill
    glm::vec4 fillColor = GetFillColor(path.style);
    if (fillColor.a > 0) {
        FillSubPathsEx(subPaths, fillColor, GetFillRule(path.style), ctx);
    }

    // Get stroke color and render stroke
    glm::vec4 strokeColor = GetStrokeColor(path.style);
    if (strokeColor.a > 0) {
        StrokeStyle strokeStyle = GetStrokeStyle(path.style);
        StrokeSubPathsEx(subPaths, strokeColor, strokeStyle, ctx);
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderCircle(const SVGCircle& circle, RenderContext& ctx) {
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(circle.transform));

    Vec2 center(circle.center.x, circle.center.y);
    center = ctx.transformStack.TransformPoint(center);
    float scale = ctx.transformStack.Current().GetScaleFactor();
    float radius = circle.radius * scale;

    std::vector<Vec2> vertices = GenerateCircleVertices(center, radius);

    glm::vec4 fillColor = GetFillColor(circle.style);
    if (fillColor.a > 0) {
        FillPolygon(vertices, fillColor, FillRule::NonZero, ctx);
    }

    glm::vec4 strokeColor = GetStrokeColor(circle.style);
    if (strokeColor.a > 0) {
        StrokeStyle strokeStyle = GetStrokeStyle(circle.style);
        strokeStyle.width *= scale;
        StrokePath(vertices, true, strokeColor, strokeStyle, ctx);
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderEllipse(const SVGEllipse& ellipse, RenderContext& ctx) {
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(ellipse.transform));

    Vec2 center(ellipse.center.x, ellipse.center.y);
    center = ctx.transformStack.TransformPoint(center);
    float scale = ctx.transformStack.Current().GetScaleFactor();

    std::vector<Vec2> vertices = GenerateEllipseVertices(center, 
                                                          ellipse.rx * scale, 
                                                          ellipse.ry * scale);

    glm::vec4 fillColor = GetFillColor(ellipse.style);
    if (fillColor.a > 0) {
        FillPolygon(vertices, fillColor, FillRule::NonZero, ctx);
    }

    glm::vec4 strokeColor = GetStrokeColor(ellipse.style);
    if (strokeColor.a > 0) {
        StrokeStyle strokeStyle = GetStrokeStyle(ellipse.style);
        strokeStyle.width *= scale;
        StrokePath(vertices, true, strokeColor, strokeStyle, ctx);
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderRect(const SVGRect& rect, RenderContext& ctx) {
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(rect.transform));

    std::vector<Vec2> vertices;
    
    if (rect.rx > 0 || rect.ry > 0) {
        // Rounded rectangle
        Vec2 pos(rect.position.x, rect.position.y);
        vertices = GenerateRoundedRectVertices(pos, rect.width, rect.height, rect.rx, rect.ry);
    } else {
        // Simple rectangle
        float x = rect.position.x;
        float y = rect.position.y;
        float w = rect.width;
        float h = rect.height;
        
        vertices.push_back(Vec2(x, y));
        vertices.push_back(Vec2(x + w, y));
        vertices.push_back(Vec2(x + w, y + h));
        vertices.push_back(Vec2(x, y + h));
    }

    // Transform vertices
    for (auto& v : vertices) {
        v = ctx.transformStack.TransformPoint(v);
    }

    glm::vec4 fillColor = GetFillColor(rect.style);
    if (fillColor.a > 0) {
        FillPolygon(vertices, fillColor, FillRule::NonZero, ctx);
    }

    glm::vec4 strokeColor = GetStrokeColor(rect.style);
    if (strokeColor.a > 0) {
        StrokeStyle strokeStyle = GetStrokeStyle(rect.style);
        float scale = ctx.transformStack.Current().GetScaleFactor();
        strokeStyle.width *= scale;
        StrokePath(vertices, true, strokeColor, strokeStyle, ctx);
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderLine(const SVGLine& line, RenderContext& ctx) {
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(line.transform));

    Vec2 start(line.start.x, line.start.y);
    Vec2 end(line.end.x, line.end.y);
    
    start = ctx.transformStack.TransformPoint(start);
    end = ctx.transformStack.TransformPoint(end);

    std::vector<Vec2> vertices = {start, end};

    glm::vec4 strokeColor = GetStrokeColor(line.style);
    if (strokeColor.a > 0) {
        StrokeStyle strokeStyle = GetStrokeStyle(line.style);
        float scale = ctx.transformStack.Current().GetScaleFactor();
        strokeStyle.width *= scale;
        StrokePath(vertices, false, strokeColor, strokeStyle, ctx);
    }

    ctx.transformStack.Pop();
}

inline void SVGRendererV2::RenderText(const SVGText& text, RenderContext& ctx) {
    // TODO: Implement proper text rendering with FreeType
    // For now, use basic text rendering from original renderer
    // This is a placeholder that draws text position as a marker
    
    ctx.transformStack.Push();
    ctx.transformStack.Multiply(ConvertTransform(text.transform));

    Vec2 pos(text.position.x, text.position.y);
    pos = ctx.transformStack.TransformPoint(pos);

    // Draw a small circle at text position as placeholder
    glm::vec4 fillColor = GetFillColor(text.style);
    if (fillColor.a > 0) {
        std::vector<Vec2> marker = GenerateCircleVertices(pos, 3.0f, 16);
        FillPolygon(marker, fillColor, FillRule::NonZero, ctx);
    }

    ctx.transformStack.Pop();
}

inline std::vector<Vec2> SVGRendererV2::TessellatePath(const SVGPath& path, const Matrix3x3& transform) {
    std::vector<Vec2> vertices;
    Vec2 currentPos(0, 0);
    Vec2 startPos(0, 0);
    Vec2 lastControlPoint(0, 0);

    for (const auto& cmd : path.commands) {
        switch (cmd.type) {
            case PathCommandType::MoveTo: {
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    startPos = target;
                    vertices.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::LineTo: {
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    vertices.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::CurveTo: {
                if (cmd.points.size() >= 3) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    Vec2 p3 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[2].x, cmd.points[2].y)
                        : Vec2(cmd.points[2].x, cmd.points[2].y);

                    // Adaptive tessellation
                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateCubicAdaptive(p0, p1, p2, p3, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        vertices.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p3;
                    lastControlPoint = p2;
                }
                break;
            }

            case PathCommandType::QuadCurveTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);

                    // Adaptive tessellation
                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateQuadraticAdaptive(p0, p1, p2, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        vertices.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p2;
                    lastControlPoint = p1;
                }
                break;
            }

            case PathCommandType::ArcTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    
                    // Arc parameters are in points[0]
                    float rx = std::abs(cmd.points[0].x);
                    float ry = std::abs(cmd.points[0].y);
                    
                    if (rx > 0 && ry > 0) {
                        // Convert arc to cubic beziers
                        std::vector<Vec2> arcControls;
                        // Simplified: assume no rotation, largeArc=false, sweep=true
                        Bezier::ArcToCubics(currentPos, rx, ry, 0, false, true, target, arcControls);
                        
                        // Process cubic segments
                        Vec2 cp0 = currentPos;
                        for (size_t i = 0; i + 2 < arcControls.size(); i += 3) {
                            std::vector<Vec2> curvePoints;
                            Bezier::TessellateCubicAdaptive(cp0, arcControls[i], arcControls[i+1], 
                                                            arcControls[i+2], _flatnessTolerance, curvePoints);
                            for (const auto& p : curvePoints) {
                                vertices.push_back(transform.TransformPoint(p));
                            }
                            cp0 = arcControls[i+2];
                        }
                    }
                    
                    vertices.push_back(transform.TransformPoint(target));
                    currentPos = target;
                }
                break;
            }

            case PathCommandType::ClosePath: {
                currentPos = startPos;
                if (!vertices.empty() && DistanceSquared(vertices.back(), transform.TransformPoint(startPos)) > 1e-4f) {
                    vertices.push_back(transform.TransformPoint(startPos));
                }
                break;
            }
        }
    }

    return vertices;
}

inline std::vector<std::vector<Vec2>> SVGRendererV2::TessellatePathSubPaths(const SVGPath& path, const Matrix3x3& transform) {
    std::vector<std::vector<Vec2>> subPaths;
    std::vector<Vec2> currentPath;
    Vec2 currentPos(0, 0);
    Vec2 startPos(0, 0);
    Vec2 lastControlPoint(0, 0);

    for (const auto& cmd : path.commands) {
        switch (cmd.type) {
            case PathCommandType::MoveTo: {
                // 新的MoveTo开始一个新的子路径
                if (!currentPath.empty() && currentPath.size() >= 2) {
                    subPaths.push_back(std::move(currentPath));
                    currentPath.clear();
                }
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    startPos = target;
                    currentPath.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::LineTo: {
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    currentPath.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::CurveTo: {
                if (cmd.points.size() >= 3) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    Vec2 p3 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[2].x, cmd.points[2].y)
                        : Vec2(cmd.points[2].x, cmd.points[2].y);

                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateCubicAdaptive(p0, p1, p2, p3, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        currentPath.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p3;
                    lastControlPoint = p2;
                }
                break;
            }

            case PathCommandType::QuadCurveTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);

                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateQuadraticAdaptive(p0, p1, p2, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        currentPath.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p2;
                    lastControlPoint = p1;
                }
                break;
            }

            case PathCommandType::ArcTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    
                    float rx = std::abs(cmd.points[0].x);
                    float ry = std::abs(cmd.points[0].y);
                    
                    if (rx > 0 && ry > 0) {
                        std::vector<Vec2> arcControls;
                        Bezier::ArcToCubics(currentPos, rx, ry, 0, false, true, target, arcControls);
                        
                        Vec2 cp0 = currentPos;
                        for (size_t i = 0; i + 2 < arcControls.size(); i += 3) {
                            std::vector<Vec2> curvePoints;
                            Bezier::TessellateCubicAdaptive(cp0, arcControls[i], arcControls[i+1], 
                                                            arcControls[i+2], _flatnessTolerance, curvePoints);
                            for (const auto& p : curvePoints) {
                                currentPath.push_back(transform.TransformPoint(p));
                            }
                            cp0 = arcControls[i+2];
                        }
                    }
                    
                    currentPath.push_back(transform.TransformPoint(target));
                    currentPos = target;
                }
                break;
            }

            case PathCommandType::ClosePath: {
                currentPos = startPos;
                if (!currentPath.empty() && DistanceSquared(currentPath.back(), transform.TransformPoint(startPos)) > 1e-4f) {
                    currentPath.push_back(transform.TransformPoint(startPos));
                }
                break;
            }
        }
    }

    // 添加最后一个子路径
    if (!currentPath.empty() && currentPath.size() >= 2) {
        subPaths.push_back(std::move(currentPath));
    }

    return subPaths;
}

inline std::vector<SubPathV2> SVGRendererV2::TessellatePathSubPathsEx(const SVGPath& path, const Matrix3x3& transform) {
    std::vector<SubPathV2> subPaths;
    SubPathV2 currentSubPath;
    Vec2 currentPos(0, 0);
    Vec2 startPos(0, 0);
    Vec2 lastControlPoint(0, 0);

    for (const auto& cmd : path.commands) {
        switch (cmd.type) {
            case PathCommandType::MoveTo: {
                if (!currentSubPath.points.empty() && currentSubPath.points.size() >= 2) {
                    subPaths.push_back(std::move(currentSubPath));
                    currentSubPath = SubPathV2();
                }
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    startPos = target;
                    currentSubPath.points.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::LineTo: {
                if (!cmd.points.empty()) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    currentPos = target;
                    currentSubPath.points.push_back(transform.TransformPoint(target));
                }
                break;
            }

            case PathCommandType::CurveTo: {
                if (cmd.points.size() >= 3) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    Vec2 p3 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[2].x, cmd.points[2].y)
                        : Vec2(cmd.points[2].x, cmd.points[2].y);

                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateCubicAdaptive(p0, p1, p2, p3, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        currentSubPath.points.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p3;
                    lastControlPoint = p2;
                }
                break;
            }

            case PathCommandType::QuadCurveTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 p0 = currentPos;
                    Vec2 p1 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[0].x, cmd.points[0].y)
                        : Vec2(cmd.points[0].x, cmd.points[0].y);
                    Vec2 p2 = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);

                    std::vector<Vec2> curvePoints;
                    Bezier::TessellateQuadraticAdaptive(p0, p1, p2, _flatnessTolerance, curvePoints);
                    
                    for (const auto& p : curvePoints) {
                        currentSubPath.points.push_back(transform.TransformPoint(p));
                    }
                    
                    currentPos = p2;
                    lastControlPoint = p1;
                }
                break;
            }

            case PathCommandType::ArcTo: {
                if (cmd.points.size() >= 2) {
                    Vec2 target = cmd.relative 
                        ? currentPos + Vec2(cmd.points[1].x, cmd.points[1].y)
                        : Vec2(cmd.points[1].x, cmd.points[1].y);
                    
                    float rx = std::abs(cmd.points[0].x);
                    float ry = std::abs(cmd.points[0].y);
                    
                    if (rx > 0 && ry > 0) {
                        std::vector<Vec2> arcControls;
                        Bezier::ArcToCubics(currentPos, rx, ry, 0, false, true, target, arcControls);
                        
                        Vec2 cp0 = currentPos;
                        for (size_t i = 0; i + 2 < arcControls.size(); i += 3) {
                            std::vector<Vec2> curvePoints;
                            Bezier::TessellateCubicAdaptive(cp0, arcControls[i], arcControls[i+1], 
                                                            arcControls[i+2], _flatnessTolerance, curvePoints);
                            for (const auto& p : curvePoints) {
                                currentSubPath.points.push_back(transform.TransformPoint(p));
                            }
                            cp0 = arcControls[i+2];
                        }
                    }
                    
                    currentSubPath.points.push_back(transform.TransformPoint(target));
                    currentPos = target;
                }
                break;
            }

            case PathCommandType::ClosePath: {
                currentPos = startPos;
                currentSubPath.closed = true;
                if (!currentSubPath.points.empty() && DistanceSquared(currentSubPath.points.back(), transform.TransformPoint(startPos)) > 1e-4f) {
                    currentSubPath.points.push_back(transform.TransformPoint(startPos));
                }
                break;
            }
        }
    }

    // 添加最后一个子路径
    if (!currentSubPath.points.empty() && currentSubPath.points.size() >= 2) {
        subPaths.push_back(std::move(currentSubPath));
    }

    return subPaths;
}

inline std::vector<Vec2> SVGRendererV2::GenerateCircleVertices(const Vec2& center, float radius, int segments) {
    std::vector<Vec2> vertices;
    vertices.reserve(segments);
    
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * 3.14159265359f * i / segments;
        vertices.push_back(center + Vec2(std::cos(angle), std::sin(angle)) * radius);
    }
    
    return vertices;
}

inline std::vector<Vec2> SVGRendererV2::GenerateEllipseVertices(const Vec2& center, float rx, float ry, int segments) {
    std::vector<Vec2> vertices;
    vertices.reserve(segments);
    
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * 3.14159265359f * i / segments;
        vertices.push_back(center + Vec2(std::cos(angle) * rx, std::sin(angle) * ry));
    }
    
    return vertices;
}

inline std::vector<Vec2> SVGRendererV2::GenerateRoundedRectVertices(const Vec2& pos, float w, float h, float rx, float ry) {
    std::vector<Vec2> vertices;
    
    // Clamp radii
    rx = std::min(rx, w * 0.5f);
    ry = std::min(ry, h * 0.5f);
    if (ry == 0) ry = rx;
    if (rx == 0) rx = ry;
    
    const int cornerSegments = 8;
    
    // Top-right corner
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = -3.14159265359f * 0.5f * i / cornerSegments;
        vertices.push_back(Vec2(
            pos.x + w - rx + std::cos(angle) * rx,
            pos.y + ry + std::sin(angle) * ry
        ));
    }
    
    // Bottom-right corner
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = 3.14159265359f * 0.5f * i / cornerSegments;
        vertices.push_back(Vec2(
            pos.x + w - rx + std::cos(angle) * rx,
            pos.y + h - ry + std::sin(angle) * ry
        ));
    }
    
    // Bottom-left corner
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = 3.14159265359f * 0.5f + 3.14159265359f * 0.5f * i / cornerSegments;
        vertices.push_back(Vec2(
            pos.x + rx + std::cos(angle) * rx,
            pos.y + h - ry + std::sin(angle) * ry
        ));
    }
    
    // Top-left corner
    for (int i = 0; i <= cornerSegments; ++i) {
        float angle = 3.14159265359f + 3.14159265359f * 0.5f * i / cornerSegments;
        vertices.push_back(Vec2(
            pos.x + rx + std::cos(angle) * rx,
            pos.y + ry + std::sin(angle) * ry
        ));
    }
    
    return vertices;
}

inline void SVGRendererV2::FillPolygon(const std::vector<Vec2>& polygon, 
                                        const glm::vec4& color,
                                        FillRule fillRule, 
                                        RenderContext& ctx) {
    if (polygon.size() < 3 || color.a <= 0) return;

    _rasterizer.SetFillRule(fillRule);
    _rasterizer.SetAAMode(ctx.enableAA ? ctx.aaMode : ScanlineRasterizer::AAMode::None);

    std::vector<float> coverage;
    _rasterizer.Rasterize(polygon, ctx.width, ctx.height, coverage);

    // Apply coverage to pixels
    for (int y = 0; y < ctx.height; ++y) {
        for (int x = 0; x < ctx.width; ++x) {
            float cov = coverage[y * ctx.width + x];
            if (cov > 0) {
                BlendPixel(*ctx.targetImage, x, y, color, cov);
            }
        }
    }
}

inline void SVGRendererV2::FillSubPaths(const std::vector<std::vector<Vec2>>& subPaths,
                                         const glm::vec4& color,
                                         FillRule fillRule,
                                         RenderContext& ctx) {
    if (subPaths.empty() || color.a <= 0) return;

    _rasterizer.SetFillRule(fillRule);
    _rasterizer.SetAAMode(ctx.enableAA ? ctx.aaMode : ScanlineRasterizer::AAMode::None);

    std::vector<float> coverage;
    _rasterizer.RasterizeSubPaths(subPaths, ctx.width, ctx.height, coverage);

    // Apply coverage to pixels
    for (int y = 0; y < ctx.height; ++y) {
        for (int x = 0; x < ctx.width; ++x) {
            float cov = coverage[y * ctx.width + x];
            if (cov > 0) {
                BlendPixel(*ctx.targetImage, x, y, color, cov);
            }
        }
    }
}

inline void SVGRendererV2::FillSubPathsEx(const std::vector<SubPathV2>& subPaths,
                                           const glm::vec4& color,
                                           FillRule fillRule,
                                           RenderContext& ctx) {
    if (subPaths.empty() || color.a <= 0) return;

    // 对于填充，closed 属性不影响结果，只需要提取所有顶点
    std::vector<std::vector<Vec2>> allSubPaths;
    for (const auto& sp : subPaths) {
        if (sp.points.size() >= 3) {
            allSubPaths.push_back(sp.points);
        }
    }
    
    if (allSubPaths.empty()) return;

    _rasterizer.SetFillRule(fillRule);
    _rasterizer.SetAAMode(ctx.enableAA ? ctx.aaMode : ScanlineRasterizer::AAMode::None);

    std::vector<float> coverage;
    _rasterizer.RasterizeSubPaths(allSubPaths, ctx.width, ctx.height, coverage);

    // Apply coverage to pixels
    for (int y = 0; y < ctx.height; ++y) {
        for (int x = 0; x < ctx.width; ++x) {
            float cov = coverage[y * ctx.width + x];
            if (cov > 0) {
                BlendPixel(*ctx.targetImage, x, y, color, cov);
            }
        }
    }
}

inline void SVGRendererV2::FillPolygonWithPaint(const std::vector<Vec2>& polygon,
                                                 const Paint& paint,
                                                 FillRule fillRule,
                                                 RenderContext& ctx) {
    if (polygon.size() < 3 || paint.IsNone()) return;

    _rasterizer.SetFillRule(fillRule);
    _rasterizer.SetAAMode(ctx.enableAA ? ctx.aaMode : ScanlineRasterizer::AAMode::None);

    std::vector<float> coverage;
    _rasterizer.Rasterize(polygon, ctx.width, ctx.height, coverage);

    BBox bounds = Geometry::ComputeBBox(polygon);

    // Apply coverage with paint sampling
    for (int y = 0; y < ctx.height; ++y) {
        for (int x = 0; x < ctx.width; ++x) {
            float cov = coverage[y * ctx.width + x];
            if (cov > 0) {
                Vec2 samplePoint(x + 0.5f, y + 0.5f);
                glm::vec4 color = paint.Sample(samplePoint, bounds);
                BlendPixel(*ctx.targetImage, x, y, color, cov);
            }
        }
    }
}

inline void SVGRendererV2::StrokePath(const std::vector<Vec2>& vertices, bool closed,
                                       const glm::vec4& color, const StrokeStyle& style,
                                       RenderContext& ctx) {
    if (vertices.size() < 2 || color.a <= 0 || style.width < 0.1f) return;

    _strokeExpander.SetStyle(style);
    
    // Check if we have a dash pattern
    if (!style.dashArray.empty()) {
        // Apply dash pattern to get multiple dash segments
        std::vector<std::vector<Vec2>> dashSegments = _strokeExpander.ApplyDashPattern(vertices, closed);
        
        // Render each dash segment (dashes are always open paths)
        for (const auto& segment : dashSegments) {
            if (segment.size() >= 2) {
                std::vector<Vec2> strokePolygon = _strokeExpander.ExpandPolyline(segment, false);
                if (strokePolygon.size() >= 3) {
                    FillPolygon(strokePolygon, color, FillRule::NonZero, ctx);
                }
            }
        }
    } else {
        // No dash pattern - render solid stroke
        std::vector<Vec2> strokePolygon = _strokeExpander.ExpandPolyline(vertices, closed);

        if (strokePolygon.size() >= 3) {
            FillPolygon(strokePolygon, color, FillRule::NonZero, ctx);
        }
    }
}

inline void SVGRendererV2::StrokeSubPaths(const std::vector<std::vector<Vec2>>& subPaths,
                                           const glm::vec4& color, const StrokeStyle& style,
                                           RenderContext& ctx) {
    if (subPaths.empty() || color.a <= 0 || style.width < 0.1f) return;

    _strokeExpander.SetStyle(style);

    for (const auto& vertices : subPaths) {
        if (vertices.size() < 2) continue;
        
        // Check if this sub-path is closed (first and last point are close)
        bool closed = vertices.size() >= 3 && 
                      DistanceSquared(vertices.front(), vertices.back()) < 1e-4f;

        std::vector<Vec2> strokePolygon = _strokeExpander.ExpandPolyline(vertices, closed);

        if (strokePolygon.size() >= 3) {
            FillPolygon(strokePolygon, color, FillRule::NonZero, ctx);
        }
    }
}

inline void SVGRendererV2::StrokeSubPathsEx(const std::vector<SubPathV2>& subPaths,
                                             const glm::vec4& color, const StrokeStyle& style,
                                             RenderContext& ctx) {
    if (subPaths.empty() || color.a <= 0 || style.width < 0.1f) return;

    _strokeExpander.SetStyle(style);

    for (const auto& subPath : subPaths) {
        if (subPath.points.size() < 2) continue;
        
        // 使用明确的 closed 属性，而不是猜测
        bool closed = subPath.closed;

        // Check if we have a dash pattern
        if (!style.dashArray.empty()) {
            // Apply dash pattern to get multiple dash segments
            std::vector<std::vector<Vec2>> dashSegments = _strokeExpander.ApplyDashPattern(subPath.points, closed);
            
            // Render each dash segment (dashes are always open paths)
            for (const auto& segment : dashSegments) {
                if (segment.size() >= 2) {
                    std::vector<Vec2> strokePolygon = _strokeExpander.ExpandPolyline(segment, false);
                    if (strokePolygon.size() >= 3) {
                        FillPolygon(strokePolygon, color, FillRule::NonZero, ctx);
                    }
                }
            }
        } else {
            // No dash pattern - render solid stroke
            std::vector<Vec2> strokePolygon = _strokeExpander.ExpandPolyline(subPath.points, closed);

            if (strokePolygon.size() >= 3) {
                FillPolygon(strokePolygon, color, FillRule::NonZero, ctx);
            }
        }
    }
}

inline void SVGRendererV2::BlendPixel(Common::ImageRGB& image, int x, int y,
                                       const glm::vec4& color, float coverage) {
    auto [width, height] = image.GetSize();
    
    if (x < 0 || x >= static_cast<int>(width) || y < 0 || y >= static_cast<int>(height)) {
        return;
    }

    float alpha = color.a * coverage;
    if (alpha <= 0) return;

    glm::vec3 existing = image.At(x, y);
    float invAlpha = 1.0f - alpha;

    glm::vec3 blended(
        existing.r * invAlpha + color.r * alpha,
        existing.g * invAlpha + color.g * alpha,
        existing.b * invAlpha + color.b * alpha
    );

    image.At(x, y) = blended;
}

inline glm::vec4 SVGRendererV2::GetFillColor(const SVGStyle& style) {
    // 根据SVG规范，如果显式设置了fill="none"，则不填充
    if (style.fillNone) return glm::vec4(0, 0, 0, 0);
    
    // 如果没有设置fillColor，SVG默认为黑色
    glm::vec4 color = style.fillColor.value_or(glm::vec4(0, 0, 0, 1));
    if (style.fillOpacity) color.a *= *style.fillOpacity;
    if (style.opacity) color.a *= *style.opacity;
    
    return color;
}

inline glm::vec4 SVGRendererV2::GetStrokeColor(const SVGStyle& style) {
    if (!style.strokeColor) return glm::vec4(0, 0, 0, 0);
    
    glm::vec4 color = *style.strokeColor;
    if (style.strokeOpacity) color.a *= *style.strokeOpacity;
    if (style.opacity) color.a *= *style.opacity;
    
    return color;
}

inline StrokeStyle SVGRendererV2::GetStrokeStyle(const SVGStyle& style) {
    StrokeStyle strokeStyle;
    strokeStyle.width = style.strokeWidth.value_or(1.0f);
    
    // Parse lineCap from style
    if (style.strokeLineCap) {
        const std::string& cap = *style.strokeLineCap;
        if (cap == "round") {
            strokeStyle.lineCap = LineCap::Round;
        } else if (cap == "square") {
            strokeStyle.lineCap = LineCap::Square;
        } else {
            strokeStyle.lineCap = LineCap::Butt;  // Default
        }
    } else {
        strokeStyle.lineCap = LineCap::Butt;
    }
    
    // Parse lineJoin from style
    if (style.strokeLineJoin) {
        const std::string& join = *style.strokeLineJoin;
        if (join == "round") {
            strokeStyle.lineJoin = LineJoin::Round;
        } else if (join == "bevel") {
            strokeStyle.lineJoin = LineJoin::Bevel;
        } else {
            strokeStyle.lineJoin = LineJoin::Miter;  // Default
        }
    } else {
        strokeStyle.lineJoin = LineJoin::Miter;
    }
    
    // Parse miter limit
    strokeStyle.miterLimit = style.strokeMiterLimit.value_or(4.0f);
    
    // Parse dash array
    if (style.strokeDashArray && !style.strokeDashArray->empty()) {
        strokeStyle.dashArray = *style.strokeDashArray;
        strokeStyle.dashOffset = style.strokeDashOffset.value_or(0.0f);
    }
    
    return strokeStyle;
}

inline FillRule SVGRendererV2::GetFillRule(const SVGStyle& style) {
    if (style.fillRule && *style.fillRule == "evenodd") {
        return FillRule::EvenOdd;
    }
    return FillRule::NonZero;
}

inline Matrix3x3 SVGRendererV2::ConvertTransform(const Transform2D& t) {
    return Matrix3x3::FromGlm(t.matrix);
}

} // namespace VCX::Labs::SVG
