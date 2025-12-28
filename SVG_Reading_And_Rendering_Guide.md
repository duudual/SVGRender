# SVG 文件的读取与渲染完整讲解

本文档详细讲解 SVGRender 项目中如何逐步读取和渲染 SVG 文件。

---

## 一、整体架构概览

SVG 处理流程分为**三个主要阶段**：

```
SVG 文件 → 解析阶段（Parser）→ 存储数据结构 → 渲染阶段（Renderer）→ 图像输出
```

**涉及的主要类：**
- `SVGParser`：负责解析 SVG 文件和字符串，生成 SVG 数据结构
- `SVGDocument`、`SVGElement`：数据存储结构
- `SVGRenderer`：负责将 SVG 数据结构渲染为像素图像
- `CaseSVGRender`：应用层，集成解析和渲染流程

---

## 二、第一阶段：文件读取与初始化

### 2.1 入口函数：`CaseSVGRender::LoadSVGFile()`

```cpp
void CaseSVGRender::LoadSVGFile() {
    // 使用 SVGParser 解析 SVG 文件
    if (_svgParser.ParseFile(_svgFilePath, _svgDocument)) {
        _fileLoaded = true;
        _recompute = true;
        std::cout << "SVG file loaded successfully: " << _svgFilePath << std::endl;
        std::cout << "Elements found: " << _svgDocument.elements.size() << std::endl;
    }
}
```

**作用：**
- 接收用户输入的 SVG 文件路径
- 调用 `SVGParser::ParseFile()` 解析文件
- 将解析结果存储在 `_svgDocument` 中

---

## 三、第二阶段：SVG 文件解析

### 3.1 解析入口：`SVGParser::ParseFile()`

```cpp
bool SVGParser::ParseFile(const std::string& filename, SVGDocument& document) {
    // 步骤1：使用 tinyxml2 库加载 XML 文件
    tinyxml2::XMLError error = _xmlDoc->LoadFile(filename.c_str());
    if (error != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load SVG file: " << filename << std::endl;
        return false;
    }

    // 步骤2：查找 SVG 根元素
    tinyxml2::XMLElement* svgElement = _xmlDoc->FirstChildElement("svg");
    if (!svgElement) {
        std::cerr << "No SVG element found in file" << std::endl;
        return false;
    }

    // 步骤3：解析 SVG 根元素及其所有子元素
    return ParseSVGElement(svgElement, document);
}
```

**关键点：**
- 使用 **tinyxml2** 库处理 XML/SVG 文件
- 先找到 `<svg>` 根元素
- 再递归解析子元素

---

### 3.2 解析 SVG 根元素：`ParseSVGElement()`

```cpp
bool SVGParser::ParseSVGElement(tinyxml2::XMLElement* svgElement, SVGDocument& document) {
    // 步骤1：解析文档级别的属性
    document.width = ParseLength(GetAttribute(svgElement, "width", "800"), 800.0f);
    document.height = ParseLength(GetAttribute(svgElement, "height", "600"), 600.0f);
    document.viewBox = GetAttribute(svgElement, "viewBox");

    // 步骤2：遍历所有子元素
    for (tinyxml2::XMLElement* child = svgElement->FirstChildElement(); 
         child; 
         child = child->NextSiblingElement()) {
        
        SVGElement element(SVGElement::Type::Path); // 临时类型
        std::string tagName = child->Name();
        bool parsed = false;

        // 步骤3：根据标签名识别元素类型并解析
        if (tagName == "path") {
            element.type = SVGElement::Type::Path;
            parsed = ParsePathElement(child, element);
        } 
        else if (tagName == "circle") {
            element.type = SVGElement::Type::Circle;
            parsed = ParseCircleElement(child, element);
        } 
        // ... 其他元素类型（ellipse, rect, line, text, g）

        // 步骤4：将解析成功的元素添加到文档
        if (parsed) {
            document.elements.push_back(std::move(element));
        }
    }

    return true;
}
```

**流程：**
1. 获取 SVG 文档的宽度、高度、viewBox
2. 遍历所有子元素（通过 XML 兄弟指针遍历）
3. 根据标签名判断元素类型（path、circle、rect 等）
4. 调用对应的解析函数
5. 解析成功后加入文档的元素列表

