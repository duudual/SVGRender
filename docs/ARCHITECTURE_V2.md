# SVG Renderer v2.0 - High-Quality Architecture Blueprint

## 概述

本文档定义了将当前 CPU 软件光栅化 SVG 渲染器升级为"接近浏览器标准"的高质量渲染器的完整架构设计。

## 1. 完整渲染流水线

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SVG Rendering Pipeline                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌────────┐ │
│  │  Parse   │───▶│  Scene   │───▶│ Geometry │───▶│ Rasterize│───▶│Compose │ │
│  │  Stage   │    │  Build   │    │  Stage   │    │  Stage   │    │ Stage  │ │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘    └────────┘ │
│       │               │               │               │               │      │
│       ▼               ▼               ▼               ▼               ▼      │
│   XML → DOM      Scene Graph    Path → Edges     Scanline +     Alpha Blend │
│   CSS Cascade    Transform Stack Tessellation    Coverage AA    Compositing │
│   Style Inherit  Clip/Mask Prep  Stroke Expand   Fill Rules     Layer Merge │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2. 模块架构

### 2.1 核心模块划分

```
src/VCX/Labs/svg/
├── Core/                      # 核心数学和工具库
│   ├── Math2D.h              # Vec2, Point2D, BBox, Transform
│   ├── Bezier.h              # 贝塞尔曲线工具
│   ├── ColorSpace.h          # 颜色空间转换
│   └── MemoryPool.h          # 内存池分配器
│
├── Parser/                    # SVG 解析层
│   ├── SVGParser.h           # XML 解析入口
│   ├── PathParser.h          # Path d 属性解析
│   ├── StyleParser.h         # CSS 样式解析
│   └── TransformParser.h     # transform 属性解析
│
├── Scene/                     # 场景图层
│   ├── SVGDocument.h         # 文档根节点
│   ├── SVGElement.h          # 元素基类
│   ├── RenderTree.h          # 渲染树构建
│   └── TransformStack.h      # 变换矩阵栈
│
├── Geometry/                  # 几何处理层
│   ├── Path.h                # 路径表示
│   ├── PathBuilder.h         # 路径构建器
│   ├── BezierTessellator.h   # 自适应贝塞尔细分
│   ├── StrokeExpander.h      # 描边扩展器
│   ├── Triangulator.h        # 三角化器
│   └── ClipPath.h            # 裁剪路径
│
├── Rasterizer/               # 光栅化层
│   ├── EdgeTable.h           # 边表数据结构
│   ├── ScanlineRasterizer.h  # 扫描线光栅化
│   ├── CoverageAA.h          # 覆盖率抗锯齿
│   ├── FillRule.h            # 填充规则
│   └── TileRasterizer.h      # 分块光栅化 (可选 GPU)
│
├── Paint/                     # 绘制样式层
│   ├── Paint.h               # 绘制样式基类
│   ├── SolidColor.h          # 纯色
│   ├── LinearGradient.h      # 线性渐变
│   ├── RadialGradient.h      # 径向渐变
│   ├── Pattern.h             # 图案填充
│   └── Sampler.h             # 采样器
│
├── Text/                      # 文本渲染层
│   ├── FontManager.h         # 字体管理器
│   ├── GlyphCache.h          # 字形缓存
│   ├── TextShaper.h          # 文本排版
│   └── SDFRenderer.h         # SDF 字体渲染
│
├── Compositor/               # 合成层
│   ├── Layer.h               # 图层
│   ├── BlendMode.h           # 混合模式
│   ├── Filter.h              # 滤镜基类
│   ├── GaussianBlur.h        # 高斯模糊
│   └── Compositor.h          # 图层合成器
│
└── Renderer/                  # 渲染器入口
    ├── SVGRenderer.h         # 主渲染器接口
    ├── CPURenderContext.h    # CPU 渲染上下文
    └── GPURenderContext.h    # GPU 渲染上下文 (可选)
```

### 2.2 改造策略

| 模块 | 当前状态 | 改造方式 | 优先级 |
|------|----------|----------|--------|
| Parser | 基本可用 | 增量改进 | 中 |
| Transform | 简单矩阵 | 增量改进 (加栈) | 高 |
| Path | 固定细分 | **重写** | 高 |
| Stroke | 无实现 | **新增** | 高 |
| Fill | 简单 Even-Odd | 增量改进 | 高 |
| Rasterizer | Bresenham | **重写** | 高 |
| AA | 无 | **新增** | 高 |
| Gradient | 无 | **新增** | 中 |
| Text | 位图字体 | **重写** | 中 |
| Filter | 无 | **新增** | 低 |

