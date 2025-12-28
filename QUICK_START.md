# SVG渲染器新功能 - 快速上手指南

## ✨ 三大新功能

### 1️⃣ 平滑的贝塞尔曲线渲染
- ✅ 三次贝塞尔曲线（Cubic Bezier）- 20段平滑采样
- ✅ 二次贝塞尔曲线（Quadratic Bezier）- 15段平滑采样
- 📈 从折线变为真正的曲线

### 2️⃣ 交互式元素高亮
- 🖱️ 鼠标悬停显示黄色边框
- 📍 实时显示元素信息（类型、ID、位置）
- 🎯 类似浏览器开发者工具的体验

### 3️⃣ 实时SVG文本编辑器
- ✏️ 内置64KB文本编辑器
- 🔄 即时预览修改效果
- 💾 支持文件加载和保存

---

## 🚀 快速开始

### 步骤1：编译项目

```bash
cd d:\lecture\2.0_xk\vci\SVGRender
xmake build
# 或使用 Visual Studio 编译
```

### 步骤2：运行程序

```bash
xmake run
# 或直接运行可执行文件
```

### 步骤3：加载测试文件

在程序中：
1. 在 "SVG Path" 输入框输入：`assets/test_bezier.svg`
2. 点击 **[Load]** 按钮
3. 点击 **[Render]** 按钮

---

## 🧪 测试功能

### 测试1：贝塞尔曲线

**查看预制的曲线：**
- 青色的S形曲线（三次贝塞尔）
- 品红色的抛物线（二次贝塞尔）
- 粉色的心形（复杂组合路径）

**手动创建曲线：**
1. 勾选 ☑ **Show Editor**
2. 在编辑器中添加：
```xml
<path d="M 100 100 C 100 50, 300 50, 300 100" 
      stroke="blue" stroke-width="3" fill="none"/>
```
3. 点击 **[Render]**
4. 看到平滑的弧线

### 测试2：交互式高亮

**鼠标悬停测试：**
1. 将鼠标移动到**红色矩形**上
2. 观察：
   - 矩形周围出现**黄色边框**
   - UI面板显示：
     ```
     Hovered Element:
       Tag: rect
       ID: rect1
     ```
3. 移动到其他元素，高亮自动切换

**支持的元素：**
- ✅ Rectangle (矩形)
- ✅ Circle (圆形)
- ✅ Ellipse (椭圆)
- ✅ Line (直线)
- ✅ Path (路径)

### 测试3：实时编辑

**修改颜色：**
1. 在编辑器中找到：
```xml
<rect id="rect1" x="50" y="50" width="120" height="80" 
      fill="red" stroke="black" stroke-width="2"/>
```

2. 修改为：
```xml
<rect id="rect1" x="50" y="50" width="120" height="80" 
      fill="green" stroke="blue" stroke-width="5"/>
```

3. 点击 **[Render]**
4. 矩形立即变为绿色，边框变蓝变粗

**添加新元素：**
在 `</svg>` 标签前添加：
```xml
<circle cx="500" cy="350" r="30" fill="gold" stroke="black" stroke-width="2"/>
```

点击 **[Render]**，右下角出现金色圆圈。

---

## 🎨 创意示例

### 示例1：彩虹曲线

```xml
<svg width="600" height="400" xmlns="http://www.w3.org/2000/svg">
  <path d="M 50 300 Q 150 100, 250 300" 
        stroke="red" stroke-width="5" fill="none"/>
  <path d="M 70 300 Q 170 120, 270 300" 
        stroke="orange" stroke-width="5" fill="none"/>
  <path d="M 90 300 Q 190 140, 290 300" 
        stroke="yellow" stroke-width="5" fill="none"/>
  <path d="M 110 300 Q 210 160, 310 300" 
        stroke="green" stroke-width="5" fill="none"/>
  <path d="M 130 300 Q 230 180, 330 300" 
        stroke="blue" stroke-width="5" fill="none"/>
  <path d="M 150 300 Q 250 200, 350 300" 
        stroke="purple" stroke-width="5" fill="none"/>
</svg>
```

### 示例2：波浪图案

```xml
<svg width="600" height="400" xmlns="http://www.w3.org/2000/svg">
  <path d="M 0 200 Q 75 150, 150 200 T 300 200 T 450 200 T 600 200" 
        stroke="blue" stroke-width="3" fill="none"/>
  <path d="M 0 220 Q 75 170, 150 220 T 300 220 T 450 220 T 600 220" 
        stroke="cyan" stroke-width="3" fill="none"/>
  <path d="M 0 240 Q 75 190, 150 240 T 300 240 T 450 240 T 600 240" 
        stroke="lightblue" stroke-width="3" fill="none"/>
</svg>
```

### 示例3：笑脸