---

### 3.3 具体元素解析示例

#### 3.3.1 圆形元素解析：`ParseCircleElement()`

```cpp
bool SVGParser::ParseCircleElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
    new (&svgElement.circle) SVGCircle();  // 在联合体中构造圆形对象

    // 解析基本几何属性
    svgElement.circle.id = GetAttribute(element, "id");
    svgElement.circle.center.x = ParseLength(GetAttribute(element, "cx", "0"));
    svgElement.circle.center.y = ParseLength(GetAttribute(element, "cy", "0"));
    svgElement.circle.radius = ParseLength(GetAttribute(element, "r", "0"));
    
    // 解析样式和变换
    svgElement.circle.style = ParseStyle(element);
    svgElement.circle.transform = ParseTransform(GetAttribute(element, "transform"));

    return true;
}
```

**解析的属性：**
- `id`：元素标识
- `cx, cy`：圆心坐标
- `r`：半径
- `style`：样式（填充色、描边色等）
- `transform`：变换矩阵（平移、缩放、旋转等）

#### 3.3.2 矩形元素解析：`ParseRectElement()`

```cpp
bool SVGParser::ParseRectElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
    new (&svgElement.rect) SVGRect();

    svgElement.rect.id = GetAttribute(element, "id");
    svgElement.rect.position.x = ParseLength(GetAttribute(element, "x", "0"));
    svgElement.rect.position.y = ParseLength(GetAttribute(element, "y", "0"));
    svgElement.rect.width = ParseLength(GetAttribute(element, "width", "0"));
    svgElement.rect.height = ParseLength(GetAttribute(element, "height", "0"));
    svgElement.rect.rx = ParseLength(GetAttribute(element, "rx", "0"));  // 圆角半径X
    svgElement.rect.ry = ParseLength(GetAttribute(element, "ry", "0"));  // 圆角半径Y
    svgElement.rect.style = ParseStyle(element);
    svgElement.rect.transform = ParseTransform(GetAttribute(element, "transform"));

    return true;
}
```

#### 3.3.3 路径元素解析：`ParsePathElement()`（最复杂）

```cpp
bool SVGParser::ParsePathElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
    new (&svgElement.path) SVGPath();

    svgElement.path.id = GetAttribute(element, "id");
    svgElement.path.style = ParseStyle(element);
    svgElement.path.transform = ParseTransform(GetAttribute(element, "transform"));

    // 关键：解析路径的 'd' 属性（包含 M, L, C, Q 等命令）
    std::string pathData = GetAttribute(element, "d");
    if (pathData.empty()) return false;

    return ParsePathData(pathData, svgElement.path.commands);
}
```

---

### 3.4 关键解析函数详解

#### 3.4.1 长度值解析：`ParseLength()`

```cpp
float SVGParser::ParseLength(const std::string& lengthStr, float defaultValue) {
    // 示例：输入 "100px" 或 "100pt"
    // 分离数字和单位
    std::string numStr;   // "100"
    std::string unit;     // "px"

    // ... 分离逻辑 ...

    float value = std::stof(numStr);  // 字符串转浮点数

    // 根据单位进行转换
    if (unit == "px" || unit.empty()) {
        return value;           // 像素，直接返回
    } else if (unit == "pt") {
        return value * 1.333f;  // 点数转像素（1pt ≈ 1.333px）
    } else if (unit == "pt") {
        return value * 96.0f;   // 英寸转像素（1in = 96px）
    }
    // ... 更多单位 ...
}
```

#### 3.4.2 颜色解析：`ParseColor()`

```cpp
glm::vec4 SVGParser::ParseColor(const std::string& colorStr) {
    // 支持三种格式：
    
    // 格式1：十六进制颜色
    if (colorStr[0] == '#') {
        // "#FF0000" → (1.0, 0.0, 0.0, 1.0)
        // "#F00"   → (1.0, 0.0, 0.0, 1.0)（缩写形式）
    }

    // 格式2：RGB 函数
    if (colorStr.find("rgb(") == 0) {
        // "rgb(255, 0, 0)" → (1.0, 0.0, 0.0, 1.0)
    }

    // 格式3：颜色名称
    static std::unordered_map<std::string, glm::vec4> colorMap = {
        {"red",     glm::vec4(1, 0, 0, 1)},
        {"blue",    glm::vec4(0, 0, 1, 1)},
        {"green",   glm::vec4(0, 1, 0, 1)},
        // ... 更多颜色 ...
    };
    // "red" → (1.0, 0.0, 0.0, 1.0)
}
```