## 3. CPU / GPU 职责边界

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CPU 职责域                                    │
├─────────────────────────────────────────────────────────────────────┤
│ • SVG 解析 & 样式计算                                                │
│ • 场景图构建 & 变换矩阵栈计算                                         │
│ • 路径细分 (Bézier → Line Segments)                                 │
│ • 描边扩展 (Stroke → Fill Path)                                     │
│ • 边表构建 (Active Edge Table)                                      │
│ • 高质量文本字形生成 (via FreeType)                                  │
└─────────────────────────────────────────────────────────────────────┘
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
            ┌───────────┐ ┌───────────┐ ┌───────────┐
            │  CPU 模式 │ │  混合模式  │ │  GPU 模式  │
            │ (当前实现) │ │ (推荐升级) │ │  (高级)    │
            └───────────┘ └───────────┘ └───────────┘
                 │              │              │
                 ▼              ▼              ▼
┌────────────────────┐ ┌────────────────┐ ┌────────────────────┐
│ CPU 扫描线光栅化    │ │ CPU 边准备 +   │ │ GPU Tessellation + │
│ CPU 覆盖率 AA       │ │ GPU 填充/混合  │ │ GPU Rasterization  │
│ CPU Alpha Blend    │ │ GPU 渐变采样   │ │ GPU Compute Blur   │
└────────────────────┘ └────────────────┘ └────────────────────────┘
```

## 4. Path 渲染改进详解

### 4.1 路径命令处理

```cpp
// 路径命令枚举 (完整 SVG spec)
enum class PathCommand : uint8_t {
    MoveTo,         // M/m
    LineTo,         // L/l, H/h, V/v
    CubicTo,        // C/c, S/s (smooth)
    QuadTo,         // Q/q, T/t (smooth)
    ArcTo,          // A/a
    Close           // Z/z
};
```

### 4.2 自适应贝塞尔细分

基于 **flatness error** 的递归细分算法：

```cpp
// 核心思想：如果曲线足够"平"，用直线代替
// 平坦度误差 = 控制点到弦的最大距离

float CubicFlatnessError(Point2D p0, Point2D p1, Point2D p2, Point2D p3) {
    // 计算控制点 p1, p2 到直线 p0-p3 的距离
    Vec2 d = p3 - p0;
    float len = d.Length();
    if (len < 1e-6f) return (p1 - p0).Length() + (p2 - p0).Length();
    
    Vec2 n = Vec2(-d.y, d.x) / len;  // 法向量
    float d1 = std::abs(Dot(p1 - p0, n));
    float d2 = std::abs(Dot(p2 - p0, n));
    return std::max(d1, d2);
}

void SubdivideCubic(Point2D p0, Point2D p1, Point2D p2, Point2D p3,
                    float tolerance, std::vector<Point2D>& output) {
    if (CubicFlatnessError(p0, p1, p2, p3) <= tolerance) {
        output.push_back(p3);  // 足够平，直接输出终点
        return;
    }
    
    // de Casteljau 细分 at t=0.5
    Point2D p01 = (p0 + p1) * 0.5f;
    Point2D p12 = (p1 + p2) * 0.5f;
    Point2D p23 = (p2 + p3) * 0.5f;
    Point2D p012 = (p01 + p12) * 0.5f;
    Point2D p123 = (p12 + p23) * 0.5f;
    Point2D p0123 = (p012 + p123) * 0.5f;  // 曲线中点
    
    SubdivideCubic(p0, p01, p012, p0123, tolerance, output);
    SubdivideCubic(p0123, p123, p23, p3, tolerance, output);
}
```

### 4.3 填充规则

```cpp
// Winding Number 计算
int ComputeWindingNumber(float x, float y, const std::vector<Edge>& edges) {
    int winding = 0;
    for (const auto& edge : edges) {
        if (edge.CrossesHorizontalRay(y)) {
            float xIntersect = edge.XAtY(y);
            if (xIntersect > x) {
                winding += edge.IsUpward() ? 1 : -1;
            }
        }
    }
    return winding;
}

