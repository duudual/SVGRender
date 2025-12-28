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

namespace VCX::Labs::SVG {

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
        Pan,
        Zoom
    };

    class CaseSVGRender : public Common::ICase {
    private:
        std::string _svgFilePath = "assets/test_bezier.svg";
        SVGDocument _svgDocument;
        SVGParser _svgParser;
        SVGRenderer _svgRenderer;

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

        // 辅助函数
        void LoadSVGFile();
        void SaveSVGFile();
        void UpdateSVGFromText();
        void UpdateTextFromSVG();
        void UpdateRender();
        void ClearCanvas();
        
        // 交互辅助
        int FindElementAtPosition(float x, float y);
        void UpdateControlPoints();
        int FindControlPointAtPosition(float x, float y);
        void MoveElement(int index, float dx, float dy);
        void ResizeElement(int index, ControlPointType type, float dx, float dy);
        void MoveControlPoint(int index, float dx, float dy);
        
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

    public:
        bool _showControlPoints = true;
        bool _autoSyncText = true;  // 自动同步文本
        ImVec2 _canvasOffset = ImVec2(0, 0);
        ImVec2 _lastMousePos;

    public:
        CaseSVGRender();

        virtual std::string_view const GetName() override { return "SVG Interactive Editor"; }

        virtual void OnSetupPropsUI() override;
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