返回 `glm::vec4(R, G, B, A)`，每个分量范围 [0, 1]

#### 3.4.3 样式解析：`ParseStyle()`

```cpp
SVGStyle SVGParser::ParseStyle(tinyxml2::XMLElement* element) {
    SVGStyle style;

    // 解析填充颜色
    if (HasAttribute(element, "fill")) {
        std::string fill = GetAttribute(element, "fill");
        if (fill != "none") {
            style.fillColor = ParseColor(fill);  // 调用颜色解析
        }
    }

    // 解析描边颜色
    if (HasAttribute(element, "stroke")) {
        std::string stroke = GetAttribute(element, "stroke");
        if (stroke != "none") {
            style.strokeColor = ParseColor(stroke);
        }
    }

    // 解析描边宽度
    if (HasAttribute(element, "stroke-width")) {
        style.strokeWidth = ParseLength(GetAttribute(element, "stroke-width"), 1.0f);
    }

    // 解析透明度相关
    if (HasAttribute(element, "opacity")) {
        style.opacity = std::stof(GetAttribute(element, "opacity"));  // [0, 1]
    }
    if (HasAttribute(element, "fill-opacity")) {
        style.fillOpacity = std::stof(GetAttribute(element, "fill-opacity"));
    }
    if (HasAttribute(element, "stroke-opacity")) {
        style.strokeOpacity = std::stof(GetAttribute(element, "stroke-opacity"));
    }

    return style;
}
```

返回一个 `SVGStyle` 结构体，包含所有样式属性（使用 `std::optional` 表示可选）

#### 3.4.4 变换解析：`ParseTransform()`

```cpp
Transform2D SVGParser::ParseTransform(const std::string& transformStr) {
    // 示例：transformStr = "translate(100,50) rotate(45)"
    
    // 使用正则表达式提取变换函数
    std::regex transformRegex(R"((translate|scale|rotate|matrix)\s*\(([^)]+)\))");
    
    Transform2D result;  // 初始为单位矩阵

    // 对每个匹配的变换函数
    for (auto& match : matches) {
        std::string type = match[1];     // "translate"
        std::string params = match[2];   // "100,50"

        std::vector<float> values = ParseParameters(params);

        if (type == "translate") {
            // T = [1  0  tx]
            //     [0  1  ty]
            //     [0  0  1 ]
            result = Transform2D::Translate(values[0], values[1]) * result;
        } 
        else if (type == "scale") {
            // S = [sx  0  0]
            //     [0  sy  0]
            //     [0  0   1]
            result = Transform2D::Scale(values[0], values[1]) * result;
        }
        else if (type == "rotate") {
            // R = [cos(θ)  -sin(θ)  0]
            //     [sin(θ)   cos(θ)  0]
            //     [0        0       1]
            result = Transform2D::Rotate(glm::radians(values[0])) * result;
        }
        else if (type == "matrix") {
            // M = [a  c  e]
            //     [b  d  f]
            //     [0  0  1]
            result = Transform2D(build_matrix(values)) * result;
        }
    }

    return result;
}
```

支持 4 种变换：平移、缩放、旋转、矩阵

#### 3.4.5 路径数据解析：`ParsePathData()`（最复杂）