// 填充规则判断
bool ShouldFill(int winding, FillRule rule) {
    switch (rule) {
        case FillRule::EvenOdd:  return (winding & 1) != 0;
        case FillRule::NonZero:  return winding != 0;
    }
}
```

## 5. Stroke（描边）系统

### 5.1 Line Cap 实现

```cpp
void GenerateLineCap(Point2D point, Vec2 direction, float halfWidth,
                     LineCap cap, std::vector<Point2D>& output) {
    Vec2 perp(-direction.y, direction.x);
    Point2D left = point + perp * halfWidth;
    Point2D right = point - perp * halfWidth;
    
    switch (cap) {
        case LineCap::Butt:
            // 无额外顶点
            output.push_back(left);
            output.push_back(right);
            break;
            
        case LineCap::Square:
            // 向外延伸 halfWidth
            output.push_back(left + direction * halfWidth);
            output.push_back(right + direction * halfWidth);
            break;
            
        case LineCap::Round:
            // 生成半圆弧
            GenerateArc(point, halfWidth, left, right, output);
            break;
    }
}
```

### 5.2 Line Join 实现

```cpp
void GenerateLineJoin(Point2D point, Vec2 inDir, Vec2 outDir,
                      float halfWidth, LineJoin join, float miterLimit,
                      std::vector<Point2D>& output) {
    Vec2 inPerp(-inDir.y, inDir.x);
    Vec2 outPerp(-outDir.y, outDir.x);
    
    // 判断是凸侧还是凹侧
    float cross = Cross(inDir, outDir);
    bool isLeftTurn = cross > 0;
    
    switch (join) {
        case LineJoin::Miter: {
            // 计算 miter 交点
            float cosHalfAngle = Dot(inPerp, outPerp);
            float miterLength = halfWidth / std::max(cosHalfAngle, 0.01f);
            
            if (miterLength / halfWidth > miterLimit) {
                // 超过限制，退化为 Bevel
                GenerateBevelJoin(...);
            } else {
                Point2D miterPoint = point + (inPerp + outPerp).Normalized() * miterLength;
                output.push_back(miterPoint);
            }
            break;
        }
        
        case LineJoin::Round:
            GenerateArc(point, halfWidth, 
                       point + inPerp * halfWidth, 
                       point + outPerp * halfWidth, output);
            break;
            
        case LineJoin::Bevel:
            output.push_back(point + inPerp * halfWidth);
            output.push_back(point + outPerp * halfWidth);
            break;
    }
}
```

## 6. 抗锯齿光栅化

### 6.1 Coverage-based AA

```cpp
// 每个像素计算 coverage (0.0 ~ 1.0)
float ComputePixelCoverage(int px, int py, const std::vector<Edge>& edges) {
    // 子像素采样 (4x4 或 8x8 grid)
    const int SAMPLES = 16;
    int hitCount = 0;
    
    for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
            float x = px + (sx + 0.5f) / 4.0f;
            float y = py + (sy + 0.5f) / 4.0f;
            if (IsPointInside(x, y, edges)) {
                hitCount++;
            }
        }
    }
    
    return float(hitCount) / SAMPLES;
}
```

### 6.2 Analytical Edge AA

```cpp
// 精确计算边缘覆盖面积
float AnalyticalEdgeCoverage(float x0, float y0, float x1, float y1,
                              int px, int py) {
    // 像素边界
    float left = px, right = px + 1;
    float top = py, bottom = py + 1;
    
    // 裁剪线段到像素范围
    // 计算线段与像素框的交集面积
    // 使用 Shoelace 公式计算多边形面积
    // ...
}
```

## 7. 字体渲染系统

### 7.1 FreeType 集成

```cpp
class FontManager {
public:
    bool LoadFont(const std::string& path);
    
    // 获取字形轮廓 (用于 CPU 填充)
    Path GetGlyphOutline(char32_t codepoint, float fontSize);
    
    // 获取 SDF 纹理 (用于 GPU 渲染)
    SDFGlyph GetGlyphSDF(char32_t codepoint, float padding = 4.0f);
    
