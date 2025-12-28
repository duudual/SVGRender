# SVG渲染器新功能说明

## 已实现的三大功能

本次更新为SVG渲染器添加了三个重要的交互式功能，让其更像专业的SVG编辑器。

---

## 功能一：完善的贝塞尔曲线绘制

### 实现原理

之前的实现简单地将贝塞尔曲线的控制点和端点用直线连接，现在使用**参数化采样**的方法生成平滑曲线。

#### 三次贝塞尔曲线（Cubic Bezier）

**数学公式：**
```
B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
其中 t ∈ [0, 1]
```

**实现细节：**
- 将曲线分成 **20 段**
- 对每个 t 值计算曲线上的点坐标
- 将这些点连接形成近似的平滑曲线

```cpp
// 采样示例
const int segments = 20;
for (int i = 1; i <= segments; i++) {
    float t = i / 20.0f;
    // 计算 B(t)
    Point2D point = cubicBezier(p0, p1, p2, p3, t);
    vertices.push_back(point);
}
```

#### 二次贝塞尔曲线（Quadratic Bezier）

**数学公式：**
```
B(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
```

**实现细节：**
- 将曲线分成 **15 段**
- 使用更简单的二次公式计算

### 使用示例

SVG 路径命令：
```svg
<!-- 三次贝塞尔曲线 -->
<path d="M 10 80 C 40 10, 65 10, 95 80" stroke="blue" fill="none"/>

<!-- 二次贝塞尔曲线 -->
<path d="M 10 80 Q 52.5 10, 95 80" stroke="red" fill="none"/>
```

### 效果对比

| 之前 | 现在 |
|------|------|
| 直线连接控制点 | 平滑的曲线 |
| 折线效果 | 20段采样的近似曲线 |
| 不符合SVG标准 | 符合标准的贝塞尔渲染 |

---

## 功能二：双向交互式高亮系统

### 核心特性

类似于浏览器开发者工具的"检查元素"功能，实现了**图像与代码的双向高亮联动**。

### 1. 鼠标悬停在图像上

**效果：**
- 鼠标移动到任何SVG元素上时，该元素会显示**黄色高亮边框**
- UI面板显示元素的详细信息：
  - 元素类型（circle, rect, path等）
  - 元素ID（如果有）
  - 边界框坐标

**实现原理：**

```cpp
// 1. 计算每个元素的边界框
struct ElementBounds {
    int elementIndex;
    float minX, minY, maxX, maxY;
    std::string id;
    std::string tagName;
};

// 2. 鼠标位置检测（在 OnProcessInput 中）
void OnProcessInput(ImVec2 const & pos) {
    int hoveredIndex = FindElementAtPosition(pos.x, pos.y);
    if (hoveredIndex != _hoveredElementIndex) {
        _hoveredElementIndex = hoveredIndex;
        RenderWithHighlight(hoveredIndex);  // 重新渲染
    }
}

// 3. 绘制高亮边框
void RenderWithHighlight(int highlightIndex) {
    // 正常渲染
    _image = _svgRenderer.RenderSVG(_svgDocument, ...);
    
    // 绘制黄色半透明边框
    if (highlightIndex >= 0) {
        DrawHighlightBorder(bounds, glm::vec4(1, 1, 0, 0.5));
    }
}
```

### 2. 元素边界计算

针对不同元素类型使用不同的边界计算方法：

| 元素类型 | 边界计算方法 |
|---------|------------|
| **Circle** | 中心点 ± 半径 |
| **Rect** | 左上角 + 宽度/高度 |
| **Line** | 起点和终点的最小/最大值 |
| **Path** | 遍历所有顶点找最小/最大坐标 |
| **Ellipse** | 中心点 ± rx/ry |

### 3. 显示效果

```
┌─────────────────────────────┐
│  Hovered Element:          │
│    Tag: circle             │
│    ID: circle1             │
│    Bounds: (160, 60) to    │
│            (240, 140)      │
└─────────────────────────────┘
```

---

## 功能三：实时SVG文本编辑器

