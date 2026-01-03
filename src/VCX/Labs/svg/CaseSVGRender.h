#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include "Labs/Common/ICase.h"
#include "Labs/Common/ImageRGB.h"
#include "Engine/GL/Texture.hpp"
#include "SVG.h"
#include "SVGParser.h"
#include "SVGRenderer.h"
#include "Renderer/SVGRendererV2.h"
#include "Rasterizer/ScanlineRasterizer.h"

namespace VCX::Labs::SVG {

    // V2 渲染器设置
    struct RendererV2Settings {
        bool useV2Renderer = false;        // 是否使用V2渲染器
        bool enableAntiAliasing = true;    // 启用抗锯齿
        int  aaSampleCount = 4;            // AA采样数: 1, 4, 8, 16
        float flatnessTolerance = 0.5f;    // 曲线细分容差
        bool showComparison = false;       // 显示对比模式
    };

    // 元素边界框结构
    struct ElementBounds {
        int elementIndex;  // 在document.elements中的索引
        float minX, minY, maxX, maxY;
        std::string id;
        std::string tagName;
    };

    // 控制点类型
    enum class ControlPointType {
        None,
        MoveElement,      // 移动整个元素
        ResizeTopLeft,    // 调整大小 - 左上角
        ResizeTopRight,   // 调整大小 - 右上角
        ResizeBottomLeft, // 调整大小 - 左下角
        ResizeBottomRight,// 调整大小 - 右下角
        ResizeTop,        // 调整大小 - 上边
        ResizeBottom,     // 调整大小 - 下边
        ResizeLeft,       // 调整大小 - 左边
        ResizeRight,      // 调整大小 - 右边
        BezierControl1,   // 贝塞尔控制点1
        BezierControl2,   // 贝塞尔控制点2
        BezierStart,      // 贝塞尔起点
        BezierEnd         // 贝塞尔终点
    };

    // 控制点结构
    struct ControlPoint {
        Point2D position;
        ControlPointType type;
        int elementIndex;
        int commandIndex;  // 对于路径命令的索引
    };

    // 编辑模式
    enum class EditMode {
        Select,    // 选择模式
        Drag,      // 拖拽移动
        Resize,    // 调整大小
        EditPath   // 编辑路径
    };

    // 工具类型
    enum class ToolType {
        Select,
    };

    class CaseSVGRender : public Common::ICase {
    private:
        std::string _svgFilePath = "assets/test_bezier.svg";
        std::string _projectRoot;  // 项目根目录
        SVGDocument _svgDocument;
        SVGParser _svgParser;
        SVGRenderer _svgRenderer;
        SVGRendererV2 _svgRendererV2;          // V2 高质量渲染器
        RendererV2Settings _v2Settings;        // V2 渲染器设置

        Engine::GL::UniqueTexture2D _texture;
        Common::ImageRGB _image;
        bool _recompute = true;
        bool _fileLoaded = false;

        std::uint32_t _renderWidth = 1200;
        std::uint32_t _renderHeight = 900;

        // 文本编辑器相关
        std::string _svgTextContent;
        char _svgTextBuffer[65536];  // 64KB文本缓冲区
        bool _textEditorVisible = true;
        float _editorWidth = 400.0f;  // 编辑器宽度

        // 交互状态
        EditMode _editMode = EditMode::Select;
        ToolType _currentTool = ToolType::Select;
        int _selectedElementIndex = -1;
        int _hoveredElementIndex = -1;
        std::vector<ElementBounds> _elementBounds;
        std::vector<ControlPoint> _controlPoints;
        
        // 拖拽状态
        bool _isDragging = false;
        ImVec2 _dragStartPos;
        ImVec2 _dragCurrentPos;
        ControlPointType _dragType = ControlPointType::None;
        int _draggedControlPointIndex = -1;
        