```cpp
bool SVGParser::ParsePathData(const std::string& pathData, std::vector<PathCommand>& commands) {
    // 示例路径：d="M 10 10 L 100 100 C 150 0 200 100 200 200 Z"
    //
    // M = MoveTo      （移动到某点，开始新路径）
    // L = LineTo      （线性线段）
    // C = CurveTo     （三次贝塞尔曲线）
    // Q = QuadCurveTo （二次贝塞尔曲线）
    // A = ArcTo       （圆弧）
    // Z = ClosePath   （闭合路径）

    commands.clear();
    
    std::string data = pathData;
    // 移除所有空白字符
    data.erase(std::remove_if(data.begin(), data.end(), ::isspace), data.end());
    // 结果：data = "M10 10L100100C1500200100200200Z"

    size_t i = 0;
    Point2D currentPos(0, 0);   // 当前绘制位置
    char lastCommand = '\0';    // 上一个命令

    while (i < data.size()) {
        char cmd = data[i];
        bool relative = std::islower(cmd);  // 小写 = 相对坐标
        char upperCmd = std::toupper(cmd);

        i++;  // 消耗命令字符

        // 根据命令类型解析参数点数
        switch (upperCmd) {
            case 'M':  // MoveTo - 1个点
                command.type = PathCommandType::MoveTo;
                command.points = ParsePoints(data, i, 1);
                currentPos = relative ? currentPos + command.points[0] 
                                      : command.points[0];
                break;

            case 'L':  // LineTo - 1个点
                command.type = PathCommandType::LineTo;
                command.points = ParsePoints(data, i, 1);
                currentPos = relative ? currentPos + command.points[0] 
                                      : command.points[0];
                break;

            case 'C':  // CurveTo（三次贝塞尔） - 3个点
                command.type = PathCommandType::CurveTo;
                command.points = ParsePoints(data, i, 3);
                // [0]=控制点1, [1]=控制点2, [2]=终点
                currentPos = relative ? currentPos + command.points[2] 
                                      : command.points[2];
                break;

            case 'Q':  // QuadCurveTo（二次贝塞尔） - 2个点
                command.type = PathCommandType::QuadCurveTo;
                command.points = ParsePoints(data, i, 2);
                // [0]=控制点, [1]=终点
                currentPos = relative ? currentPos + command.points[1] 
                                      : command.points[1];
                break;

            case 'Z':  // ClosePath - 无参数
                command.type = PathCommandType::ClosePath;
                break;

            // ... 更多命令类型 ...
        }

        // 保存命令
        if (!command.points.empty() || command.type == PathCommandType::ClosePath) {
            commands.push_back(command);
        }
    }

    return !commands.empty();
}
```

**关键概念：**
- **绝对坐标**（大写字母）：基于画布原点
- **相对坐标**（小写字母）：基于当前位置
- **贝塞尔曲线**：通过控制点定义曲线形状

---

### 3.5 数据结构存储

解析后的数据存储在 `SVGDocument` 中：

```cpp
struct SVGDocument {
    float width = 800.0f;              // 文档宽度
    float height = 600.0f;             // 文档高度
    std::string viewBox;               // 视口属性
    std::vector<SVGElement> elements;  // 所有元素列表
};

struct SVGElement {
    enum Type { Path, Circle, Ellipse, Rect, Line, Text, Group } type;
    
    std::string id;
    SVGStyle style;           // 样式（填充、描边等）
    Transform2D transform;    // 变换矩阵
    std::vector<SVGElement> children;  // 组内的子元素
    
    union {
        SVGPath path;
        SVGCircle circle;
        SVGEllipse ellipse;
        SVGRect rect;
        SVGLine line;
        SVGText text;
    };  // 联合体存储元素特定数据
};
```

**使用联合体的原因：**
- 节省内存：每个 `SVGElement` 只存储一种类型的数据
- 支持多态：通过 `type` 字段判断是哪种类型

---

## 四、第三阶段：SVG 渲染

### 4.1 渲染入口：`CaseSVGRender::UpdateRender()`

```cpp
void CaseSVGRender::UpdateRender() {
    // 使用渲染器将解析后的 SVG 渲染为图像
    Common::ImageRGB image = _renderer.RenderSVG(_svgDocument, _renderWidth, _renderHeight);
    
    // 将像素数据上传到 GPU 纹理
    _texture.SetData(image.GetData(), 0, 0, _renderWidth, _renderHeight);
}
```

---

### 4.2 SVG 文档渲染：`SVGRenderer::RenderSVG()`

