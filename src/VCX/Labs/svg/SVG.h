#pragma once

#include <vector>
#include <string>
#include <optional>
#include <glm/glm.hpp>

// Forward declarations
namespace tinyxml2 {
    class XMLDocument;
    class XMLElement;
}

namespace VCX::Labs::SVG {

// SVG 样式结构
struct SVGStyle {
    std::optional<glm::vec4> fillColor;        // RGBA fill color
    std::optional<glm::vec4> strokeColor;      // RGBA stroke color
    std::optional<float>     strokeWidth;      // Stroke width
    std::optional<float>     opacity;          // Overall opacity
    std::optional<float>     fillOpacity;      // Fill opacity
    std::optional<float>     strokeOpacity;    // Stroke opacity
    std::optional<std::string> fillRule;       // "evenodd" or "nonzero"
    bool fillNone = false;                     // 显式设置fill="none"
    bool strokeNone = false;                   // 显式设置stroke="none"
    
    // Stroke styling (V2 renderer)
    std::optional<std::string> strokeLineCap;  // "butt", "round", "square"
    std::optional<std::string> strokeLineJoin; // "miter", "round", "bevel"
    std::optional<float>     strokeMiterLimit; // Miter limit ratio (default 4)
    std::optional<std::vector<float>> strokeDashArray;  // Dash pattern
    std::optional<float>     strokeDashOffset; // Dash offset
};

// 2D点结构
struct Point2D {
    float x, y;
    Point2D(float x = 0, float y = 0) : x(x), y(y) {}
    Point2D operator+(const Point2D& other) const { return Point2D(x + other.x, y + other.y); }
    Point2D operator-(const Point2D& other) const { return Point2D(x - other.x, y - other.y); }
    Point2D operator*(float scalar) const { return Point2D(x * scalar, y * scalar); }
};

// 2D变换矩阵
struct Transform2D {
    glm::mat3 matrix;

    Transform2D() : matrix(1.0f) {}
    Transform2D(const glm::mat3& m) : matrix(m) {}

    // 平移
    static Transform2D Translate(float tx, float ty) {
        glm::mat3 m(1.0f);
        m[2][0] = tx;
        m[2][1] = ty;
        return Transform2D(m);
    }

    // 缩放
    static Transform2D Scale(float sx, float sy) {
        glm::mat3 m(1.0f);
        m[0][0] = sx;
        m[1][1] = sy;
        return Transform2D(m);
    }

    // 旋转
    static Transform2D Rotate(float angle) {
        float c = cos(angle);
        float s = sin(angle);
        glm::mat3 m(1.0f);
        m[0][0] = c; m[0][1] = -s;
        m[1][0] = s; m[1][1] = c;
        return Transform2D(m);
    }

    Transform2D operator*(const Transform2D& other) const {
        return Transform2D(matrix * other.matrix);
    }

    Point2D TransformPoint(const Point2D& p) const {
        glm::vec3 v(p.x, p.y, 1.0f);
        v = matrix * v;
        return Point2D(v.x, v.y);
    }
};

// SVG路径命令枚举
enum class PathCommandType {
    MoveTo,
    LineTo,
    CurveTo,        // Cubic Bezier curve
    QuadCurveTo,    // Quadratic Bezier curve
    ArcTo,
    ClosePath
};

// 路径命令
struct PathCommand {
    PathCommandType type;
    std::vector<Point2D> points;  // 参数点
    bool relative = false;        // 是否为相对坐标

    PathCommand(PathCommandType t, bool rel = false) : type(t), relative(rel) {}
};

// SVG路径元素
struct SVGPath {
    std::vector<PathCommand> commands;
    SVGStyle style;
    Transform2D transform;
    std::string id;

    // 获取所有顶点（用于渲染）
    std::vector<Point2D> GetVertices() const;
    
    // 获取所有子路径（每个子路径是一个独立的多边形）
    std::vector<std::vector<Point2D>> GetSubPaths() const;
};

// SVG圆形元素
struct SVGCircle {
    Point2D center;
    float radius;
    SVGStyle style;
    Transform2D transform;
    std::string id;
};

// SVG椭圆元素
struct SVGEllipse {
    Point2D center;
    float rx, ry;  // x和y方向的半径
    SVGStyle style;
    Transform2D transform;
    std::string id;
};

// SVG矩形元素
struct SVGRect {
    Point2D position;
    float width, height;
    float rx = 0, ry = 0;  // 圆角半径
    SVGStyle style;
    Transform2D transform;
    std::string id;
};

// SVG线段元素
struct SVGLine {
    Point2D start, end;
    SVGStyle style;
    Transform2D transform;
    std::string id;
};

// SVG文本元素
struct SVGText {
    std::string text;
    Point2D position;
    float fontSize = 12.0f;
    std::string fontFamily = "Arial";
    SVGStyle style;
    Transform2D transform;
    std::string id;
};

// SVG元素基类
struct SVGElement {
    enum Type {
        Path,
        Circle,
        Ellipse,
        Rect,
        Line,
        Text,
        Group
    } type;