        // 原始数据（用于拖拽时计算增量）
        float _originalX = 0, _originalY = 0;
        float _originalWidth = 0, _originalHeight = 0;
        Point2D _originalControlPoint;
        std::vector<Point2D> _originalPoints; // 用于路径拖拽

        // UI状态
        bool _showGrid = false;
        float _gridSize = 50.0f;
        float _zoomLevel = 1.0f;
        ImVec4 _backgroundColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        int _currentTab = 0;  // 当前选中的标签页：0=Settings, 1=Layers, 2=Inspector

        // V2渲染器设置
        bool _useV2Renderer = false;       // 是否使用V2渲染器
        bool _enableAntiAliasing = true;   // 启用抗锯齿
        int _aaMode = 1;                   // AA模式: 0=None, 1=4x, 2=8x, 3=16x, 4=Analytical
        float _flatnessTolerance = 0.5f;   // 曲线细分容差

        // 辅助函数
        void LoadSVGFile();
        void SaveSVGFile();
        void UpdateSVGFromText();
        void UpdateTextFromSVG();
        void UpdateRender();
        void ClearCanvas();
        void UpdateBackgroundInSVG();
        void AddNewElement(SVGElement::Type type);
        
        // 交互辅助
        int FindElementAtPosition(float x, float y);
        void UpdateControlPoints();
        int FindControlPointAtPosition(float x, float y);
        void MoveElement(int index, float dx, float dy);
        void ResizeElement(int index, ControlPointType type, float dx, float dy);
        void MoveControlPoint(int index, float dx, float dy);
        
        // 坐标转换辅助（SVG坐标 <-> 渲染/屏幕坐标）
        Point2D SVGToScreen(const Point2D& svgPoint) const;
        Point2D ScreenToSVG(const Point2D& screenPoint) const;
        void UpdateViewBoxTransform();  // 更新viewBox变换参数
        
        // ViewBox 变换参数（缓存）
        float _vbOffsetX = 0.0f, _vbOffsetY = 0.0f;
        float _vbScaleX = 1.0f, _vbScaleY = 1.0f;
        bool _hasViewBox = false;
        
        // 拖拽处理
        void StartDrag(float x, float y);
        void UpdateDrag(float x, float y);
        void EndDrag();

        // UI 绘制
        void DrawToolbar();
        void DrawStatusBar(const ImVec2& mousePos);
        void DrawPropertiesPanel();
        void DrawLayersPanel();
        void DrawCodeEditor();
        void DrawAddElementPanel();  // 新增：添加元素面板
        

    public:
        bool _showControlPoints = true;
        bool _autoSyncText = true;  // 自动同步文本
        ImVec2 _canvasOffset = ImVec2(0, 0);
        ImVec2 _lastMousePos;

    public:
        CaseSVGRender();

        virtual std::string_view const GetName() override { return "SVG Interactive Editor"; }

        virtual void OnSetupPropsUI() override;  // 标签栏按钮
        virtual void OnSetupPropsContent() override;  // 标签内容
        virtual void OnSetupCodeUI() override;  // 新增：代码编辑器UI
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void OnProcessInput(ImVec2 const & pos) override;

    private:
        void UpdateElementBounds();
        
        // 查找函数
        
        // 渲染函数
        void RenderWithHighlight(int highlightIndex);
        void RenderControlPoints(Common::ImageRGB& image);
        void DrawControlPoint(Common::ImageRGB& image, const Point2D& pos, 
                             ControlPointType type, bool isHovered);
        
        // 编辑操作
        void ResizeElement(int elementIndex, float dx, float dy, ControlPointType resizeType);
        void UpdateBezierControlPoint(int elementIndex, int commandIndex, 
                                     int pointIndex, float x, float y);
        
        // SVG同步
        void SyncElementToText(int elementIndex);
        
        // 辅助函数
        bool IsPointNearLine(const Point2D& p, const Point2D& a, const Point2D& b, float threshold);
        std::string GenerateSVGString();
    };

}