```cpp
Common::ImageRGB SVGRenderer::RenderSVG(const SVGDocument& document,
                                       std::uint32_t width,
                                       std::uint32_t height) {
    // 步骤1：创建指定尺寸的图像
    Common::ImageRGB image(width, height);

    // 步骤2：用背景色填充
    glm::vec3 bgColor(_backgroundColor.r, _backgroundColor.g, _backgroundColor.b);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            image.At(x, y) = bgColor;
        }
    }

    // 步骤3：逐个渲染所有元素
    for (const auto& element : document.elements) {
        RenderElement(element, image);
    }

    return image;  // 返回渲染后的图像
}
```

**流程：**
1. 创建目标图像
2. 初始化为背景色
3. 遍历所有元素，逐个渲染

---

### 4.3 单个元素渲染：`RenderElement()`

```cpp
void SVGRenderer::RenderElement(const SVGElement& element,
                               Common::ImageRGB& targetImage) {
    // 根据元素类型调用不同的渲染函数
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
            // 递归渲染组内的所有子元素
            for (const auto& child : element.children) {
                RenderElement(child, targetImage);
            }
            break;
    }
}
```

---

### 4.4 具体元素渲染示例

#### 4.4.1 圆形渲染：`RenderCircle()`

```cpp
void SVGRenderer::RenderCircle(const SVGCircle& circle, Common::ImageRGB& image) {
    // 步骤1：应用变换
    Point2D center = circle.transform.TransformPoint(circle.center);

    // 步骤2：计算填充颜色（应用透明度）
    glm::vec4 fillColor(0, 0, 0, 0);  // 初始透明
    if (circle.style.fillColor) {
        fillColor = *circle.style.fillColor;
        if (circle.style.fillOpacity) {
            fillColor.a *= *circle.style.fillOpacity;
        }
        if (circle.style.opacity) {
            fillColor.a *= *circle.style.opacity;
        }
    }

    // 步骤3：计算描边颜色
    glm::vec4 strokeColor(0, 0, 0, 0);  // 初始透明
    if (circle.style.strokeColor) {
        strokeColor = *circle.style.strokeColor;
        if (circle.style.strokeOpacity) {
            strokeColor.a *= *circle.style.strokeOpacity;
        }
        if (circle.style.opacity) {
            strokeColor.a *= *circle.style.opacity;
        }
    }

    // 步骤4：渲染填充（如果有填充色）
    if (fillColor.a > 0) {
        DrawCircle(image, center, circle.radius, fillColor, true);
    }

    // 步骤5：渲染描边（如果有描边色和宽度）
    if (strokeColor.a > 0 && circle.style.strokeWidth) {
        DrawCircle(image, center, circle.radius, strokeColor, false);
    }
}
```

**流程：**
1. 对圆心应用变换矩阵
2. 根据样式信息计算有效的填充和描边颜色
3. 如果有填充色，渲染填充圆
4. 如果有描边，渲染圆形轮廓

#### 4.4.2 矩形渲染：`RenderRect()`

```cpp
void SVGRenderer::RenderRect(const SVGRect& rect, Common::ImageRGB& image) {
    // 应用变换
    Point2D position = rect.transform.TransformPoint(rect.position);

    // 计算颜色（同圆形）
    glm::vec4 fillColor = ...;
    glm::vec4 strokeColor = ...;

    // 渲染填充
    if (fillColor.a > 0) {
        DrawRect(image, position, rect.width, rect.height, 
                 fillColor, true, rect.rx, rect.ry);  // rx, ry = 圆角半径
    }

    // 渲染描边
    if (strokeColor.a > 0 && rect.style.strokeWidth) {
        DrawRect(image, position, rect.width, rect.height,
                 strokeColor, false, rect.rx, rect.ry);
    }
}
```

#### 4.4.3 路径渲染：`RenderPath()`（最复杂）

```cpp
void SVGRenderer::RenderPath(const SVGPath& path, Common::ImageRGB& image) {
    // 步骤1：将路径命令序列转换为顶点列表
    auto vertices = path.GetVertices();

    if (vertices.empty()) return;

    // 步骤2：计算填充和描边颜色
    glm::vec4 fillColor = ...;
    glm::vec4 strokeColor = ...;
    float strokeWidth = path.style.strokeWidth.value_or(1.0f);

    // 步骤3：检查路径是否闭合
    bool closed = false;
    if (!path.commands.empty() && path.commands.back().type == PathCommandType::ClosePath) {
        closed = true;
    }

    // 步骤4：绘制路径（填充和描边）
    DrawPath(image, vertices, fillColor, strokeColor, strokeWidth, closed);
}
```

