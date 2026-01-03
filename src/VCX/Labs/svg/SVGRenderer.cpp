#include "SVGRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <limits>

namespace VCX::Labs::SVG {

// 简单的 5x7 字体数据 (ASCII 32-126)
static const unsigned char font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3e}, // @
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
    {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
    {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7f, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, // H
    {0x00, 0x41, 0x7f, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3f, 0x01}, // J
    {0x7f, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // M
    {0x7f, 0x04, 0x08, 0x10, 0x7f}, // N
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
    {0x7f, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
    {0x7f, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7f, 0x01, 0x01}, // T
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
    {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7f, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \
    {0x00, 0x41, 0x41, 0x7f, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7f, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7f}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7e, 0x09, 0x01, 0x02}, // f
    {0x0c, 0x52, 0x52, 0x52, 0x3e}, // g
    {0x7f, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7d, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3d, 0x00}, // j
    {0x7f, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7f, 0x40, 0x00}, // l
    {0x7c, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7c, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7c, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7c}, // q
    {0x7c, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3f, 0x44, 0x40, 0x20}, // t
    {0x3c, 0x40, 0x40, 0x20, 0x7c}, // u
    {0x1c, 0x20, 0x40, 0x20, 0x1c}, // v
    {0x3c, 0x40, 0x30, 0x40, 0x3c}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0c, 0x50, 0x50, 0x50, 0x3c}, // y
    {0x44, 0x64, 0x54, 0x4c, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7f, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
};

SVGRenderer::SVGRenderer() : _backgroundColor(1.0f, 1.0f, 1.0f, 1.0f) {}

SVGRenderer::~SVGRenderer() = default;

void SVGRenderer::SetBackgroundColor(const glm::vec4& color) {
    _backgroundColor = color;
}

Common::ImageRGB SVGRenderer::RenderSVG(const SVGDocument& document,
                                       std::uint32_t width,
                                       std::uint32_t height) {
    // 创建图像
    Common::ImageRGB image(width, height);

    // 初始化背景
    glm::vec3 bgColor(_backgroundColor.r, _backgroundColor.g, _backgroundColor.b);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            image.At(x, y) = bgColor;
        }
    }

    // 渲染所有元素
    for (const auto& element : document.elements) {
        RenderElement(element, image);
    }

    return image;
}

void SVGRenderer::RenderElement(const SVGElement& element,
                               Common::ImageRGB& targetImage) {
    switch (element.type) {
        case SVGElement::Type::Path:
            RenderPath(element.path, targetImage);
            break;
        case SVGElement::Type::Circle:
            RenderCircle(element.circle, targetImage);
            break;
        case SVGElement::Type::Ellipse:
            RenderEllipse(element.ellipse, targetImage);
            break;
        case SVGElement::Type::Rect:
            RenderRect(element.rect, targetImage);
            break;
        case SVGElement::Type::Line:
            RenderLine(element.line, targetImage);
            break;
        case SVGElement::Type::Text:
            RenderText(element.text, targetImage);
            break;
        case SVGElement::Type::Group:
            // 递归渲染组内的元素
            for (const auto& child : element.children) {
                RenderElement(child, targetImage);
            }
            break;
    }
}

void SVGRenderer::RenderPath(const SVGPath& path, Common::ImageRGB& image) {
    // 获取所有子路径
    auto subPaths = path.GetSubPaths();
    
    if (subPaths.empty()) return;

    // 计算填充和描边颜色
    // 根据SVG规范，默认fill是黑色(#000000)，除非显式设置为none
    glm::vec4 fillColor(0, 0, 0, 1); // SVG默认fill为黑色
    if (path.style.fillColor) {
        fillColor = *path.style.fillColor;
        if (path.style.fillOpacity) {
            fillColor.a *= *path.style.fillOpacity;
        }
        if (path.style.opacity) {
            fillColor.a *= *path.style.opacity;
        }
    } else if (path.style.fillNone) {
        // 如果显式设置了fill="none"，则不填充
        fillColor = glm::vec4(0, 0, 0, 0);
    }

    glm::vec4 strokeColor(0, 0, 0, 0); // 默认透明
    if (path.style.strokeColor) {
        strokeColor = *path.style.strokeColor;
        if (path.style.strokeOpacity) {
            strokeColor.a *= *path.style.strokeOpacity;
        }
        if (path.style.opacity) {
            strokeColor.a *= *path.style.opacity;
        }
    }

    float strokeWidth = path.style.strokeWidth.value_or(1.0f);

    // 判断填充规则：默认为NonZero
    bool useNonZeroRule = true;
    if (path.style.fillRule.has_value() && path.style.fillRule.value() == "evenodd") {
        useNonZeroRule = false;
    }

    // 使用支持多子路径的渲染方法
    DrawPathWithSubPaths(image, subPaths, fillColor, strokeColor, strokeWidth, useNonZeroRule);
}

void SVGRenderer::RenderCircle(const SVGCircle& circle, Common::ImageRGB& image) {
    Point2D center = circle.transform.TransformPoint(circle.center);

    // 根据SVG规范，默认fill是黑色
    glm::vec4 fillColor(0, 0, 0, 1);
    if (circle.style.fillColor) {
        fillColor = *circle.style.fillColor;
        if (circle.style.fillOpacity) {
            fillColor.a *= *circle.style.fillOpacity;
        }
        if (circle.style.opacity) {
            fillColor.a *= *circle.style.opacity;
        }
    } else if (circle.style.fillNone) {
        fillColor = glm::vec4(0, 0, 0, 0);
    }

    glm::vec4 strokeColor(0, 0, 0, 0);
    if (circle.style.strokeColor) {
        strokeColor = *circle.style.strokeColor;
        if (circle.style.strokeOpacity) {
            strokeColor.a *= *circle.style.strokeOpacity;
        }
        if (circle.style.opacity) {
            strokeColor.a *= *circle.style.opacity;
        }
    }

    // 渲染填充
    if (fillColor.a > 0) {
        DrawCircle(image, center, circle.radius, fillColor, true);
    }

    // 渲染描边
    if (strokeColor.a > 0 && circle.style.strokeWidth) {
        DrawCircle(image, center, circle.radius, strokeColor, false);
    }
}

void SVGRenderer::RenderEllipse(const SVGEllipse& ellipse, Common::ImageRGB& image) {
    Point2D center = ellipse.transform.TransformPoint(ellipse.center);

    // 根据SVG规范，默认fill是黑色
    glm::vec4 fillColor(0, 0, 0, 1);
    if (ellipse.style.fillColor) {
        fillColor = *ellipse.style.fillColor;
        if (ellipse.style.fillOpacity) {
            fillColor.a *= *ellipse.style.fillOpacity;
        }
        if (ellipse.style.opacity) {
            fillColor.a *= *ellipse.style.opacity;
        }
    } else if (ellipse.style.fillNone) {
        fillColor = glm::vec4(0, 0, 0, 0);
    }

    glm::vec4 strokeColor(0, 0, 0, 0);
    if (ellipse.style.strokeColor) {
        strokeColor = *ellipse.style.strokeColor;
        if (ellipse.style.strokeOpacity) {
            strokeColor.a *= *ellipse.style.strokeOpacity;
        }
        if (ellipse.style.opacity) {
            strokeColor.a *= *ellipse.style.opacity;
        }
    }

    // 简化的椭圆渲染 - 使用多个圆近似
    // 实际实现应该使用椭圆光栅化算法
    int cx = static_cast<int>(center.x);
    int cy = static_cast<int>(center.y);
    int rx = static_cast<int>(ellipse.rx);
    int ry = static_cast<int>(ellipse.ry);

    if (fillColor.a > 0) {
        // 填充椭圆 - 简化为在椭圆边界内的像素
        for (int y = cy - ry; y <= cy + ry; y++) {
            for (int x = cx - rx; x <= cx + rx; x++) {
                float dx = (x - cx) / static_cast<float>(rx);
                float dy = (y - cy) / static_cast<float>(ry);
                if (dx * dx + dy * dy <= 1.0f) {
                    SetPixel(image, x, y, fillColor);
                }
            }
        }
    }

    if (strokeColor.a > 0 && ellipse.style.strokeWidth) {
        // 描边椭圆 - 简化为椭圆轮廓
        for (int y = cy - ry; y <= cy + ry; y++) {
            for (int x = cx - rx; x <= cx + rx; x++) {
                float dx = (x - cx) / static_cast<float>(rx);
                float dy = (y - cy) / static_cast<float>(ry);
                float dist = dx * dx + dy * dy;
                if (dist >= 0.95f && dist <= 1.05f) {  // 椭圆边界附近
                    SetPixel(image, x, y, strokeColor);
                }
            }
        }
    }
}

void SVGRenderer::RenderRect(const SVGRect& rect, Common::ImageRGB& image) {
    Point2D position = rect.transform.TransformPoint(rect.position);

    // 根据SVG规范，默认fill是黑色
    glm::vec4 fillColor(0, 0, 0, 1);
    if (rect.style.fillColor) {
        fillColor = *rect.style.fillColor;
        if (rect.style.fillOpacity) {
            fillColor.a *= *rect.style.fillOpacity;
        }
        if (rect.style.opacity) {
            fillColor.a *= *rect.style.opacity;
        }
    } else if (rect.style.fillNone) {
        fillColor = glm::vec4(0, 0, 0, 0);
    }

    glm::vec4 strokeColor(0, 0, 0, 0);
    if (rect.style.strokeColor) {
        strokeColor = *rect.style.strokeColor;
        if (rect.style.strokeOpacity) {
            strokeColor.a *= *rect.style.strokeOpacity;
        }
        if (rect.style.opacity) {
            strokeColor.a *= *rect.style.opacity;
        }
    }

    // 渲染填充
    if (fillColor.a > 0) {
        DrawRect(image, position, rect.width, rect.height, fillColor, true, rect.rx, rect.ry);
    }

    // 渲染描边
    if (strokeColor.a > 0 && rect.style.strokeWidth) {
        DrawRect(image, position, rect.width, rect.height, strokeColor, false, rect.rx, rect.ry);
    }
}

void SVGRenderer::RenderLine(const SVGLine& line, Common::ImageRGB& image) {
    Point2D start = line.transform.TransformPoint(line.start);
    Point2D end = line.transform.TransformPoint(line.end);

    glm::vec4 strokeColor(0, 0, 0, 1);
    if (line.style.strokeColor) {
        strokeColor = *line.style.strokeColor;
        if (line.style.strokeOpacity) {
            strokeColor.a *= *line.style.strokeOpacity;
        }
        if (line.style.opacity) {
            strokeColor.a *= *line.style.opacity;
        }
    }

    float strokeWidth = line.style.strokeWidth.value_or(1.0f);

    DrawLine(image, start, end, strokeColor, strokeWidth);
}

void SVGRenderer::RenderText(const SVGText& text, Common::ImageRGB& image) {
    Point2D position = text.transform.TransformPoint(text.position);

    glm::vec4 fillColor(0, 0, 0, 1);
    if (text.style.fillColor) {
        fillColor = *text.style.fillColor;
        if (text.style.fillOpacity) {
            fillColor.a *= *text.style.fillOpacity;
        }
        if (text.style.opacity) {
            fillColor.a *= *text.style.opacity;
        }
    }

    // 使用 5x7 字体渲染文本
    float scale = text.fontSize / 7.0f;
    float xOffset = 0;
    for (char c : text.text) {
        if (c >= 32 && c <= 126) {
            int idx = c - 32;
            for (int col = 0; col < 5; col++) {
                unsigned char bits = font5x7[idx][col];
                for (int row = 0; row < 7; row++) {
                    if (bits & (1 << row)) {
                        // 绘制缩放后的像素块
                        int px = static_cast<int>(position.x + (xOffset + col) * scale);
                        int py = static_cast<int>(position.y + row * scale);
                        int size = static_cast<int>(std::ceil(scale));
                        for (int dy = 0; dy < size; dy++) {
                            for (int dx = 0; dx < size; dx++) {
                                BlendPixel(image, px + dx, py + dy, fillColor);
                            }
                        }
                    }
                }
            }
            xOffset += 6; // 5 cols + 1 space
        }
    }
}

void SVGRenderer::DrawLine(Common::ImageRGB& image,
                         const Point2D& start, const Point2D& end,
                         const glm::vec4& color, float width) {
    int x1 = static_cast<int>(start.x);
    int y1 = static_cast<int>(start.y);
    int x2 = static_cast<int>(end.x);
    int y2 = static_cast<int>(end.y);

    BresenhamLine(image, x1, y1, x2, y2, color);

    // 如果宽度大于1，绘制额外的像素
    if (width > 1.0f) {
        // 简化的粗线实现
        for (float w = 1; w < width; w += 1.0f) {
            Point2D offset((end.y - start.y) / (end.x - start.x + 0.001f), 1.0f);
            offset = offset * (w * 0.5f);

            Point2D start1 = start + offset;
            Point2D end1 = end + offset;
            Point2D start2 = start - offset;
            Point2D end2 = end - offset;

            BresenhamLine(image, start1.x, start1.y, end1.x, end1.y, color);
            BresenhamLine(image, start2.x, start2.y, end2.x, end2.y, color);
        }
    }
}

void SVGRenderer::DrawCircle(Common::ImageRGB& image,
                            const Point2D& center, float radius,
                            const glm::vec4& color, bool filled) {
    int cx = static_cast<int>(center.x);
    int cy = static_cast<int>(center.y);
    int r = static_cast<int>(radius);

    if (filled) {
        // 填充圆 - 使用中点圆算法的变体
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    SetPixel(image, cx + x, cy + y, color);
                }
            }
        }
    } else {
        // 空心圆 - 使用中点圆算法
        int x = r, y = 0;
        int err = 0;

        while (x >= y) {
            SetPixel(image, cx + x, cy + y, color);
            SetPixel(image, cx + y, cy + x, color);
            SetPixel(image, cx - y, cy + x, color);
            SetPixel(image, cx - x, cy + y, color);
            SetPixel(image, cx - x, cy - y, color);
            SetPixel(image, cx - y, cy - x, color);
            SetPixel(image, cx + y, cy - x, color);
            SetPixel(image, cx + x, cy - y, color);

            if (err <= 0) {
                y += 1;
                err += 2 * y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
    }
}

void SVGRenderer::DrawRect(Common::ImageRGB& image,
                          const Point2D& position, float width, float height,
                          const glm::vec4& color, bool filled, float rx, float ry) {
    int x = static_cast<int>(position.x);
    int y = static_cast<int>(position.y);
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);

    if (filled) {
        // 填充矩形
        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                SetPixel(image, px, py, color);
            }
        }
    } else {
        // 空心矩形
        // 绘制四条边
        for (int px = x; px < x + w; px++) {
            SetPixel(image, px, y, color);         // 上边
            SetPixel(image, px, y + h - 1, color); // 下边
        }
        for (int py = y; py < y + h; py++) {
            SetPixel(image, x, py, color);         // 左边
            SetPixel(image, x + w - 1, py, color); // 右边
        }
    }
}

