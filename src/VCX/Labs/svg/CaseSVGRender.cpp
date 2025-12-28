#include "Labs/svg/CaseSVGRender.h"
#include "Labs/Common/ImGuiHelper.h"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <sstream>
#include <fstream>
#include <cmath>

namespace VCX::Labs::SVG {

    CaseSVGRender::CaseSVGRender():
        _texture({ .MinFilter = Engine::GL::FilterMode::Linear, .MagFilter = Engine::GL::FilterMode::Nearest }) {
        std::memset(_svgTextBuffer, 0, sizeof(_svgTextBuffer));
    }

    void CaseSVGRender::OnSetupPropsUI() {
        if (ImGui::BeginTabBar("EditorTabs")) {
            if (ImGui::BeginTabItem("Code")) {
                DrawCodeEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Inspector")) {
                DrawPropertiesPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Layers")) {
                DrawLayersPanel();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings")) {
                ImGui::Text("=== Editor Settings ===");
                ImGui::Checkbox("Show Control Points", &_showControlPoints);
                ImGui::Checkbox("Auto Sync Text", &_autoSyncText);
                ImGui::Checkbox("Show Grid", &_showGrid);
                ImGui::SliderFloat("Grid Size", &_gridSize, 10.0f, 100.0f);
                ImGui::ColorEdit4("Background", (float*)&_backgroundColor);
                
                ImGui::Separator();
                ImGui::Text("=== File Operations ===");
                static char pathBuf[512] = {0};
                static bool bufInit = true;
                if (bufInit) {
                    std::size_t n = std::min(_svgFilePath.size(), sizeof(pathBuf) - 1);
                    std::memcpy(pathBuf, _svgFilePath.data(), n);
                    pathBuf[n] = '\0';
                    bufInit = false;
                }
                ImGui::InputText("SVG Path", pathBuf, IM_ARRAYSIZE(pathBuf));
                if (ImGui::Button("Load File", ImVec2(-1, 0))) {
                    _svgFilePath = pathBuf;
                    LoadSVGFile();
                }
                if (ImGui::Button("Save File", ImVec2(-1, 0))) {
                    _svgFilePath = pathBuf;
                    SaveSVGFile();
                }
                if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) {
                    ClearCanvas();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void CaseSVGRender::DrawCodeEditor() {
        ImGui::Text("SVG Source Code");
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
        // Calculate available height
        float height = ImGui::GetContentRegionAvail().y - 40;
        if (ImGui::InputTextMultiline("##SVGEditor", _svgTextBuffer, 
                                     sizeof(_svgTextBuffer), 
                                     ImVec2(-1, height > 200 ? height : 200), flags)) {
            if (_autoSyncText) {
                UpdateSVGFromText();
                UpdateRender();
            }
        }
        
        if (!_autoSyncText && ImGui::Button("Apply Changes", ImVec2(-1, 0))) {
            UpdateSVGFromText();
            UpdateRender();
        }
    }

    void CaseSVGRender::DrawPropertiesPanel() {
        if (!_fileLoaded) {
            ImGui::Text("No file loaded.");
            return;
        }
        
        if (_selectedElementIndex < 0 || _selectedElementIndex >= (int)_svgDocument.elements.size()) {
            ImGui::Text("No element selected.");
            return;
        }

        auto& element = _svgDocument.elements[_selectedElementIndex];
        const auto& bounds = _elementBounds[_selectedElementIndex];

        ImGui::Text("Type: %s", bounds.tagName.c_str());
        
        // ID Editing
        static char idBuf[128];
        std::strncpy(idBuf, element.id.c_str(), sizeof(idBuf) - 1);
        idBuf[sizeof(idBuf) - 1] = '\0';
        if (ImGui::InputText("ID", idBuf, sizeof(idBuf))) {
            element.id = idBuf;
            UpdateTextFromSVG();
        }

        ImGui::Separator();
        ImGui::Text("Style");

        // Fill Color
        if (element.style.fillColor.has_value()) {
            glm::vec4 color = element.style.fillColor.value();
            if (ImGui::ColorEdit4("Fill Color", (float*)&color)) {
                element.style.fillColor = color;
                UpdateTextFromSVG();
                UpdateRender();
            }
        } else {
            if (ImGui::Button("Add Fill")) {
                element.style.fillColor = glm::vec4(0, 0, 0, 1);
                UpdateTextFromSVG();
                UpdateRender();
            }
        }

        // Stroke Color
        if (element.style.strokeColor.has_value()) {
            glm::vec4 color = element.style.strokeColor.value();
            if (ImGui::ColorEdit4("Stroke Color", (float*)&color)) {
                element.style.strokeColor = color;
                UpdateTextFromSVG();
                UpdateRender();
            }
        } else {
            if (ImGui::Button("Add Stroke")) {
                element.style.strokeColor = glm::vec4(0, 0, 0, 1);
                UpdateTextFromSVG();
                UpdateRender();
            }
        }

        float strokeWidth = element.style.strokeWidth.value_or(1.0f);
        if (ImGui::DragFloat("Stroke Width", &strokeWidth, 0.1f, 0.0f, 100.0f)) {
            element.style.strokeWidth = strokeWidth;
            UpdateTextFromSVG();
            UpdateRender();
        }

        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Text("Matrix:");
        bool changed = false;
        for (int i = 0; i < 3; ++i) {
            if (ImGui::DragFloat3(("##row" + std::to_string(i)).c_str(), (float*)&element.transform.matrix[i])) {
                changed = true;
            }
        }
        if (changed) {
            UpdateTextFromSVG();
            UpdateRender();
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Element", ImVec2(-1, 0))) {
            _svgDocument.elements.erase(_svgDocument.elements.begin() + _selectedElementIndex);
            _selectedElementIndex = -1;
            UpdateTextFromSVG();
            UpdateRender();
        }
    }

    void CaseSVGRender::DrawLayersPanel() {
        if (!_fileLoaded) return;

        ImGui::Text("Elements: %zu", _svgDocument.elements.size());
        ImGui::Separator();

        ImGui::BeginChild("LayersList");
        for (size_t i = 0; i < _svgDocument.elements.size(); ++i) {
            const auto& elem = _svgDocument.elements[i];
            std::string label = elem.id.empty() ? 
                (elem.type == SVGElement::Type::Path ? "Path" : 
                 elem.type == SVGElement::Type::Rect ? "Rect" : 
                 elem.type == SVGElement::Type::Circle ? "Circle" : "Element") : elem.id;
            
            label += "##" + std::to_string(i);
            
            if (ImGui::Selectable(label.c_str(), _selectedElementIndex == i)) {
                _selectedElementIndex = i;
                UpdateRender();
            }
        }
        ImGui::EndChild();
    }

    Common::CaseRenderResult CaseSVGRender::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        if (_recompute) {
            UpdateRender();
            _recompute = false;
        }

        return Common::CaseRenderResult {
            .Fixed     = true,
            .Image     = _texture,
            .ImageSize = { _renderWidth, _renderHeight },
        };
    }

    void CaseSVGRender::OnProcessInput(ImVec2 const & pos) {
        // Draw Overlay UI
        DrawToolbar();
        DrawStatusBar(pos);

        if (!_fileLoaded) return;
        
        ImGuiIO& io = ImGui::GetIO();
        bool isMouseDown = io.MouseDown[0];
        bool wasMouseDown = _isDragging;
        
        // Handle Tools
        if (_currentTool == ToolType::Pan) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 delta = io.MouseDelta;
                ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
                ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
            }
            return; // Skip other interactions
        }

        if (_currentTool == ToolType::Zoom) {
            if (io.MouseWheel != 0) {
                float scaleFactor = 1.1f;
                if (io.MouseWheel < 0) scaleFactor = 1.0f / scaleFactor;
                
                _zoomLevel *= scaleFactor;
                _renderWidth = static_cast<uint32_t>(_renderWidth * scaleFactor);
                _renderHeight = static_cast<uint32_t>(_renderHeight * scaleFactor);
                UpdateRender();
            }
            return;
        }

        // Default Select/Edit Mode
        
        // 鼠标按下
        if (isMouseDown && !wasMouseDown) {
            StartDrag(pos.x, pos.y);
        }
        // 鼠标拖拽中
        else if (isMouseDown && wasMouseDown) {
            UpdateDrag(pos.x, pos.y);
        }
        // 鼠标释放
        else if (!isMouseDown && wasMouseDown) {
            EndDrag();
        }
        // 鼠标移动（未按下）
        else {
            // 更新悬停状态
            int newHovered = -1;
            
            // 先检查控制点
            if (_selectedElementIndex >= 0 && _showControlPoints) {
                int cpIndex = FindControlPointAtPosition(pos.x, pos.y);
                if (cpIndex >= 0) {
                    // 悬停在控制点上
                    newHovered = _selectedElementIndex;
                }
            }
            
            // 检查元素
            if (newHovered < 0) {
                newHovered = FindElementAtPosition(pos.x, pos.y);
            }
            
            if (newHovered != _hoveredElementIndex) {
                _hoveredElementIndex = newHovered;
                UpdateRender();
            }
        }
        
        _lastMousePos = pos;
        
        // 键盘输入
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            _selectedElementIndex = -1;
            _isDragging = false;
            UpdateRender();
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (_selectedElementIndex >= 0 && _selectedElementIndex < _svgDocument.elements.size()) {
                _svgDocument.elements.erase(_svgDocument.elements.begin() + _selectedElementIndex);
                _selectedElementIndex = -1;
                UpdateTextFromSVG();
                UpdateRender();
            }
        }
    }

    void CaseSVGRender::DrawToolbar() {
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::BeginGroup();
        
        // Toolbar background
        ImGui::PushStyleColor(ImGuiCol_Button, _currentTool == ToolType::Select ? ImVec4(0.4f, 0.4f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        if (ImGui::Button("Select")) _currentTool = ToolType::Select;
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, _currentTool == ToolType::Pan ? ImVec4(0.4f, 0.4f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        if (ImGui::Button("Pan")) _currentTool = ToolType::Pan;
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, _currentTool == ToolType::Zoom ? ImVec4(0.4f, 0.4f, 0.8f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
        if (ImGui::Button("Zoom")) _currentTool = ToolType::Zoom;
        ImGui::PopStyleColor();

        ImGui::EndGroup();
    }

    void CaseSVGRender::DrawStatusBar(const ImVec2& mousePos) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(10, windowSize.y - 30));
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Pos: (%.1f, %.1f) | Zoom: %.1f%% | Tool: %s", 
            mousePos.x, mousePos.y, _zoomLevel * 100.0f, 
            _currentTool == ToolType::Select ? "Select" : _currentTool == ToolType::Pan ? "Pan" : "Zoom");
    }

    void CaseSVGRender::StartDrag(float x, float y) {
        _dragStartPos = ImVec2(x, y);
        _dragCurrentPos = ImVec2(x, y);
        
        // 检查是否点击了控制点
        if (_selectedElementIndex >= 0 && _showControlPoints) {
            int cpIndex = FindControlPointAtPosition(x, y);
            if (cpIndex >= 0) {
                _isDragging = true;
                _draggedControlPointIndex = cpIndex;
                _dragType = _controlPoints[cpIndex].type;
                
                // 保存原始数据
                const auto& cp = _controlPoints[cpIndex];
                _originalControlPoint = cp.position;
                
                if (_selectedElementIndex < (int)_svgDocument.elements.size()) {
                    const auto& elem = _svgDocument.elements[_selectedElementIndex];
                    const auto& bounds = _elementBounds[_selectedElementIndex];
                    _originalX = bounds.minX;
                    _originalY = bounds.minY;
                    _originalWidth = bounds.maxX - bounds.minX;
                    _originalHeight = bounds.maxY - bounds.minY;
                }
                
                return;
            }
        }
        
        // 检查是否点击了元素
        int clickedElement = FindElementAtPosition(x, y);
        if (clickedElement >= 0) {
            _selectedElementIndex = clickedElement;
            _isDragging = true;
            _dragType = ControlPointType::MoveElement;
            
            // 更新控制点
            UpdateControlPoints();
            
            // 保存原始位置
            const auto& bounds = _elementBounds[clickedElement];
            _originalX = bounds.minX;
            _originalY = bounds.minY;
            _originalWidth = bounds.maxX - bounds.minX;
            _originalHeight = bounds.maxY - bounds.minY;

            // 保存原始点数据（用于路径和直线）
            _originalPoints.clear();
            const auto& elem = _svgDocument.elements[clickedElement];
            if (elem.type == SVGElement::Type::Path) {
                for (const auto& cmd : elem.path.commands) {
                    for (const auto& p : cmd.points) {
                        _originalPoints.push_back(p);
                    }
                }
            } else if (elem.type == SVGElement::Type::Line) {
                _originalPoints.push_back(elem.line.start);
                _originalPoints.push_back(elem.line.end);
            }
            
            UpdateRender();
        } else {
            _selectedElementIndex = -1;
            UpdateRender();
        }
    }

    void CaseSVGRender::UpdateDrag(float x, float y) {
        if (!_isDragging) return;
        
        _dragCurrentPos = ImVec2(x, y);
        float dx = x - _dragStartPos.x;
        float dy = y - _dragStartPos.y;
        
        if (_dragType == ControlPointType::MoveElement) {
            MoveElement(_selectedElementIndex, dx, dy);
        }
        else if (_dragType >= ControlPointType::ResizeTopLeft && 
                 _dragType <= ControlPointType::ResizeRight) {
            ResizeElement(_selectedElementIndex, dx, dy, _dragType);
        }
        else if (_dragType == ControlPointType::BezierControl1 || 
                 _dragType == ControlPointType::BezierControl2) {
            // 贝塞尔控制点拖拽
            if (_draggedControlPointIndex >= 0) {
                const auto& cp = _controlPoints[_draggedControlPointIndex];
                UpdateBezierControlPoint(cp.elementIndex, cp.commandIndex, 
                                        _dragType == ControlPointType::BezierControl1 ? 0 : 1,
                                        x, y);
            }
        }
        
        UpdateRender();
    }

    void CaseSVGRender::EndDrag() {
        if (_isDragging) {
            _isDragging = false;
            _draggedControlPointIndex = -1;
            
            // 同步到文本
            if (_autoSyncText) {
                UpdateTextFromSVG();
            }
        }
    }

    void CaseSVGRender::LoadSVGFile() {
        // 尝试加载测试SVG文件
        std::ifstream file(_svgFilePath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            _svgTextContent = buffer.str();
            file.close();
        }

        if (_svgParser.ParseFile(_svgFilePath, _svgDocument)) {
            _fileLoaded = true;
            _recompute = true;
            
            // 更新文本编辑器内容
            if (!_svgTextContent.empty()) {
                std::size_t n = std::min(_svgTextContent.size(), sizeof(_svgTextBuffer) - 1);
                std::memcpy(_svgTextBuffer, _svgTextContent.data(), n);
                _svgTextBuffer[n] = '\0';
            }
            
            std::cout << "SVG file loaded successfully: " << _svgFilePath << std::endl;
            std::cout << "Elements found: " << _svgDocument.elements.size() << std::endl;
            
            UpdateElementBounds();
        }
        else{
            std::cerr << "Failed to load SVG file: " << _svgFilePath << std::endl;
        }
    }

    void CaseSVGRender::SaveSVGFile() {
        std::ofstream file(_svgFilePath);
        if (file.is_open()) {
            file << _svgTextBuffer;
            file.close();
            std::cout << "SVG file saved successfully: " << _svgFilePath << std::endl;
        } else {
            std::cerr << "Failed to save SVG file: " << _svgFilePath << std::endl;
        }
    }

    void CaseSVGRender::ClearCanvas() {
        _svgDocument.elements.clear();
        _svgDocument.width = 800;
        _svgDocument.height = 600;
        _svgDocument.viewBox = "0 0 800 600";
        
        _selectedElementIndex = -1;
        _hoveredElementIndex = -1;
        _isDragging = false;
        _fileLoaded = true; // 即使清空了，我们也认为有一个“空文档”加载着
        
        UpdateTextFromSVG();
        UpdateElementBounds();
        UpdateRender();
        
        std::cout << "Canvas cleared" << std::endl;
    }

    void CaseSVGRender::UpdateRender() {
        if (!_fileLoaded) {
            // 创建一个空白图像
            _image = Common::CreatePureImageRGB(_renderWidth, _renderHeight, glm::vec3(0.8f, 0.8f, 0.8f));
        } else {
            // 渲染SVG
            RenderWithHighlight(_hoveredElementIndex);
        }

        _texture.Update(_image);
    }

    void CaseSVGRender::UpdateElementBounds() {
        _elementBounds.clear();
        
        for (size_t i = 0; i < _svgDocument.elements.size(); i++) {
            const auto& element = _svgDocument.elements[i];
            ElementBounds bounds;
            bounds.elementIndex = static_cast<int>(i);
            bounds.id = element.id;
            
            // 根据元素类型计算边界
            switch (element.type) {
                case SVGElement::Type::Circle: {
                    Point2D center = element.circle.transform.TransformPoint(element.circle.center);
                    float r = element.circle.radius;
                    bounds.minX = center.x - r;
                    bounds.minY = center.y - r;
                    bounds.maxX = center.x + r;
                    bounds.maxY = center.y + r;
                    bounds.tagName = "circle";
                    break;
                }
                case SVGElement::Type::Rect: {
                    Point2D pos = element.rect.transform.TransformPoint(element.rect.position);
                    bounds.minX = pos.x;
                    bounds.minY = pos.y;
                    bounds.maxX = pos.x + element.rect.width;
                    bounds.maxY = pos.y + element.rect.height;
                    bounds.tagName = "rect";
                    break;
                }
                case SVGElement::Type::Line: {
                    Point2D start = element.line.transform.TransformPoint(element.line.start);
                    Point2D end = element.line.transform.TransformPoint(element.line.end);
                    bounds.minX = std::min(start.x, end.x);
                    bounds.minY = std::min(start.y, end.y);
                    bounds.maxX = std::max(start.x, end.x);
                    bounds.maxY = std::max(start.y, end.y);
                    bounds.tagName = "line";
                    break;
                }
                case SVGElement::Type::Path: {
                    auto vertices = element.path.GetVertices();
                    if (!vertices.empty()) {
                        bounds.minX = bounds.maxX = vertices[0].x;
                        bounds.minY = bounds.maxY = vertices[0].y;
                        for (const auto& v : vertices) {
                            bounds.minX = std::min(bounds.minX, v.x);
                            bounds.minY = std::min(bounds.minY, v.y);
                            bounds.maxX = std::max(bounds.maxX, v.x);
                            bounds.maxY = std::max(bounds.maxY, v.y);
                        }
                    }
                    bounds.tagName = "path";
                    break;
                }
                case SVGElement::Type::Ellipse: {
                    Point2D center = element.ellipse.transform.TransformPoint(element.ellipse.center);
                    bounds.minX = center.x - element.ellipse.rx;
                    bounds.minY = center.y - element.ellipse.ry;
                    bounds.maxX = center.x + element.ellipse.rx;
                    bounds.maxY = center.y + element.ellipse.ry;
                    bounds.tagName = "ellipse";
                    break;
                }
                case SVGElement::Type::Text: {
                    Point2D pos = element.text.transform.TransformPoint(element.text.position);
                    float w = element.text.text.length() * element.text.fontSize * 0.6f;
                    float h = element.text.fontSize;
                    bounds.minX = pos.x;
                    bounds.minY = pos.y;
                    bounds.maxX = pos.x + w;
                    bounds.maxY = pos.y + h;
                    bounds.tagName = "text";
                    break;
                }
                default:
                    continue;
            }
            
            _elementBounds.push_back(bounds);
        }
    }

    int CaseSVGRender::FindElementAtPosition(float x, float y) {
        // 从后往前遍历（后绘制的元素在上层）
        for (int i = static_cast<int>(_elementBounds.size()) - 1; i >= 0; i--) {
            const auto& bounds = _elementBounds[i];
            if (x >= bounds.minX && x <= bounds.maxX && 
                y >= bounds.minY && y <= bounds.maxY) {
                return i;
            }
        }
        return -1;
    }

    void CaseSVGRender::RenderWithHighlight(int highlightIndex) {
        // 正常渲染所有元素
        _image = _svgRenderer.RenderSVG(_svgDocument, _renderWidth, _renderHeight);
        
        // 如果有选中的元素，绘制选择框和控制点
        if (_selectedElementIndex >= 0 && _selectedElementIndex < _elementBounds.size()) {
            const auto& bounds = _elementBounds[_selectedElementIndex];
            
            // 绘制选择框（蓝色）
            glm::vec3 selectColor(0, 0, 255);
            
            int x1 = static_cast<int>(bounds.minX) - 2;
            int y1 = static_cast<int>(bounds.minY) - 2;
            int x2 = static_cast<int>(bounds.maxX) + 2;
            int y2 = static_cast<int>(bounds.maxY) + 2;
            
            // 绘制选择边框
            for (int x = x1; x <= x2; x++) {
                if (x >= 0 && x < _image.GetSizeX()) {
                    if (y1 >= 0 && y1 < _image.GetSizeY()) {
                        _image.At(x, y1) = selectColor;
                    }
                    if (y2 >= 0 && y2 < _image.GetSizeY()) {
                        _image.At(x, y2) = selectColor;
                    }
                }
            }
            
            for (int y = y1; y <= y2; y++) {
                if (y >= 0 && y < _image.GetSizeY()) {
                    if (x1 >= 0 && x1 < _image.GetSizeX()) {
                        _image.At(x1, y) = selectColor;
                    }
                    if (x2 >= 0 && x2 < _image.GetSizeX()) {
                        _image.At(x2, y) = selectColor;
                    }
                }
            }
            
            // 绘制控制点
            if (_showControlPoints) {
                RenderControlPoints(_image);
            }
        }
        // 如果只是悬停（未选中），绘制高亮边框
        else if (highlightIndex >= 0 && highlightIndex < _elementBounds.size()) {
            const auto& bounds = _elementBounds[highlightIndex];
            
            // 绘制高亮边框（黄色半透明）
            glm::vec4 highlightColor(1.0f, 1.0f, 0.0f, 0.5f);
            
            int x1 = static_cast<int>(bounds.minX) - 2;
            int y1 = static_cast<int>(bounds.minY) - 2;
            int x2 = static_cast<int>(bounds.maxX) + 2;
            int y2 = static_cast<int>(bounds.maxY) + 2;
            
            // 绘制边框矩形
            for (int x = x1; x <= x2; x++) {
                if (x >= 0 && x < _image.GetSizeX()) {
                    if (y1 >= 0 && y1 < _image.GetSizeY()) {
                        glm::vec3 oldColor = _image.At(x, y1);
                        _image.At(x, y1) = oldColor * (1.0f - highlightColor.a) + 
                                          glm::vec3(highlightColor) * highlightColor.a;
                    }
                    if (y2 >= 0 && y2 < _image.GetSizeY()) {
                        glm::vec3 oldColor = _image.At(x, y2);
                        _image.At(x, y2) = oldColor * (1.0f - highlightColor.a) + 
                                          glm::vec3(highlightColor) * highlightColor.a;
                    }
                }
            }
            
            for (int y = y1; y <= y2; y++) {
                if (y >= 0 && y < _image.GetSizeY()) {
                    if (x1 >= 0 && x1 < _image.GetSizeX()) {
                        glm::vec3 oldColor = _image.At(x1, y);
                        _image.At(x1, y) = oldColor * (1.0f - highlightColor.a) + 
                                          glm::vec3(highlightColor) * highlightColor.a;
                    }
                    if (x2 >= 0 && x2 < _image.GetSizeX()) {
                        glm::vec3 oldColor = _image.At(x2, y);
                        _image.At(x2, y) = oldColor * (1.0f - highlightColor.a) + 
                                          glm::vec3(highlightColor) * highlightColor.a;
                    }
                }
            }
        }
    }

    void CaseSVGRender::UpdateSVGFromText() {
        // 从文本编辑器更新SVG
        std::string newContent(_svgTextBuffer);
        
        if (!newContent.empty()) {
            SVGDocument newDocument;
            if (_svgParser.ParseString(newContent, newDocument)) {
                _svgDocument = std::move(newDocument);
                _svgTextContent = newContent;
                std::cout << "SVG updated from text editor" << std::endl;
                std::cout << "Elements: " << _svgDocument.elements.size() << std::endl;
                UpdateElementBounds();
                UpdateControlPoints();
            } else {
                std::cerr << "Failed to parse SVG from text editor" << std::endl;
            }
        }
    }

    void CaseSVGRender::UpdateTextFromSVG() {
        // 从SVG文档更新文本编辑器
        std::string svgString = GenerateSVGString();
        
        std::size_t n = std::min(svgString.size(), sizeof(_svgTextBuffer) - 1);
        std::memcpy(_svgTextBuffer, svgString.data(), n);
        _svgTextBuffer[n] = '\0';
        _svgTextContent = svgString;
    }

    std::string CaseSVGRender::GenerateSVGString() {
        std::ostringstream oss;
        
        oss << "<svg width=\"" << _svgDocument.width << "\" height=\"" << _svgDocument.height << "\" ";
        oss << "xmlns=\"http://www.w3.org/2000/svg\">\n";
        
        for (const auto& elem : _svgDocument.elements) {
            oss << "  ";
            
            switch (elem.type) {
                case SVGElement::Type::Rect: {
                    oss << "<rect";
                    if (!elem.rect.id.empty()) oss << " id=\"" << elem.rect.id << "\"";
                    oss << " x=\"" << elem.rect.position.x << "\"";
                    oss << " y=\"" << elem.rect.position.y << "\"";
                    oss << " width=\"" << elem.rect.width << "\"";
                    oss << " height=\"" << elem.rect.height << "\"";
                    if (elem.rect.style.fillColor) {
                        auto c = *elem.rect.style.fillColor;
                        oss << " fill=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.rect.style.strokeColor) {
                        auto c = *elem.rect.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.rect.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.rect.style.strokeWidth << "\"";
                    }
                    oss << "/>\n";
                    break;
                }
                
                case SVGElement::Type::Circle: {
                    oss << "<circle";
                    if (!elem.circle.id.empty()) oss << " id=\"" << elem.circle.id << "\"";
                    oss << " cx=\"" << elem.circle.center.x << "\"";
                    oss << " cy=\"" << elem.circle.center.y << "\"";
                    oss << " r=\"" << elem.circle.radius << "\"";
                    if (elem.circle.style.fillColor) {
                        auto c = *elem.circle.style.fillColor;
                        oss << " fill=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.circle.style.strokeColor) {
                        auto c = *elem.circle.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.circle.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.circle.style.strokeWidth << "\"";
                    }
                    oss << "/>\n";
                    break;
                }
                
                case SVGElement::Type::Line: {
                    oss << "<line";
                    if (!elem.line.id.empty()) oss << " id=\"" << elem.line.id << "\"";
                    oss << " x1=\"" << elem.line.start.x << "\"";
                    oss << " y1=\"" << elem.line.start.y << "\"";
                    oss << " x2=\"" << elem.line.end.x << "\"";
                    oss << " y2=\"" << elem.line.end.y << "\"";
                    if (elem.line.style.strokeColor) {
                        auto c = *elem.line.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.line.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.line.style.strokeWidth << "\"";
                    }
                    oss << "/>\n";
                    break;
                }
                
                case SVGElement::Type::Path: {
                    oss << "<path";
                    if (!elem.path.id.empty()) oss << " id=\"" << elem.path.id << "\"";
                    oss << " d=\"";
                    
                    Point2D currentPos(0, 0);
                    for (const auto& cmd : elem.path.commands) {
                        switch (cmd.type) {
                            case PathCommandType::MoveTo:
                                oss << "M " << cmd.points[0].x << " " << cmd.points[0].y << " ";
                                currentPos = cmd.points[0];
                                break;
                            case PathCommandType::LineTo:
                                oss << "L " << cmd.points[0].x << " " << cmd.points[0].y << " ";
                                currentPos = cmd.points[0];
                                break;
                            case PathCommandType::CurveTo:
                                if (cmd.points.size() >= 3) {
                                    oss << "C " << cmd.points[0].x << " " << cmd.points[0].y << " ";
                                    oss << cmd.points[1].x << " " << cmd.points[1].y << " ";
                                    oss << cmd.points[2].x << " " << cmd.points[2].y << " ";
                                    currentPos = cmd.points[2];
                                }
                                break;
                            case PathCommandType::QuadCurveTo:
                                if (cmd.points.size() >= 2) {
                                    oss << "Q " << cmd.points[0].x << " " << cmd.points[0].y << " ";
                                    oss << cmd.points[1].x << " " << cmd.points[1].y << " ";
                                    currentPos = cmd.points[1];
                                }
                                break;
                            case PathCommandType::ClosePath:
                                oss << "Z ";
                                break;
                            default:
                                break;
                        }
                    }
                    
                    oss << "\"";
                    if (elem.path.style.fillColor) {
                        auto c = *elem.path.style.fillColor;
                        oss << " fill=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    } else {
                        oss << " fill=\"none\"";
                    }
                    if (elem.path.style.strokeColor) {
                        auto c = *elem.path.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.path.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.path.style.strokeWidth << "\"";
                    }
                    oss << "/>\n";
                    break;
                }
                
                default:
                    break;
            }
        }
        
        oss << "</svg>";
        return oss.str();
    }

    void CaseSVGRender::UpdateControlPoints() {
        _controlPoints.clear();
        
        if (_selectedElementIndex < 0 || _selectedElementIndex >= _svgDocument.elements.size()) {
            return;
        }
        
        const auto& elem = _svgDocument.elements[_selectedElementIndex];
        const auto& bounds = _elementBounds[_selectedElementIndex];
        
        // 添加调整大小的控制点
        if (elem.type == SVGElement::Type::Rect || elem.type == SVGElement::Type::Ellipse) {
            // 四角
            _controlPoints.push_back({Point2D(bounds.minX, bounds.minY), ControlPointType::ResizeTopLeft, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(bounds.maxX, bounds.minY), ControlPointType::ResizeTopRight, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(bounds.minX, bounds.maxY), ControlPointType::ResizeBottomLeft, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(bounds.maxX, bounds.maxY), ControlPointType::ResizeBottomRight, _selectedElementIndex, -1});
            
            // 边中点
            float midX = (bounds.minX + bounds.maxX) / 2;
            float midY = (bounds.minY + bounds.maxY) / 2;
            _controlPoints.push_back({Point2D(midX, bounds.minY), ControlPointType::ResizeTop, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(midX, bounds.maxY), ControlPointType::ResizeBottom, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(bounds.minX, midY), ControlPointType::ResizeLeft, _selectedElementIndex, -1});
            _controlPoints.push_back({Point2D(bounds.maxX, midY), ControlPointType::ResizeRight, _selectedElementIndex, -1});
        }
        else if (elem.type == SVGElement::Type::Circle) {
            // 圆形只需要一个调整半径的控制点，或者四角
            _controlPoints.push_back({Point2D(bounds.maxX, bounds.maxY), ControlPointType::ResizeBottomRight, _selectedElementIndex, -1});
        }
        else if (elem.type == SVGElement::Type::Line) {
            // 直线的两个端点
            Point2D start = elem.line.transform.TransformPoint(elem.line.start);
            Point2D end = elem.line.transform.TransformPoint(elem.line.end);
            _controlPoints.push_back({start, ControlPointType::ResizeTopLeft, _selectedElementIndex, -1});
            _controlPoints.push_back({end, ControlPointType::ResizeBottomRight, _selectedElementIndex, -1});
        }
        
        // 贝塞尔曲线的控制点
        if (elem.type == SVGElement::Type::Path) {
            Point2D currentPos(0, 0);
            for (size_t i = 0; i < elem.path.commands.size(); i++) {
                const auto& cmd = elem.path.commands[i];
                
                if (cmd.type == PathCommandType::CurveTo && cmd.points.size() >= 3) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    Point2D p3 = cmd.relative ? currentPos + cmd.points[2] : cmd.points[2];
                    
                    _controlPoints.push_back({p1, ControlPointType::BezierControl1, _selectedElementIndex, (int)i});
                    _controlPoints.push_back({p2, ControlPointType::BezierControl2, _selectedElementIndex, (int)i});
                    
                    currentPos = p3;
                }
                else if (cmd.type == PathCommandType::QuadCurveTo && cmd.points.size() >= 2) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    
                    _controlPoints.push_back({p1, ControlPointType::BezierControl1, _selectedElementIndex, (int)i});
                    
                    currentPos = p2;
                }
                else if (cmd.type == PathCommandType::MoveTo || cmd.type == PathCommandType::LineTo) {
                    if (!cmd.points.empty()) {
                        currentPos = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    }
                }
            }
        }
        
        // 绘制控制点到图像
        if (_showControlPoints) {
            RenderControlPoints(_image);
        }
    }

    int CaseSVGRender::FindControlPointAtPosition(float x, float y) {
        const float threshold = 8.0f;  // 控制点点击区域
        
        for (size_t i = 0; i < _controlPoints.size(); i++) {
            const auto& cp = _controlPoints[i];
            float dx = x - cp.position.x;
            float dy = y - cp.position.y;
            if (dx*dx + dy*dy <= threshold*threshold) {
                return (int)i;
            }
        }
        
        return -1;
    }

    void CaseSVGRender::RenderControlPoints(Common::ImageRGB& image) {
        for (const auto& cp : _controlPoints) {
            DrawControlPoint(image, cp.position, cp.type, false);
        }
    }

    void CaseSVGRender::DrawControlPoint(Common::ImageRGB& image, const Point2D& pos, 
                                         ControlPointType type, bool isHovered) {
        int cx = (int)pos.x;
        int cy = (int)pos.y;
        int size = 4;
        
        // 根据类型选择颜色
        glm::vec3 color;
        if (type >= ControlPointType::ResizeTopLeft && type <= ControlPointType::ResizeRight) {
            color = glm::vec3(0, 255, 0);  // 绿色 - 调整大小
        } else if (type == ControlPointType::BezierControl1 || type == ControlPointType::BezierControl2) {
            color = glm::vec3(255, 0, 255);  // 品红色 - 贝塞尔控制点
        } else {
            color = glm::vec3(255, 255, 0);  // 黄色 - 其他
        }
        
        if (isHovered) {
            size = 6;
        }
        
        // 绘制实心方块
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int x = cx + dx;
                int y = cy + dy;
                if (x >= 0 && x < image.GetSizeX() && y >= 0 && y < image.GetSizeY()) {
                    image.At(x, y) = color;
                }
            }
        }
        
        // 绘制黑色边框
        for (int dy = -size-1; dy <= size+1; dy++) {
            for (int dx = -size-1; dx <= size+1; dx++) {
                if (std::abs(dx) == size+1 || std::abs(dy) == size+1) {
                    int x = cx + dx;
                    int y = cy + dy;
                    if (x >= 0 && x < image.GetSizeX() && y >= 0 && y < image.GetSizeY()) {
                        image.At(x, y) = glm::vec3(0, 0, 0);
                    }
                }
            }
        }
    }

    void CaseSVGRender::MoveElement(int elementIndex, float dx, float dy) {
        if (elementIndex < 0 || elementIndex >= _svgDocument.elements.size()) return;
        
        auto& elem = _svgDocument.elements[elementIndex];
        
        switch (elem.type) {
            case SVGElement::Type::Rect:
                elem.rect.position.x = _originalX + dx;
                elem.rect.position.y = _originalY + dy;
                break;
                
            case SVGElement::Type::Circle:
                elem.circle.center.x = _originalX + _originalWidth / 2 + dx;
                elem.circle.center.y = _originalY + _originalHeight / 2 + dy;
                break;
                
            case SVGElement::Type::Line:
                elem.line.start.x += dx;
                elem.line.start.y += dy;
                elem.line.end.x += dx;
                elem.line.end.y += dy;
                break;
                
            case SVGElement::Type::Ellipse:
                elem.ellipse.center.x = _originalX + _originalWidth / 2 + dx;
                elem.ellipse.center.y = _originalY + _originalHeight / 2 + dy;
                break;

            case SVGElement::Type::Path: {
                size_t pointIdx = 0;
                for (auto& cmd : elem.path.commands) {
                    for (auto& p : cmd.points) {
                        if (pointIdx < _originalPoints.size()) {
                            p.x = _originalPoints[pointIdx].x + dx;
                            p.y = _originalPoints[pointIdx].y + dy;
                            pointIdx++;
                        }
                    }
                }
                break;
            }

            case SVGElement::Type::Text:
                elem.text.position.x = _originalX + dx;
                elem.text.position.y = _originalY + dy;
                break;
                
            default:
                break;
        }
        
        UpdateElementBounds();
        UpdateControlPoints();
    }

    void CaseSVGRender::ResizeElement(int elementIndex, float dx, float dy, ControlPointType resizeType) {
        if (elementIndex < 0 || elementIndex >= _svgDocument.elements.size()) return;
        
        auto& elem = _svgDocument.elements[elementIndex];
        
        if (elem.type == SVGElement::Type::Rect) {
            float newX = _originalX;
            float newY = _originalY;
            float newWidth = _originalWidth;
            float newHeight = _originalHeight;
            
            switch (resizeType) {
                case ControlPointType::ResizeTopLeft:
                    newX += dx;
                    newY += dy;
                    newWidth -= dx;
                    newHeight -= dy;
                    break;
                case ControlPointType::ResizeTopRight:
                    newY += dy;
                    newWidth += dx;
                    newHeight -= dy;
                    break;
                case ControlPointType::ResizeBottomLeft:
                    newX += dx;
                    newWidth -= dx;
                    newHeight += dy;
                    break;
                case ControlPointType::ResizeBottomRight:
                    newWidth += dx;
                    newHeight += dy;
                    break;
                case ControlPointType::ResizeTop:
                    newY += dy;
                    newHeight -= dy;
                    break;
                case ControlPointType::ResizeBottom:
                    newHeight += dy;
                    break;
                case ControlPointType::ResizeLeft:
                    newX += dx;
                    newWidth -= dx;
                    break;
                case ControlPointType::ResizeRight:
                    newWidth += dx;
                    break;
                default:
                    break;
            }
            
            // 确保宽高为正
            if (newWidth > 5 && newHeight > 5) {
                elem.rect.position.x = newX;
                elem.rect.position.y = newY;
                elem.rect.width = newWidth;
                elem.rect.height = newHeight;
            }
        }
        else if (elem.type == SVGElement::Type::Circle) {
            float newRadius = _originalWidth / 2;
            
            if (resizeType == ControlPointType::ResizeTopLeft || 
                resizeType == ControlPointType::ResizeBottomRight) {
                newRadius += (dx + dy) / 2;
            }
            
            if (newRadius > 5) {
                elem.circle.radius = newRadius;
            }
        }
        else if (elem.type == SVGElement::Type::Ellipse) {
            float newRx = _originalWidth / 2;
            float newRy = _originalHeight / 2;
            
            switch (resizeType) {
                case ControlPointType::ResizeLeft:
                case ControlPointType::ResizeRight:
                    newRx = std::abs(_originalWidth / 2 + (resizeType == ControlPointType::ResizeLeft ? -dx : dx));
                    break;
                case ControlPointType::ResizeTop:
                case ControlPointType::ResizeBottom:
                    newRy = std::abs(_originalHeight / 2 + (resizeType == ControlPointType::ResizeTop ? -dy : dy));
                    break;
                case ControlPointType::ResizeTopLeft:
                case ControlPointType::ResizeTopRight:
                case ControlPointType::ResizeBottomLeft:
                case ControlPointType::ResizeBottomRight:
                    newRx = std::abs(_originalWidth / 2 + (resizeType == ControlPointType::ResizeLeft || resizeType == ControlPointType::ResizeTopLeft || resizeType == ControlPointType::ResizeBottomLeft ? -dx : dx));
                    newRy = std::abs(_originalHeight / 2 + (resizeType == ControlPointType::ResizeTop || resizeType == ControlPointType::ResizeTopLeft || resizeType == ControlPointType::ResizeTopRight ? -dy : dy));
                    break;
                default:
                    break;
            }
            
            if (newRx > 2) elem.ellipse.rx = newRx;
            if (newRy > 2) elem.ellipse.ry = newRy;
        }
        else if (elem.type == SVGElement::Type::Line) {
            if (resizeType == ControlPointType::ResizeTopLeft) { // Start point
                elem.line.start.x = _originalPoints[0].x + dx;
                elem.line.start.y = _originalPoints[0].y + dy;
            } else if (resizeType == ControlPointType::ResizeBottomRight) { // End point
                elem.line.end.x = _originalPoints[1].x + dx;
                elem.line.end.y = _originalPoints[1].y + dy;
            }
        }
        
        UpdateElementBounds();
        UpdateControlPoints();
    }

    void CaseSVGRender::UpdateBezierControlPoint(int elementIndex, int commandIndex, 
                                                 int pointIndex, float x, float y) {
        if (elementIndex < 0 || elementIndex >= _svgDocument.elements.size()) return;
        
        auto& elem = _svgDocument.elements[elementIndex];
        if (elem.type != SVGElement::Type::Path) return;
        if (commandIndex < 0 || commandIndex >= elem.path.commands.size()) return;
        
        auto& cmd = elem.path.commands[commandIndex];
        
        if (cmd.type == PathCommandType::CurveTo && pointIndex < cmd.points.size()) {
            cmd.points[pointIndex].x = x;
            cmd.points[pointIndex].y = y;
        }
        else if (cmd.type == PathCommandType::QuadCurveTo && pointIndex < cmd.points.size()) {
            cmd.points[pointIndex].x = x;
            cmd.points[pointIndex].y = y;
        }
        
        UpdateElementBounds();
        UpdateControlPoints();
    }

    bool CaseSVGRender::IsPointNearLine(const Point2D& p, const Point2D& a, const Point2D& b, float threshold) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float length = std::sqrt(dx*dx + dy*dy);
        
        if (length < 0.001f) return false;
        
        float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (length * length);
        t = std::max(0.0f, std::min(1.0f, t));
        
        Point2D closest(a.x + t * dx, a.y + t * dy);
        
        float distX = p.x - closest.x;
        float distY = p.y - closest.y;
        float dist = std::sqrt(distX*distX + distY*distY);
        
        return dist <= threshold;
    }

}