**关键函数：`GetVertices()`**
- 将路径命令序列（M, L, C, Q 等）转换为像素级的顶点序列
- 对贝塞尔曲线进行细分，生成近似的直线段
- 返回一个 `Point2D` 向量

---

### 4.5 基础绘图函数

#### 4.5.1 直线绘制：`DrawLine()`

```cpp
void SVGRenderer::DrawLine(Common::ImageRGB& image,
                          const Point2D& start, const Point2D& end,
                          const glm::vec4& color, float width = 1.0f) {
    // 使用 Bresenham 算法或其他直线光栅化算法
    int x1 = (int)start.x, y1 = (int)start.y;
    int x2 = (int)end.x,   y2 = (int)end.y;

    BresenhamLine(image, x1, y1, x2, y2, color);

    // 如果宽度 > 1，需要多次绘制以获得更粗的线
    if (width > 1.0f) {
        // ... 额外的绘制逻辑 ...
    }
}
```

#### 4.5.2 圆形绘制：`DrawCircle()`

```cpp
void SVGRenderer::DrawCircle(Common::ImageRGB& image,
                            const Point2D& center, float radius,
                            const glm::vec4& color, bool filled = true) {
    int cx = (int)center.x;
    int cy = (int)center.y;
    int r = (int)radius;

    if (filled) {
        // 填充圆：用中点圆算法扫描所有像素
        for (int y = cy - r; y <= cy + r; y++) {
            for (int x = cx - r; x <= cx + r; x++) {
                float dx = x - cx;
                float dy = y - cy;
                if (dx*dx + dy*dy <= r*r) {  // 在圆内
                    SetPixel(image, x, y, color);
                }
            }
        }
    } else {
        // 圆形轮廓：只绘制边界附近的像素
        for (int y = cy - r; y <= cy + r; y++) {
            for (int x = cx - r; x <= cx + r; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = dx*dx + dy*dy;
                if (dist >= (r-1)*(r-1) && dist <= r*r) {  // 在圆周附近
                    SetPixel(image, x, y, color);
                }
            }
        }
    }
}
```

#### 4.5.3 像素设置：`SetPixel()` 和 `BlendPixel()`

```cpp
void SVGRenderer::SetPixel(Common::ImageRGB& image, int x, int y, 
                          const glm::vec4& color) {
    // 检查边界
    if (x < 0 || x >= image.GetWidth() || y < 0 || y >= image.GetHeight()) {
        return;
    }

    // 将 [0,1] 范围的 RGBA 颜色转换为 [0,255] 范围的 RGB
    glm::vec3 rgbColor = glm::vec3(color.r, color.g, color.b) * 255.0f;
    
    // 根据 alpha 值混合（简单的透明度处理）
    if (color.a >= 1.0f) {
        image.At(x, y) = rgbColor;
    } else if (color.a > 0.0f) {
        // Alpha 混合：新颜色 = 原颜色 * (1-α) + 新颜色 * α
        glm::vec3 oldColor = image.At(x, y);
        image.At(x, y) = oldColor * (1.0f - color.a) + rgbColor * color.a;
    }
}
```

---

## 五、完整渲染流程总结