    // 文本度量
    TextMetrics MeasureText(const std::u32string& text, float fontSize);
    
private:
    FT_Library ftLibrary;
    std::map<std::string, FT_Face> loadedFonts;
    GlyphCache glyphCache;
};
```

### 7.2 两种渲染路径

```
┌─────────────────────────────────────────────────────────────────┐
│                    Text Rendering Paths                          │
├────────────────────────────┬────────────────────────────────────┤
│      CPU Outline Fill      │        GPU SDF Rendering           │
├────────────────────────────┼────────────────────────────────────┤
│ 1. FreeType 提取轮廓        │ 1. 预生成 SDF atlas               │
│ 2. 贝塞尔细分               │ 2. GPU 着色器采样                 │
│ 3. 扫描线填充               │ 3. smoothstep 抗锯齿              │
│                            │                                    │
│ 优点: 任意缩放无损          │ 优点: 高性能，GPU 加速             │
│ 缺点: CPU 密集              │ 缺点: 需要预处理                   │
└────────────────────────────┴────────────────────────────────────┘
```

## 8. 渐变和图案系统

### 8.1 线性渐变采样

```cpp
Color SampleLinearGradient(const LinearGradient& grad, Point2D point) {
    // 计算在渐变轴上的投影位置
    Vec2 axis = grad.end - grad.start;
    float t = Dot(point - grad.start, axis) / Dot(axis, axis);
    
    // 应用 spreadMethod
    switch (grad.spreadMethod) {
        case SpreadMethod::Pad:
            t = std::clamp(t, 0.0f, 1.0f);
            break;
        case SpreadMethod::Repeat:
            t = t - std::floor(t);
            break;
        case SpreadMethod::Reflect:
            t = std::abs(std::fmod(t, 2.0f));
            if (t > 1.0f) t = 2.0f - t;
            break;
    }
    
    // 在 color stops 之间插值
    return InterpolateColorStops(grad.stops, t);
}
```

### 8.2 径向渐变采样

```cpp
Color SampleRadialGradient(const RadialGradient& grad, Point2D point) {
    // 计算到中心的距离比例
    float dist = (point - grad.center).Length();
    float t = dist / grad.radius;
    
    // 应用 focal point 偏移 (如果有)
    if (grad.hasFocalPoint) {
        // 使用焦点到采样点连线与圆的交点
        t = ComputeFocalGradientT(grad, point);
    }
    
    // spreadMethod 和插值同上
    return InterpolateColorStops(grad.stops, ClampSpread(t, grad.spreadMethod));
}
```

## 9. 高斯模糊滤镜

### 9.1 可分离卷积

```cpp
void GaussianBlur(ImageRGB& image, float sigma) {
    std::vector<float> kernel = GenerateGaussianKernel(sigma);
    int radius = kernel.size() / 2;
    
    ImageRGB temp = image;  // 临时缓冲
    
    // 水平方向卷积
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vec3 sum(0);
            for (int k = -radius; k <= radius; ++k) {
                int sx = std::clamp(x + k, 0, width - 1);
                sum += temp.At(sx, y) * kernel[k + radius];
            }
            image.At(x, y) = sum;
        }
    }
    
    temp = image;
    
    // 垂直方向卷积
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Vec3 sum(0);
            for (int k = -radius; k <= radius; ++k) {
                int sy = std::clamp(y + k, 0, height - 1);
                sum += temp.At(x, sy) * kernel[k + radius];
            }
            image.At(x, y) = sum;
        }
    }
}
```

## 10. 性能优化策略

### 10.1 Tile-based 渲染

```
┌─────────────────────────────────────────────────────────────────┐
│                    Tile-based Rendering                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────┬──────┬──────┬──────┐   1. 将画布划分为 16x16 或        │
│  │ T00  │ T01  │ T02  │ T03  │      32x32 的 tiles               │
│  ├──────┼──────┼──────┼──────┤                                   │
│  │ T10  │ T11  │ T12  │ T13  │   2. 计算每个图元影响的 tiles      │
│  ├──────┼──────┼──────┼──────┤                                   │
│  │ T20  │ T21  │ T22  │ T23  │   3. 每个 tile 独立渲染            │
│  ├──────┼──────┼──────┼──────┤      (可并行)                      │
│  │ T30  │ T31  │ T32  │ T33  │                                   │
│  └──────┴──────┴──────┴──────┘   4. 空 tiles 跳过渲染             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 10.2 边表优化

- **活动边表 (AET)**: 仅维护当前扫描线相交的边
- **边排序**: 按 Y 坐标排序，减少遍历
- **桶排序**: 按 Y 坐标分桶，O(1) 查找
- **缓存优化**: 边数据按扫描顺序排列

## 11. 实现优先级

### Phase 1 (核心功能) - 本次实现
1. ✅ Transform Matrix Stack
2. ✅ Adaptive Bézier Tessellation  
3. ✅ Stroke System (Cap + Join)
4. ✅ Coverage-based Anti-aliasing
5. ✅ Non-Zero Winding Fill Rule

### Phase 2 (增强功能)
1. Linear/Radial Gradient
2. Analytical Edge AA
3. FreeType Font Integration
4. Gaussian Blur Filter

### Phase 3 (高级功能)
1. Tile-based Rendering
2. GPU Acceleration
3. SDF Font Rendering
4. Clip Path / Mask
5. Blend Modes

## 12. 参考实现

- **Skia**: Google's 2D graphics library (Chrome, Android)
- **NanoVG**: Lightweight OpenGL vector graphics
- **Blend2D**: High-performance 2D graphics engine
- **Cairo**: Cross-platform vector graphics library
- **WebRender**: Firefox's GPU-based renderer