### 核心特性

在应用程序内部直接编辑SVG源代码，点击"Render"按钮即可实时预览修改效果。

### UI 布局

```
┌─────────────────────────────────────────┐
│  SVG Path: [assets/test.svg] [Load]   │
│  ☑ Show Editor                          │
├─────────────────────────────────────────┤
│  SVG Source Editor:                     │
│  ┌───────────────────────────────────┐  │
│  │ <svg width="400" height="300">   │  │
│  │   <rect x="50" y="50"            │  │
│  │         width="100" height="80"  │  │
│  │         fill="red"/>             │  │
│  │   <circle cx="200" cy="100"      │  │
│  │           r="40" fill="blue"/>   │  │
│  │ </svg>                           │  │
│  └───────────────────────────────────┘  │
│  [Render]                               │
│  Tip: Edit the SVG and click 'Render'  │
└─────────────────────────────────────────┘
```

### 使用流程

1. **加载SVG文件**
   - 点击 "Load" 按钮加载文件
   - 文件内容自动显示在编辑器中

2. **编辑SVG代码**
   - 在多行文本框中直接修改SVG标签
   - 支持Tab键缩进
   - 最大支持 64KB 的文本

3. **实时预览**
   - 点击 "Render" 按钮
   - 系统解析新的SVG代码
   - 立即显示渲染结果

### 实现细节

```cpp
// 1. 文本缓冲区（64KB）
char _svgTextBuffer[65536];

// 2. 多行文本编辑器
ImGui::InputTextMultiline("##SVGEditor", 
                         _svgTextBuffer, 
                         sizeof(_svgTextBuffer), 
                         ImVec2(-1, 300),  // 300像素高度
                         ImGuiInputTextFlags_AllowTabInput);

// 3. 点击Render时更新
if (ImGui::Button("Render")) {
    UpdateSVGFromText();  // 解析文本
    UpdateRender();       // 重新渲染
}

// 4. 解析更新
void UpdateSVGFromText() {
    std::string newContent(_svgTextBuffer);
    SVGDocument newDocument;
    if (_svgParser.ParseString(newContent, newDocument)) {
        _svgDocument = std::move(newDocument);
        UpdateElementBounds();  // 更新边界信息
    }
}
```

### 错误处理

- 如果SVG语法错误，解析失败时会在控制台输出错误信息
- 原有的渲染结果保持不变
- 不会因为错误的输入而崩溃

---

## 使用示例

### 1. 测试贝塞尔曲线

在编辑器中输入：

```svg
<svg width="400" height="300" xmlns="http://www.w3.org/2000/svg">
  <!-- 三次贝塞尔曲线 - S形曲线 -->
  <path id="cubic1" 
        d="M 50 150 C 50 50, 350 50, 350 150" 
        stroke="blue" 
        stroke-width="3" 
        fill="none"/>
  
  <!-- 二次贝塞尔曲线 - 抛物线 -->
  <path id="quad1" 
        d="M 50 200 Q 200 100, 350 200" 
        stroke="red" 
        stroke-width="3" 
        fill="none"/>
</svg>
```

点击 "Render"，你会看到两条平滑的曲线。

### 2. 测试交互式高亮

```svg
<svg width="400" height="300" xmlns="http://www.w3.org/2000/svg">
  <rect id="rect1" x="50" y="50" width="100" height="80" 
        fill="red" stroke="black" stroke-width="2"/>
  <circle id="circle1" cx="200" cy="100" r="40" 
          fill="blue" stroke="green" stroke-width="3"/>
  <path id="triangle" d="M 250 200 L 350 200 L 300 250 Z" 
        fill="yellow" stroke="orange" stroke-width="2"/>
</svg>
```

- 将鼠标移动到红色矩形上，会看到黄色边框
- UI面板显示："Tag: rect, ID: rect1"
- 移动到蓝色圆圈，高亮自动切换

### 3. 实时编辑测试