```
┌─────────────────────────────────────────────────────────────┐
│  1. SVG 文件加载                                             │
│  CaseSVGRender::LoadSVGFile()                               │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│  2. XML 文件解析（使用 tinyxml2）                           │
│  SVGParser::ParseFile()                                     │
│  → 加载 XML 文档                                             │
│  → 找到 <svg> 根元素                                        │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│  3. SVG 结构解析                                             │
│  SVGParser::ParseSVGElement()                               │
│  → 解析文档属性（width, height, viewBox）                  │
│  → 遍历子元素，识别类型                                     │
│  → 解析每个元素的几何属性                                   │
└──────────────────┬──────────────────────────────────────────┘
                   │
       ┌───────────┼───────────┬──────────────┐
       ▼           ▼           ▼              ▼
    Circle      Rect        Path           Line
    │           │           │              │
    └───────────┼───────────┼──────────────┘
                │
    ┌───────────┴────────────┐
    │ ParseStyle()           │ ParseTransform()
    │ ├─ fillColor          │ ├─ translate()
    │ ├─ strokeColor        │ ├─ scale()
    │ ├─ strokeWidth        │ ├─ rotate()
    │ ├─ opacity            │ └─ matrix()
    │ └─ ...                │
    └───────────┬────────────┘
                │
                ▼
┌─────────────────────────────────────────────────────────────┐
│  4. 数据存储                                                 │
│  SVGDocument                                                │
│  ├─ width, height, viewBox                                  │
│  └─ elements[] → SVGElement[]                               │
│       ├─ type, id, style, transform                         │
│       └─ union { SVGPath, SVGCircle, ... }                  │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│  5. 图像渲染                                                 │
│  SVGRenderer::RenderSVG()                                   │
│  → 创建图像缓冲（ImageRGB）                                 │
│  → 填充背景色                                               │
│  → 遍历所有元素                                             │
└──────────────────┬──────────────────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    ▼              ▼              ▼
RenderCircle   RenderRect    RenderPath
    │              │              │
    │              │              ▼
    │              │         GetVertices()
    │              │         (转换为顶点序列)
    │              │              │
    └──────────────┼──────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│  6. 基础绘图                                                 │
│  ├─ DrawCircle()    (中点圆算法)                            │
│  ├─ DrawRect()      (直线填充)                              │
│  ├─ DrawLine()      (Bresenham 算法)                        │
│  ├─ DrawPath()      (多边形填充)                            │
│  └─ SetPixel()      (像素混合)                              │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│  7. 输出结果                                                 │
│  ImageRGB → OpenGL 纹理 → 屏幕显示                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、关键数据结构

### 6.1 变换矩阵：`Transform2D`

```cpp
struct Transform2D {
    glm::mat3 matrix;  // 3x3 变换矩阵

    // 静态工厂方法
    static Transform2D Translate(float tx, float ty);
    static Transform2D Scale(float sx, float sy);
    static Transform2D Rotate(float angle);

    // 应用变换：输入点 → 输出点
    Point2D TransformPoint(const Point2D& p) const;
};
```

变换矩阵：
$$\begin{pmatrix} m_{00} & m_{01} & m_{20} \\ m_{10} & m_{11} & m_{21} \\ 0 & 0 & 1 \end{pmatrix} \times \begin{pmatrix} x \\ y \\ 1 \end{pmatrix} = \begin{pmatrix} x' \\ y' \\ 1 \end{pmatrix}$$

### 6.2 样式：`SVGStyle`

```cpp
struct SVGStyle {
    std::optional<glm::vec4> fillColor;        // 填充颜色
    std::optional<glm::vec4> strokeColor;      // 描边颜色
    std::optional<float>     strokeWidth;      // 描边宽度
    std::optional<float>     opacity;          // 总透明度
    std::optional<float>     fillOpacity;      // 填充透明度
    std::optional<float>     strokeOpacity;    // 描边透明度
    std::optional<std::string> fillRule;       // "evenodd" 或 "nonzero"
};
```

---

## 七、性能优化点

1. **路径细分缓存**：贝塞尔曲线的细分结果可以缓存
2. **扫描线算法**：多边形填充可用扫描线填充优化
3. **抗锯齿**：对边界进行 MSAA 或 SSAA 处理
4. **分层渲染**：Z-order 排序后批量渲染

---

## 八、总结

SVG 渲染流程的三个核心阶段：

| 阶段 | 输入 | 处理 | 输出 |
|------|------|------|------|
| **解析** | SVG 文件/字符串 | XML 解析 + 属性提取 | SVGDocument 结构体 |
| **存储** | 解析结果 | 类型识别 + 数据组织 | 元素树（包含联合体） |
| **渲染** | SVGDocument | 遍历元素 + 光栅化 | 像素图像 |

每一步都通过明确的函数接口实现，使得代码易于维护和扩展。
