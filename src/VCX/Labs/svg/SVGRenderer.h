#pragma once

#include "SVG.h"
#include "Labs/Common/ImageRGB.h"
#include <memory>

namespace VCX::Labs::SVG {

class SVGRenderer {
public:
    SVGRenderer();
    ~SVGRenderer();

    // 渲染SVG文档到图像
    Common::ImageRGB RenderSVG(const SVGDocument& document,
                              std::uint32_t width,
                              std::uint32_t height);

    // 渲染单个SVG元素
    void RenderElement(const SVGElement& element,
                      Common::ImageRGB& targetImage);

    // 设置背景颜色
    void SetBackgroundColor(const glm::vec4& color);

private:
    glm::vec4 _backgroundColor;

    // 渲染基本形状
    void RenderPath(const SVGPath& path, Common::ImageRGB& image);
    void RenderCircle(const SVGCircle& circle, Common::ImageRGB& image);
    void RenderEllipse(const SVGEllipse& ellipse, Common::ImageRGB& image);
    void RenderRect(const SVGRect& rect, Common::ImageRGB& image);
    void RenderLine(const SVGLine& line, Common::ImageRGB& image);
    void RenderText(const SVGText& text, Common::ImageRGB& image);

    // 绘制函数
    void DrawLine(Common::ImageRGB& image,
                const Point2D& start, const Point2D& end,
                const glm::vec4& color, float width = 1.0f);

    void DrawCircle(Common::ImageRGB& image,
                   const Point2D& center, float radius,
                   const glm::vec4& color, bool filled = true);

    void DrawRect(Common::ImageRGB& image,
                 const Point2D& position, float width, float height,
                 const glm::vec4& color, bool filled = true, float rx = 0, float ry = 0);

    void DrawPath(Common::ImageRGB& image,
                 const std::vector<Point2D>& points,
                 const glm::vec4& fillColor, const glm::vec4& strokeColor,
                 float strokeWidth = 1.0f, bool closed = false);

    // 像素操作
    void SetPixel(Common::ImageRGB& image, int x, int y, const glm::vec4& color);
    glm::vec4 GetPixel(const Common::ImageRGB& image, int x, int y);
    void BlendPixel(Common::ImageRGB& image, int x, int y, const glm::vec4& color);

    // 工具函数
    bool IsPointInRect(int x, int y, int left, int top, int right, int bottom);
    void BresenhamLine(Common::ImageRGB& image, int x1, int y1, int x2, int y2, const glm::vec4& color);
};

} // namespace VCX::Labs::SVG