void SVGRenderer::DrawPath(Common::ImageRGB& image,
                          const std::vector<Point2D>& points,
                          const glm::vec4& fillColor, const glm::vec4& strokeColor,
                          float strokeWidth, bool closed) {
    if (points.size() < 2) return;

    // 1. 绘制填充
    if (fillColor.a > 0 && points.size() >= 3) {
        // 扫描线填充算法 (Even-Odd Rule)
        float minY = points[0].y, maxY = points[0].y;
        for (const auto& p : points) {
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        int iMinY = std::max(0, static_cast<int>(std::floor(minY)));
        int iMaxY = std::min(static_cast<int>(image.GetSizeY()) - 1, static_cast<int>(std::ceil(maxY)));

        for (int y = iMinY; y <= iMaxY; y++) {
            std::vector<float> intersections;
            float scanY = static_cast<float>(y) + 0.5f; // 使用像素中心

            for (size_t i = 0; i < points.size(); i++) {
                Point2D p1 = points[i];
                Point2D p2 = points[(i + 1) % points.size()];

                if ((p1.y <= scanY && p2.y > scanY) || (p2.y <= scanY && p1.y > scanY)) {
                    float x = p1.x + (scanY - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
                    intersections.push_back(x);
                }
            }

            std::sort(intersections.begin(), intersections.end());

            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                int xStart = std::max(0, static_cast<int>(std::ceil(intersections[i])));
                int xEnd = std::min(static_cast<int>(image.GetSizeX()) - 1, static_cast<int>(std::floor(intersections[i+1])));
                for (int x = xStart; x <= xEnd; x++) {
                    BlendPixel(image, x, y, fillColor);
                }
            }
        }
    }

    // 2. 绘制描边
    if (strokeColor.a > 0) {
        for (size_t i = 0; i < points.size() - 1; i++) {
            DrawLine(image, points[i], points[i + 1], strokeColor, strokeWidth);
        }
        if (closed && points.size() > 2) {
            DrawLine(image, points.back(), points[0], strokeColor, strokeWidth);
        }
    }
}

void SVGRenderer::DrawPathWithSubPaths(Common::ImageRGB& image,
                                       const std::vector<std::vector<Point2D>>& subPaths,
                                       const glm::vec4& fillColor, const glm::vec4& strokeColor,
                                       float strokeWidth, bool useNonZeroRule) {
    if (subPaths.empty()) return;

    // 1. 绘制填充（使用NonZero或EvenOdd规则）
    if (fillColor.a > 0) {
        // 计算所有子路径的边界
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        
        for (const auto& path : subPaths) {
            for (const auto& p : path) {
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
            }
        }

        int iMinY = std::max(0, static_cast<int>(std::floor(minY)));
        int iMaxY = std::min(static_cast<int>(image.GetSizeY()) - 1, static_cast<int>(std::ceil(maxY)));

        // 扫描线填充
        for (int y = iMinY; y <= iMaxY; y++) {
            float scanY = static_cast<float>(y) + 0.5f;
            
            // 收集所有子路径与扫描线的交点，同时记录方向
            std::vector<std::pair<float, int>> intersections; // (x, direction)
            
            for (const auto& path : subPaths) {
                if (path.size() < 2) continue;
                
                for (size_t i = 0; i < path.size(); i++) {
                    Point2D p1 = path[i];
                    Point2D p2 = path[(i + 1) % path.size()];
                    
                    // 检查边是否与扫描线相交
                    if ((p1.y <= scanY && p2.y > scanY) || (p2.y <= scanY && p1.y > scanY)) {
                        float x = p1.x + (scanY - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
                        // 方向：向上为+1，向下为-1
                        int direction = (p2.y > p1.y) ? 1 : -1;
                        intersections.push_back({x, direction});
                    }
                }
            }
            
            // 按x坐标排序
            std::sort(intersections.begin(), intersections.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            
            if (useNonZeroRule) {
                // NonZero填充规则：跟踪winding number，非零时填充
                int windingNumber = 0;
                float fillStart = 0;
                bool inFill = false;
                
                for (size_t i = 0; i < intersections.size(); i++) {
                    float x = intersections[i].first;
                    int dir = intersections[i].second;
                    
                    bool wasInFill = (windingNumber != 0);
                    windingNumber += dir;
                    bool nowInFill = (windingNumber != 0);
                    
                    if (!wasInFill && nowInFill) {
                        // 开始填充区域
                        fillStart = x;
                        inFill = true;
                    } else if (wasInFill && !nowInFill) {
                        // 结束填充区域
                        int xStart = std::max(0, static_cast<int>(std::ceil(fillStart)));
                        int xEnd = std::min(static_cast<int>(image.GetSizeX()) - 1, 
                                           static_cast<int>(std::floor(x)));
                        for (int px = xStart; px <= xEnd; px++) {
                            BlendPixel(image, px, y, fillColor);
                        }
                        inFill = false;
                    }
                }
            } else {
                // EvenOdd填充规则 - 简单地在奇数交点对之间填充
                for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                    int xStart = std::max(0, static_cast<int>(std::ceil(intersections[i].first)));
                    int xEnd = std::min(static_cast<int>(image.GetSizeX()) - 1, 
                                       static_cast<int>(std::floor(intersections[i+1].first)));
                    for (int x = xStart; x <= xEnd; x++) {
                        BlendPixel(image, x, y, fillColor);
                    }
                }
            }
        }
    }

    // 2. 绘制描边
    if (strokeColor.a > 0) {
        for (const auto& path : subPaths) {
            if (path.size() < 2) continue;
            
            for (size_t i = 0; i < path.size() - 1; i++) {
                DrawLine(image, path[i], path[i + 1], strokeColor, strokeWidth);
            }
            // 闭合路径
            if (path.size() > 2) {
                DrawLine(image, path.back(), path[0], strokeColor, strokeWidth);
            }
        }
    }
}

void SVGRenderer::SetPixel(Common::ImageRGB& image, int x, int y, const glm::vec4& color) {
    auto [width, height] = image.GetSize();

    if (x >= 0 && x < static_cast<int>(width) && y >= 0 && y < static_cast<int>(height)) {
        // ImageRGB使用RGB格式，所以我们忽略alpha通道，转换为RGB
        glm::vec3 rgbColor(color.r, color.g, color.b);
        image.At(x, y) = rgbColor;
    }
}

glm::vec4 SVGRenderer::GetPixel(const Common::ImageRGB& image, int x, int y) {
    auto [width, height] = image.GetSize();

    if (x >= 0 && x < static_cast<int>(width) && y >= 0 && y < static_cast<int>(height)) {
        glm::vec3 rgbColor = image.At(x, y);
        return glm::vec4(rgbColor.r, rgbColor.g, rgbColor.b, 1.0f);
    }

    return glm::vec4(0, 0, 0, 1);
}

void SVGRenderer::BlendPixel(Common::ImageRGB& image, int x, int y, const glm::vec4& color) {
    glm::vec4 existing = GetPixel(image, x, y);

    // Alpha混合
    float alpha = color.a;
    float invAlpha = 1.0f - alpha;

    glm::vec4 blended(
        existing.r * invAlpha + color.r * alpha,
        existing.g * invAlpha + color.g * alpha,
        existing.b * invAlpha + color.b * alpha,
        1.0f
    );

    SetPixel(image, x, y, blended);
}

bool SVGRenderer::IsPointInRect(int x, int y, int left, int top, int right, int bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}

void SVGRenderer::BresenhamLine(Common::ImageRGB& image, int x1, int y1, int x2, int y2, const glm::vec4& color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        SetPixel(image, x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

} // namespace VCX::Labs::SVG