    std::string id;
    SVGStyle style;
    Transform2D transform;
    std::vector<SVGElement> children;  // 用于group元素

    SVGElement(Type t) : type(t) {
        // 根据类型初始化对应的union成员
        switch (type) {
            case Path:    new (&path)    SVGPath();    break;
            case Circle:  new (&circle)  SVGCircle();  break;
            case Ellipse: new (&ellipse) SVGEllipse(); break;
            case Rect:    new (&rect)    SVGRect();    break;
            case Line:    new (&line)    SVGLine();    break;
            case Text:    new (&text)    SVGText();    break;
            case Group:   /* no union member for group */ break;
        }
    }

    // Non-copyable due to manual union management
    SVGElement(SVGElement const &) = delete;
    SVGElement & operator=(SVGElement const &) = delete;

    // Move constructor/assignment to allow storage in std::vector
    SVGElement(SVGElement && other) noexcept :
        type(other.type),
        id(std::move(other.id)),
        style(std::move(other.style)),
        transform(std::move(other.transform)),
        children(std::move(other.children)) {
        switch (type) {
            case Path:    new (&path)    SVGPath(std::move(other.path));       break;
            case Circle:  new (&circle)  SVGCircle(std::move(other.circle));   break;
            case Ellipse: new (&ellipse) SVGEllipse(std::move(other.ellipse)); break;
            case Rect:    new (&rect)    SVGRect(std::move(other.rect));       break;
            case Line:    new (&line)    SVGLine(std::move(other.line));       break;
            case Text:    new (&text)    SVGText(std::move(other.text));       break;
            case Group:   /* no union member constructed */                    break;
        }
    }

    SVGElement & operator=(SVGElement && other) noexcept {
        if (this == &other) return *this;
        // Destroy current member
        switch (type) {
            case Path:    path.~SVGPath();       break;
            case Circle:  circle.~SVGCircle();   break;
            case Ellipse: ellipse.~SVGEllipse(); break;
            case Rect:    rect.~SVGRect();       break;
            case Line:    line.~SVGLine();       break;
            case Text:    text.~SVGText();       break;
            case Group:   break;
        }

        type = other.type;
        id = std::move(other.id);
        style = std::move(other.style);
        transform = std::move(other.transform);
        children = std::move(other.children);

        switch (type) {
            case Path:    new (&path)    SVGPath(std::move(other.path));       break;
            case Circle:  new (&circle)  SVGCircle(std::move(other.circle));   break;
            case Ellipse: new (&ellipse) SVGEllipse(std::move(other.ellipse)); break;
            case Rect:    new (&rect)    SVGRect(std::move(other.rect));       break;
            case Line:    new (&line)    SVGLine(std::move(other.line));       break;
            case Text:    new (&text)    SVGText(std::move(other.text));       break;
            case Group:   /* no union member constructed */                    break;
        }
        return *this;
    }

    // 获取具体元素数据的联合体或变体
    union {
        SVGPath path;
        SVGCircle circle;
        SVGEllipse ellipse;
        SVGRect rect;
        SVGLine line;
        SVGText text;
    };

    ~SVGElement() {
        // 析构函数处理联合体成员的清理
        switch (type) {
            case Path: path.~SVGPath(); break;
            case Circle: circle.~SVGCircle(); break;
            case Ellipse: ellipse.~SVGEllipse(); break;
            case Rect: rect.~SVGRect(); break;
            case Line: line.~SVGLine(); break;
            case Text: text.~SVGText(); break;
            case Group: break;
        }
    }
};

// SVG文档
struct SVGDocument {
    float width = 800.0f;
    float height = 600.0f;
    std::string viewBox;  // "x y width height"
    std::vector<SVGElement> elements;

    // 解析viewBox
    bool ParseViewBox(float& x, float& y, float& w, float& h) const;
};

} // namespace VCX::Labs::SVG