```xml
<svg width="400" height="400" xmlns="http://www.w3.org/2000/svg">
  <!-- 脸 -->
  <circle id="face" cx="200" cy="200" r="100" 
          fill="yellow" stroke="orange" stroke-width="3"/>
  
  <!-- 左眼 -->
  <circle id="left-eye" cx="170" cy="180" r="10" fill="black"/>
  
  <!-- 右眼 -->
  <circle id="right-eye" cx="230" cy="180" r="10" fill="black"/>
  
  <!-- 微笑（二次贝塞尔曲线）-->
  <path id="smile" 
        d="M 160 220 Q 200 250, 240 220" 
        stroke="black" 
        stroke-width="3" 
        fill="none"/>
</svg>
```

---

## 📋 UI控件说明

### 主界面

```
┌──────────────────────────────────────────┐
│ SVG Path: [________] [Load]   Current:  │
│ ☑ Show Editor                            │
├──────────────────────────────────────────┤
│ Render Size:                             │
│   Width:  [800]                          │
│   Height: [600]                          │
│ [Render]                                 │
├──────────────────────────────────────────┤
│ SVG Info:                                │
│   Document Size: 600.0 x 400.0           │
│   Elements: 9                            │
│   ViewBox:                               │
├──────────────────────────────────────────┤
│ Hovered Element:                         │
│   Tag: circle                            │
│   ID: circle1                            │
└──────────────────────────────────────────┘
```

### 编辑器面板

```
┌──────────────────────────────────────────┐
│ SVG Source Editor:                       │
│ ┌────────────────────────────────────┐   │
│ │ <svg width="600" height="400">    │   │
│ │   <rect x="50" y="50"             │   │
│ │         width="100" height="80"   │   │
│ │         fill="red"/>              │   │
│ │   ...                             │   │
│ │ </svg>                            │   │
│ └────────────────────────────────────┘   │
│ Tip: Edit the SVG and click 'Render'    │
└──────────────────────────────────────────┘
```

---

## ⌨️ 键盘快捷键

| 快捷键 | 功能 |
|--------|------|
| **Tab** | 在编辑器中插入缩进 |
| **Ctrl+A** | 全选编辑器文本 |
| **Ctrl+C/V** | 复制/粘贴 |

---

## 🐛 常见问题

### Q1: 编辑后点击Render没反应？

**检查：**
- SVG语法是否正确（标签闭合、引号匹配）
- 查看控制台是否有错误信息

### Q2: 鼠标悬停没有高亮？

**可能原因：**
- 元素太小，鼠标没有精确指向
- 元素被其他元素遮挡（后绘制的在上层）

### Q3: 贝塞尔曲线不够平滑？

**解决：**
在 `SVG.cpp` 中增加采样点数：
```cpp
const int segments = 30;  // 从20改为30
```

### Q4: 文本编辑器内容太长被截断？

**限制：**
- 最大64KB文本
- 如需更大，修改 `CaseSVGRender.h`:
```cpp
char _svgTextBuffer[131072];  // 改为128KB
```

---

## 🎯 高级技巧

### 技巧1：复制浏览器的SVG

1. 在浏览器中右键点击SVG图像
2. 选择"检查元素"
3. 复制SVG代码
4. 粘贴到编辑器
5. 点击Render

### 技巧2：动态调整曲线平滑度

修改源代码中的segments参数：
- `segments = 10`：快速渲染，略有折线感
- `segments = 20`：默认值，平衡
- `segments = 50`：非常平滑，但渲染较慢

### 技巧3：查看元素层级

元素的绘制顺序就是在SVG中的顺序，后面的元素会覆盖前面的。

**实验：**
```xml
<circle cx="100" cy="100" r="50" fill="red"/>
<circle cx="120" cy="100" r="50" fill="blue"/>
```
蓝色圆会部分覆盖红色圆。

---

## 📚 学习资源

### SVG路径命令速查

| 命令 | 含义 | 参数 | 示例 |
|------|------|------|------|
| **M** | 移动到 | x y | `M 10 10` |
| **L** | 直线到 | x y | `L 100 100` |
| **H** | 水平线 | x | `H 100` |
| **V** | 垂直线 | y | `V 100` |
| **C** | 三次贝塞尔 | x1 y1 x2 y2 x y | `C 10 10 90 90 100 50` |
| **Q** | 二次贝塞尔 | x1 y1 x y | `Q 50 10 100 50` |
| **Z** | 闭合路径 | 无 | `Z` |

**注意：** 小写字母表示相对坐标，大写表示绝对坐标。

### 在线SVG编辑器

练习完本渲染器后，可以尝试：
- **SVG Path Editor** - https://yqnn.github.io/svg-path-editor/
- **Boxy SVG** - https://boxy-svg.com/
- **Inkscape** - 专业的SVG编辑软件

---

## 🎉 开始创作吧！

现在你已经掌握了所有功能，开始创作你的SVG作品吧！

**建议练习顺序：**
1. ✅ 加载测试文件，熟悉界面
2. ✅ 修改颜色和尺寸，体验实时编辑
3. ✅ 创建简单的贝塞尔曲线
4. ✅ 组合多个元素制作图案
5. ✅ 尝试复杂的路径和组合

祝你玩得开心！ 🚀