1. 加载上面的SVG
2. 在编辑器中修改 `fill="red"` 为 `fill="green"`
3. 点击 "Render"
4. 矩形颜色立即变为绿色

---

## 技术架构

### 数据流

```
SVG文件/文本
    ↓
SVGParser::Parse()
    ↓
SVGDocument (内存结构)
    ↓
UpdateElementBounds() → ElementBounds[]
    ↓
SVGRenderer::RenderSVG()
    ↓
ImageRGB (像素数组)
    ↓
OpenGL Texture → 屏幕显示
    ↑
OnProcessInput() ← 鼠标输入
    ↓
FindElementAtPosition()
    ↓
RenderWithHighlight()
```

### 关键数据结构

```cpp
// 元素边界信息
struct ElementBounds {
    int elementIndex;      // 在elements数组中的索引
    float minX, minY;      // 左上角
    float maxX, maxY;      // 右下角
    std::string id;        // 元素ID
    std::string tagName;   // 元素类型
};

// 应用程序状态
class CaseSVGRender {
    SVGDocument _svgDocument;           // 解析后的文档
    char _svgTextBuffer[65536];         // 文本编辑器缓冲
    std::vector<ElementBounds> _elementBounds;  // 所有元素边界
    int _hoveredElementIndex;           // 当前悬停的元素
    bool _textEditorVisible;            // 编辑器可见性
};
```

---

## 性能考虑

### 贝塞尔曲线细分

- **三次曲线**：20段 → 每条曲线20个顶点
- **二次曲线**：15段 → 每条曲线15个顶点
- 如果路径很复杂，可能需要更多采样点以获得更平滑的效果

**调整方法：**
```cpp
// 在 SVG.cpp 中修改
const int segments = 30;  // 增加到30段（更平滑但更慢）
```

### 高亮渲染优化

每次鼠标移动都会触发重新渲染，对于复杂的SVG可能有性能影响。

**优化建议：**
1. 添加鼠标移动的防抖（debounce）
2. 只在鼠标停止移动一段时间后才高亮
3. 缓存渲染结果，只重绘高亮边框

### 文本编辑器

- 最大文本大小：64KB
- 如果需要处理更大的文件，增加 `_svgTextBuffer` 的大小

---

## 已知限制

1. **贝塞尔曲线**
   - 不支持圆弧命令（ArcTo）的完整实现
   - 采样点数量固定（20和15段）

2. **高亮系统**
   - 只能检测到边界框内的点击，不是精确的形状内部检测
   - 对于复杂路径，边界框可能不够精确

3. **文本编辑器**
   - 没有语法高亮
   - 没有自动补全
   - 错误提示只在控制台显示

---

## 未来改进方向

### 短期改进

1. **精确的形状碰撞检测**
   - 使用光线投射算法判断点是否在多边形内部
   - 对圆形和椭圆使用精确的数学公式

2. **文本编辑器增强**
   - 添加语法高亮（XML/SVG）
   - 行号显示
   - 查找/替换功能

3. **撤销/重做**
   - 保存编辑历史
   - Ctrl+Z 撤销功能

### 长期改进

1. **节点编辑模式**
   - 点击路径显示控制点
   - 拖动控制点实时修改形状
   - 类似于 Inkscape 的节点工具

2. **图层系统**
   - 元素列表视图
   - 拖拽改变绘制顺序
   - 显示/隐藏单个元素

3. **变换工具**
   - 平移、缩放、旋转的可视化控制
   - 变换矩阵的实时预览

---

## 总结

本次更新显著提升了SVG渲染器的交互性和实用性：

| 功能 | 改进 | 用户体验 |
|-----|------|---------|
| **贝塞尔曲线** | 从折线变为平滑曲线 | ⭐⭐⭐⭐⭐ |
| **交互式高亮** | 像浏览器开发者工具 | ⭐⭐⭐⭐⭐ |
| **实时编辑** | 所见即所得的编辑体验 | ⭐⭐⭐⭐⭐ |

这些功能使得 SVGRender 从一个简单的渲染器，进化为一个可交互的SVG编辑预览工具。
