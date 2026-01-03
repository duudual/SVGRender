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
        _svgTextContent = "";  // 初始化文本内容
    }

    void CaseSVGRender::OnSetupPropsUI() {
        // 只绘制标签按钮
        if (ImGui::Button("Settings", ImVec2(100, 30) )) {
            _currentTab = 0;
        }
        if (_currentTab == 0) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y),
                ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 1.0f)), 2.0f);
            ImGui::PopStyleColor();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Layers", ImVec2(100, 30))) {
            _currentTab = 1;
        }
        if (_currentTab == 1) {
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y),
                ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 1.0f)), 2.0f);
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Inspector", ImVec2(100, 30))) {
            _currentTab = 2;
        }
        if (_currentTab == 2) {
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y),
                ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 1.0f)), 2.0f);
        }
    }

    void CaseSVGRender::OnSetupPropsContent() {
        // 根据当前选中的标签显示对应内容
        if (_currentTab == 0) {
            // Settings 内容
            ImGui::Text("=== Editor Settings ===");
            ImGui::Checkbox("Show Control Points", &_showControlPoints);
            ImGui::Checkbox("Auto Sync Text", &_autoSyncText);
            ImGui::Checkbox("Show Grid", &_showGrid);
            ImGui::SliderFloat("Grid Size", &_gridSize, 10.0f, 100.0f);
            
            // 背景颜色调整
            if (ImGui::ColorEdit4("Background", (float*)&_backgroundColor)) {
                UpdateRender();
            }
            if (ImGui::Button("Apply Background to SVG", ImVec2(-1, 0))) {
                UpdateBackgroundInSVG();
                UpdateTextFromSVG();
                UpdateRender();
            }
            
            ImGui::Separator();
            ImGui::Text("=== Canvas Resolution ===");
            int width = static_cast<int>(_renderWidth);
            int height = static_cast<int>(_renderHeight);
            
            if (ImGui::DragInt("Width", &width, 10.0f, 100, 4000)) {
                _renderWidth = static_cast<std::uint32_t>(std::max(100, width));
                _recompute = true;
            }
            if (ImGui::DragInt("Height", &height, 10.0f, 100, 4000)) {
                _renderHeight = static_cast<std::uint32_t>(std::max(100, height));
                _recompute = true;
            }
            
            // 预设分辨率快捷按钮
            ImGui::Text("Presets:");
            if (ImGui::Button("800x600", ImVec2(-1, 0))) {
                _renderWidth = 800;
                _renderHeight = 600;
                _recompute = true;
            }
            if (ImGui::Button("1024x768", ImVec2(-1, 0))) {
                _renderWidth = 1024;
                _renderHeight = 768;
                _recompute = true;
            }
            if (ImGui::Button("1920x1080", ImVec2(-1, 0))) {
                _renderWidth = 1920;
                _renderHeight = 1080;
                _recompute = true;
            }
            if (ImGui::Button("2560x1440", ImVec2(-1, 0))) {
                _renderWidth = 2560;
                _renderHeight = 1440;
                _recompute = true;
            }
            
            ImGui::Separator();
            ImGui::Text("=== Renderer Settings ===");
            
            // V2 渲染器开关
            if (ImGui::Checkbox("Use V2 Renderer (High Quality)", &_useV2Renderer)) {
                _recompute = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable the high-quality V2 renderer with:\n"
                                  "- Adaptive Bezier curve tessellation\n"
                                  "- Proper stroke expansion (linecap/linejoin)\n"
                                  "- Multi-sample anti-aliasing\n"
                                  "- Correct fill rules (even-odd/nonzero)");
            }
            
            // V2 渲染器详细设置
            if (_useV2Renderer) {
                ImGui::Indent();
                
                if (ImGui::Checkbox("Anti-Aliasing", &_enableAntiAliasing)) {
                    _recompute = true;
                }
                
                if (_enableAntiAliasing) {
                    const char* aaModeNames[] = { "None", "4x Coverage", "8x Coverage", "16x Coverage", "Analytical" };
                    if (ImGui::Combo("AA Mode", &_aaMode, aaModeNames, IM_ARRAYSIZE(aaModeNames))) {
                        _recompute = true;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "None: No anti-aliasing (fastest)\n"
                            "4x Coverage: 4 samples per pixel\n"
                            "8x Coverage: 8 samples per pixel\n"
                            "16x Coverage: 16 samples per pixel (best quality)\n"
                            "Analytical: Distance-based edge smoothing"
                        );
                    }
                }
                
                if (ImGui::SliderFloat("Curve Flatness", &_flatnessTolerance, 0.1f, 5.0f, "%.2f")) {
                    _recompute = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Lower = smoother curves (more segments)\n"
                                      "Higher = faster rendering (fewer segments)\n"
                                      "Recommended: 0.25 - 1.0");
                }
                
                ImGui::Unindent();
            }
            
            ImGui::Separator();
            ImGui::Text("=== Add Elements ===");
            if (ImGui::Button("Add Rectangle", ImVec2(-1, 0))) {
                AddNewElement(SVGElement::Type::Rect);
            }
            if (ImGui::Button("Add Circle", ImVec2(-1, 0))) {
                AddNewElement(SVGElement::Type::Circle);
            }
            if (ImGui::Button("Add Line", ImVec2(-1, 0))) {
                AddNewElement(SVGElement::Type::Line);
            }
            
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
            
            ImGui::Separator();
            if (ImGui::Button("Clear All", ImVec2(-1, 0))) {
                // 清空所有内容，包括文本编辑器
                _svgDocument.elements.clear();
                _svgDocument.width = 800;
                _svgDocument.height = 600;
                _svgDocument.viewBox = "0 0 800 600";
                
                _selectedElementIndex = -1;
                _hoveredElementIndex = -1;
                _isDragging = false;
                
                // 清空文本编辑器
                std::memset(_svgTextBuffer, 0, sizeof(_svgTextBuffer));
                _svgTextContent = "";
                
                _fileLoaded = false;  // 完全重置状态
                
                UpdateElementBounds();
                UpdateRender();
                
                std::cout << "All content cleared" << std::endl;
            }
        } else if (_currentTab == 1) {
            // Layers 内容
            DrawLayersPanel();
        } else if (_currentTab == 2) {
            // Inspector 内容
            DrawPropertiesPanel();
        }
    }

    void CaseSVGRender::OnSetupCodeUI() {
        DrawCodeEditor();
    }

    void CaseSVGRender::DrawCodeEditor() {
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
        // Calculate available height for the editor
        float buttonHeight = _autoSyncText ? 0 : 30;
        float height = ImGui::GetContentRegionAvail().y - buttonHeight - 10;
        
        bool textChanged = ImGui::InputTextMultiline("##SVGEditor", _svgTextBuffer, 
                                     sizeof(_svgTextBuffer), 
                                     ImVec2(-1, height > 100 ? height : 100), flags);
        
        // 实时检测文本变化
        if (_autoSyncText) {
            std::string currentText(_svgTextBuffer);
            if (currentText != _svgTextContent) {
                _svgTextContent = currentText;
                UpdateSVGFromText();
                UpdateRender();
            }
        } else if (textChanged) {
            // 非实时模式下，只在编辑完成时更新
            _svgTextContent = std::string(_svgTextBuffer);
        }
        
        if (!_autoSyncText && ImGui::Button("Apply Changes", ImVec2(-1, 0))) {
            UpdateSVGFromText();
            UpdateRender();
        }
    }

    void CaseSVGRender::DrawPropertiesPanel() {
        if (!_fileLoaded) {
            ImGui::Text("No file loaded.");
            ImGui::Text("Please load an SVG file first.");
            return;
        }
        
        if (_selectedElementIndex < 0 || _selectedElementIndex >= (int)_svgDocument.elements.size()) {
            ImGui::Text("No element selected.");
            ImGui::Text("Click on an element in the canvas");
            ImGui::Text("or select from the Layers panel.");
            return;
        }

        auto& element = _svgDocument.elements[_selectedElementIndex];
        const auto& bounds = _elementBounds[_selectedElementIndex];

        ImGui::Text("Selected Element");
        ImGui::Separator();
        ImGui::Text("Type: %s", bounds.tagName.c_str());
        
        // ID Editing
        static char idBuf[128];
        static int lastSelectedIndex = -1;
        if (lastSelectedIndex != _selectedElementIndex) {
            std::strncpy(idBuf, element.id.c_str(), sizeof(idBuf) - 1);
            idBuf[sizeof(idBuf) - 1] = '\0';
            lastSelectedIndex = _selectedElementIndex;
        }
        
        if (ImGui::InputText("ID", idBuf, sizeof(idBuf))) {
            element.id = idBuf;
            UpdateTextFromSVG();
            UpdateRender();
        }

        ImGui::Separator();
        ImGui::Text("Style");

        // Fill Color
        if (element.style.fillColor.has_value()) {
            glm::vec4 color = element.style.fillColor.value();
            if (ImGui::ColorEdit4("Fill Color", (float*)&color)) {
                element.style.fillColor = color;
                // 根据元素类型同步到具体元素
                switch (element.type) {
                    case SVGElement::Type::Rect:
                        element.rect.style.fillColor = color;
                        break;
                    case SVGElement::Type::Circle:
                        element.circle.style.fillColor = color;
                        break;
                    case SVGElement::Type::Path:
                        element.path.style.fillColor = color;
                        break;
                    default:
                        break;
                }
                UpdateTextFromSVG();
                UpdateRender();
            }
        } else {
            if (ImGui::Button("Add Fill")) {
                glm::vec4 color(0, 0, 0, 1);
                element.style.fillColor = color;
                switch (element.type) {
                    case SVGElement::Type::Rect:
                        element.rect.style.fillColor = color;
                        break;
                    case SVGElement::Type::Circle:
                        element.circle.style.fillColor = color;
                        break;
                    case SVGElement::Type::Path:
                        element.path.style.fillColor = color;
                        break;
                    default:
                        break;
                }
                UpdateTextFromSVG();
                UpdateRender();
            }
        }

        // Stroke Color
        if (element.style.strokeColor.has_value()) {
            glm::vec4 color = element.style.strokeColor.value();
            if (ImGui::ColorEdit4("Stroke Color", (float*)&color)) {
                element.style.strokeColor = color;
                switch (element.type) {
                    case SVGElement::Type::Rect:
                        element.rect.style.strokeColor = color;
                        break;
                    case SVGElement::Type::Circle:
                        element.circle.style.strokeColor = color;
                        break;
                    case SVGElement::Type::Path:
                        element.path.style.strokeColor = color;
                        break;
                    default:
                        break;
                }
                UpdateTextFromSVG();
                UpdateRender();
            }
        } else {
            if (ImGui::Button("Add Stroke")) {
                glm::vec4 color(0, 0, 0, 1);
                element.style.strokeColor = color;
                switch (element.type) {
                    case SVGElement::Type::Rect:
                        element.rect.style.strokeColor = color;
                        break;
                    case SVGElement::Type::Circle:
                        element.circle.style.strokeColor = color;
                        break;
                    case SVGElement::Type::Path:
                        element.path.style.strokeColor = color;
                        break;
                    default:
                        break;
                }
                UpdateTextFromSVG();
                UpdateRender();
            }
        }

        float strokeWidth = element.style.strokeWidth.value_or(1.0f);
        if (ImGui::DragFloat("Stroke Width", &strokeWidth, 0.1f, 0.0f, 100.0f)) {
            element.style.strokeWidth = strokeWidth;
            switch (element.type) {
                case SVGElement::Type::Rect:
                    element.rect.style.strokeWidth = strokeWidth;
                    break;
                case SVGElement::Type::Circle:
                    element.circle.style.strokeWidth = strokeWidth;
                    break;
                case SVGElement::Type::Path:
                    element.path.style.strokeWidth = strokeWidth;
                    break;
                default:
                    break;
            }
            UpdateTextFromSVG();
            UpdateRender();
        }

        // 元素特定属性
        ImGui::Separator();
        ImGui::Text("Element Properties");
        
        switch (element.type) {
            case SVGElement::Type::Rect: {
                float pos[2] = { element.rect.position.x, element.rect.position.y };
                if (ImGui::DragFloat2("Position", pos, 1.0f)) {
                    element.rect.position = Point2D(pos[0], pos[1]);
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                float size[2] = { element.rect.width, element.rect.height };
                if (ImGui::DragFloat2("Size", size, 1.0f, 0.0f, 10000.0f)) {
                    element.rect.width = size[0];
                    element.rect.height = size[1];
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                break;
            }
            case SVGElement::Type::Circle: {
                float center[2] = { element.circle.center.x, element.circle.center.y };
                if (ImGui::DragFloat2("Center", center, 1.0f)) {
                    element.circle.center = Point2D(center[0], center[1]);
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                float radius = element.circle.radius;
                if (ImGui::DragFloat("Radius", &radius, 1.0f, 0.0f, 10000.0f)) {
                    element.circle.radius = radius;
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                break;
            }
            case SVGElement::Type::Line: {
                float start[2] = { element.line.start.x, element.line.start.y };
                if (ImGui::DragFloat2("Start", start, 1.0f)) {
                    element.line.start = Point2D(start[0], start[1]);
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                float end[2] = { element.line.end.x, element.line.end.y };
                if (ImGui::DragFloat2("End", end, 1.0f)) {
                    element.line.end = Point2D(end[0], end[1]);
                    UpdateTextFromSVG();
                    UpdateRender();
                }
                break;
            }
            default:
                ImGui::Text("(No specific properties)");
                break;
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
        ImGuiIO& io = ImGui::GetIO();
        
        // 检查鼠标是否在画布范围内（基于坐标判断）
        bool isMouseInCanvas = (pos.x >= 0 && pos.x < static_cast<float>(_renderWidth) &&
                                pos.y >= 0 && pos.y < static_cast<float>(_renderHeight));
        
        // 检测新的鼠标点击事件（从未按下变为按下）
        bool mouseJustPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        
        // Draw Overlay UI (工具栏等)
        // 这会在画布上绘制UI元素，并且会影响 io.WantCaptureMouse
        DrawToolbar();
        DrawStatusBar(pos);
        
        // 检查点击是否发生在工具栏等覆盖UI上
        // 如果鼠标悬停在工具栏按钮等UI元素上，则不处理画布点击
        if (!_fileLoaded) return;
        
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
        
        // 鼠标刚刚按下（新的点击事件）
        if (mouseJustPressed && !_isDragging) {
            // 只处理以下情况的点击：
            // 1. 鼠标在画布范围内
            // 2. 没有点击在覆盖UI上（工具栏按钮等）
            // 如果点击在窗口外或其他子窗口，pos会在画布范围外，不会触发
            if (isMouseInCanvas) {
                StartDrag(pos.x, pos.y);
            }
            // 如果点击在画布外或覆盖UI上，不做任何处理，保持当前选择状态
        }
        // 鼠标拖拽中
        else if (isMouseDown && _isDragging) {
            // 如果拖拽已经开始，继续处理（即使鼠标移到UI上）
            UpdateDrag(pos.x, pos.y);
        }
        // 鼠标释放
        else if (!isMouseDown && _isDragging) {
            // 只有当拖拽在进行中时才结束拖拽
            EndDrag();
        }
        // 鼠标移动（未按下且未拖拽）
        else if (!isMouseDown && !_isDragging) {
            // 只有在画布内才更新悬停状态
            if (isMouseInCanvas) {
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
                    
                    // 保存原始点数据（用于路径和直线的控制点操作）
                    _originalPoints.clear();
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
        
        // 将屏幕坐标增量转换为 SVG 坐标增量
        float screenDx = x - _dragStartPos.x;
        float screenDy = y - _dragStartPos.y;
        
        // 使用 viewBox 缩放因子转换增量
        float svgDx = screenDx / _vbScaleX;
        float svgDy = screenDy / _vbScaleY;
        
        if (_dragType == ControlPointType::MoveElement) {
            MoveElement(_selectedElementIndex, svgDx, svgDy);
        }
        else if (_dragType >= ControlPointType::ResizeTopLeft && 
                 _dragType <= ControlPointType::ResizeRight) {
            ResizeElement(_selectedElementIndex, svgDx, svgDy, _dragType);
        }
        else if (_dragType == ControlPointType::BezierControl1 || 
                 _dragType == ControlPointType::BezierControl2) {
            // 贝塞尔控制点拖拽 - 需要转换为 SVG 坐标
            if (_draggedControlPointIndex >= 0) {
                const auto& cp = _controlPoints[_draggedControlPointIndex];
                // 将屏幕坐标转换为 SVG 坐标
                Point2D svgPos = ScreenToSVG(Point2D(x, y));
                UpdateBezierControlPoint(cp.elementIndex, cp.commandIndex, 
                                        _dragType == ControlPointType::BezierControl1 ? 0 : 1,
                                        svgPos.x, svgPos.y);
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
        // 在保存前确保SVG文本是最新的
        UpdateTextFromSVG();
        
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

    void CaseSVGRender::UpdateBackgroundInSVG() {
        if (!_fileLoaded) return;
        
        // 查找背景矩形元素
        int bgIndex = -1;
        for (size_t i = 0; i < _svgDocument.elements.size(); i++) {
            if (_svgDocument.elements[i].id == "background" && 
                _svgDocument.elements[i].type == SVGElement::Type::Rect) {
                bgIndex = i;
                break;
            }
        }
        
        if (bgIndex >= 0) {
            // 更新现有背景
            auto& bgElement = _svgDocument.elements[bgIndex];
            bgElement.rect.position = Point2D(0, 0);
            bgElement.rect.width = _svgDocument.width;
            bgElement.rect.height = _svgDocument.height;
            bgElement.rect.style.fillColor = glm::vec4(_backgroundColor.x, _backgroundColor.y, _backgroundColor.z, _backgroundColor.w);
            bgElement.rect.style.strokeWidth = 0.0f;
        } else {
            // 创建新的背景矩形
            SVGElement bgElement(SVGElement::Type::Rect);
            // 手动初始化rect成员
            new (&bgElement.rect) SVGRect();
            
            bgElement.id = "background";
            bgElement.rect.id = "background";
            bgElement.rect.position = Point2D(0, 0);
            bgElement.rect.width = _svgDocument.width;
            bgElement.rect.height = _svgDocument.height;
            bgElement.rect.style.fillColor = glm::vec4(_backgroundColor.x, _backgroundColor.y, _backgroundColor.z, _backgroundColor.w);
            bgElement.rect.style.strokeWidth = 0.0f;
            
            // 在最前面插入新背景
            _svgDocument.elements.insert(_svgDocument.elements.begin(), std::move(bgElement));
            // 更新选中索引
            if (_selectedElementIndex >= 0) {
                _selectedElementIndex++;
            }
        }
        
        UpdateElementBounds();
        std::cout << "Background updated" << std::endl;
    }

    void CaseSVGRender::AddNewElement(SVGElement::Type type) {
        if (!_fileLoaded) return;
        
        SVGElement newElement(type);
        
        // 根据类型初始化union成员和默认值
        switch (type) {
            case SVGElement::Type::Rect: {
                new (&newElement.rect) SVGRect();
                newElement.id = "rect_" + std::to_string(_svgDocument.elements.size());
                newElement.rect.id = newElement.id;
                newElement.rect.position = Point2D(100, 100);
                newElement.rect.width = 100;
                newElement.rect.height = 100;
                newElement.rect.style.fillColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                newElement.rect.style.strokeColor = glm::vec4(0, 0, 0, 1.0f);
                newElement.rect.style.strokeWidth = 2.0f;
                break;
            }
            case SVGElement::Type::Circle: {
                new (&newElement.circle) SVGCircle();
                newElement.id = "circle_" + std::to_string(_svgDocument.elements.size());
                newElement.circle.id = newElement.id;
                newElement.circle.center = Point2D(200, 200);
                newElement.circle.radius = 50;
                newElement.circle.style.fillColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
                newElement.circle.style.strokeColor = glm::vec4(0, 0, 0, 1.0f);
                newElement.circle.style.strokeWidth = 2.0f;
                break;
            }
            case SVGElement::Type::Line: {
                new (&newElement.line) SVGLine();
                newElement.id = "line_" + std::to_string(_svgDocument.elements.size());
                newElement.line.id = newElement.id;
                newElement.line.start = Point2D(50, 50);
                newElement.line.end = Point2D(150, 150);
                newElement.line.style.strokeColor = glm::vec4(0, 0, 0, 1.0f);
                newElement.line.style.strokeWidth = 2.0f;
                break;
            }
            default:
                return;
        }
        
        _svgDocument.elements.push_back(std::move(newElement));
        _selectedElementIndex = _svgDocument.elements.size() - 1;
        
        UpdateElementBounds();
        UpdateTextFromSVG();
        UpdateRender();
        
        std::cout << "New element added: " << _svgDocument.elements.back().id << std::endl;
    }

    void CaseSVGRender::UpdateRender() {
        if (!_fileLoaded) {
            // 创建一个空白图像，使用当前的背景颜色
            glm::vec3 bgColor(_backgroundColor.x, _backgroundColor.y, _backgroundColor.z);
            _image = Common::CreatePureImageRGB(_renderWidth, _renderHeight, bgColor);
        } else {
            // 渲染SVG
            RenderWithHighlight(_hoveredElementIndex);
        }

        _texture.Update(_image);
    }

    void CaseSVGRender::UpdateElementBounds() {
        _elementBounds.clear();
        
        // 首先更新 viewBox 变换参数
        UpdateViewBoxTransform();
        
        for (size_t i = 0; i < _svgDocument.elements.size(); i++) {
            const auto& element = _svgDocument.elements[i];
            ElementBounds bounds;
            bounds.elementIndex = static_cast<int>(i);
            bounds.id = element.id;
            
            // 临时存储 SVG 坐标系中的边界
            float svgMinX, svgMinY, svgMaxX, svgMaxY;
            
            // 根据元素类型计算边界（在 SVG 坐标系中）
            switch (element.type) {
                case SVGElement::Type::Circle: {
                    Point2D center = element.circle.transform.TransformPoint(element.circle.center);
                    float r = element.circle.radius;
                    svgMinX = center.x - r;
                    svgMinY = center.y - r;
                    svgMaxX = center.x + r;
                    svgMaxY = center.y + r;
                    bounds.tagName = "circle";
                    break;
                }
                case SVGElement::Type::Rect: {
                    Point2D pos = element.rect.transform.TransformPoint(element.rect.position);
                    svgMinX = pos.x;
                    svgMinY = pos.y;
                    svgMaxX = pos.x + element.rect.width;
                    svgMaxY = pos.y + element.rect.height;
                    bounds.tagName = "rect";
                    break;
                }
                case SVGElement::Type::Line: {
                    Point2D start = element.line.transform.TransformPoint(element.line.start);
                    Point2D end = element.line.transform.TransformPoint(element.line.end);
                    svgMinX = std::min(start.x, end.x);
                    svgMinY = std::min(start.y, end.y);
                    svgMaxX = std::max(start.x, end.x);
                    svgMaxY = std::max(start.y, end.y);
                    bounds.tagName = "line";
                    break;
                }
                case SVGElement::Type::Path: {
                    auto vertices = element.path.GetVertices();
                    if (!vertices.empty()) {
                        // 应用元素的变换到顶点
                        svgMinX = svgMaxX = vertices[0].x;
                        svgMinY = svgMaxY = vertices[0].y;
                        for (const auto& v : vertices) {
                            Point2D transformed = element.transform.TransformPoint(v);
                            svgMinX = std::min(svgMinX, transformed.x);
                            svgMinY = std::min(svgMinY, transformed.y);
                            svgMaxX = std::max(svgMaxX, transformed.x);
                            svgMaxY = std::max(svgMaxY, transformed.y);
                        }
                    } else {
                        svgMinX = svgMinY = svgMaxX = svgMaxY = 0;
                    }
                    bounds.tagName = "path";
                    break;
                }
                case SVGElement::Type::Ellipse: {
                    Point2D center = element.ellipse.transform.TransformPoint(element.ellipse.center);
                    svgMinX = center.x - element.ellipse.rx;
                    svgMinY = center.y - element.ellipse.ry;
                    svgMaxX = center.x + element.ellipse.rx;
                    svgMaxY = center.y + element.ellipse.ry;
                    bounds.tagName = "ellipse";
                    break;
                }
                case SVGElement::Type::Text: {
                    Point2D pos = element.text.transform.TransformPoint(element.text.position);
                    float w = element.text.text.length() * element.text.fontSize * 0.6f;
                    float h = element.text.fontSize;
                    svgMinX = pos.x;
                    svgMinY = pos.y;
                    svgMaxX = pos.x + w;
                    svgMaxY = pos.y + h;
                    bounds.tagName = "text";
                    break;
                }
                default:
                    continue;
            }
            
            // 将 SVG 坐标转换为屏幕/渲染坐标
            Point2D screenMin = SVGToScreen(Point2D(svgMinX, svgMinY));
            Point2D screenMax = SVGToScreen(Point2D(svgMaxX, svgMaxY));
            
            bounds.minX = std::min(screenMin.x, screenMax.x);
            bounds.minY = std::min(screenMin.y, screenMax.y);
            bounds.maxX = std::max(screenMin.x, screenMax.x);
            bounds.maxY = std::max(screenMin.y, screenMax.y);
            
            _elementBounds.push_back(bounds);
        }
    }

    // 更新 viewBox 变换参数
    void CaseSVGRender::UpdateViewBoxTransform() {
        float vbX, vbY, vbW, vbH;
        if (_svgDocument.ParseViewBox(vbX, vbY, vbW, vbH) && vbW > 0 && vbH > 0) {
            _vbScaleX = static_cast<float>(_renderWidth) / vbW;
            _vbScaleY = static_cast<float>(_renderHeight) / vbH;
            _vbOffsetX = -vbX * _vbScaleX;
            _vbOffsetY = -vbY * _vbScaleY;
            _hasViewBox = true;
        } else {
            // 没有 viewBox，使用 1:1 映射
            _vbScaleX = 1.0f;
            _vbScaleY = 1.0f;
            _vbOffsetX = 0.0f;
            _vbOffsetY = 0.0f;
            _hasViewBox = false;
        }
    }

    // 将 SVG 坐标转换为屏幕/渲染坐标
    Point2D CaseSVGRender::SVGToScreen(const Point2D& svgPoint) const {
        return Point2D(
            svgPoint.x * _vbScaleX + _vbOffsetX,
            svgPoint.y * _vbScaleY + _vbOffsetY
        );
    }

    // 将屏幕/渲染坐标转换为 SVG 坐标
    Point2D CaseSVGRender::ScreenToSVG(const Point2D& screenPoint) const {
        return Point2D(
            (screenPoint.x - _vbOffsetX) / _vbScaleX,
            (screenPoint.y - _vbOffsetY) / _vbScaleY
        );
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
        // Choose renderer based on settings
        if (_useV2Renderer) {
            // Configure V2 renderer
            _svgRendererV2.SetBackgroundColor(glm::vec4(_backgroundColor.x, _backgroundColor.y, 
                                                        _backgroundColor.z, _backgroundColor.w));
            _svgRendererV2.SetAntiAliasing(_enableAntiAliasing);
            _svgRendererV2.SetFlatnessTolerance(_flatnessTolerance);
            
            // Set AA mode
            ScanlineRasterizer::AAMode aaMode = ScanlineRasterizer::AAMode::Coverage4x;
            switch (_aaMode) {
                case 0: aaMode = ScanlineRasterizer::AAMode::None; break;
                case 1: aaMode = ScanlineRasterizer::AAMode::Coverage4x; break;
                case 2: aaMode = ScanlineRasterizer::AAMode::Coverage8x; break;
                case 3: aaMode = ScanlineRasterizer::AAMode::Coverage16x; break;
                case 4: aaMode = ScanlineRasterizer::AAMode::Analytical; break;
            }
            _svgRendererV2.SetAAMode(aaMode);
            
            _image = _svgRendererV2.RenderSVG(_svgDocument, _renderWidth, _renderHeight);
        } else {
            // Use original renderer
            _image = _svgRenderer.RenderSVG(_svgDocument, _renderWidth, _renderHeight);
        }
        
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
                // 保存新文档的属性（在move之前）
                float newWidth = newDocument.width;
                float newHeight = newDocument.height;
                std::string newViewBox = newDocument.viewBox;
                size_t elementCount = newDocument.elements.size();
                
                // 移动文档数据
                _svgDocument = std::move(newDocument);
                _svgTextContent = newContent;
                
                // 输出解析信息到控制台
                std::cout << "\n========== SVG Parse Result ==========" << std::endl;
                std::cout << "Document Size: " << newWidth << "x" << newHeight << std::endl;
                if (!newViewBox.empty()) {
                    std::cout << "ViewBox: " << newViewBox << std::endl;
                }
                std::cout << "Total Elements: " << elementCount << std::endl;
                std::cout << "----------------------------------------" << std::endl;
                
                // 统计各类型元素数量
                size_t pathCount = 0, circleCount = 0, rectCount = 0, lineCount = 0;
                size_t ellipseCount = 0, textCount = 0, groupCount = 0;
                
                // 遍历并输出每个元素的详细信息
                for (size_t i = 0; i < _svgDocument.elements.size(); ++i) {
                    const auto& elem = _svgDocument.elements[i];
                    
                    // 统计类型
                    switch (elem.type) {
                        case SVGElement::Type::Path: pathCount++; break;
                        case SVGElement::Type::Circle: circleCount++; break;
                        case SVGElement::Type::Rect: rectCount++; break;
                        case SVGElement::Type::Line: lineCount++; break;
                        case SVGElement::Type::Ellipse: ellipseCount++; break;
                        case SVGElement::Type::Text: textCount++; break;
                        case SVGElement::Type::Group: groupCount++; break;
                    }
                    
                    // 输出元素信息
                    std::cout << "\n[" << i << "] ";
                    switch (elem.type) {
                        case SVGElement::Type::Path:
                            std::cout << "Path";
                            if (!elem.path.id.empty()) std::cout << " (id: " << elem.path.id << ")";
                            std::cout << " - Commands: " << elem.path.commands.size();
                            break;
                        case SVGElement::Type::Circle:
                            std::cout << "Circle";
                            if (!elem.circle.id.empty()) std::cout << " (id: " << elem.circle.id << ")";
                            std::cout << " - Center: (" << elem.circle.center.x << ", " << elem.circle.center.y 
                                     << "), Radius: " << elem.circle.radius;
                            break;
                        case SVGElement::Type::Rect:
                            std::cout << "Rect";
                            if (!elem.rect.id.empty()) std::cout << " (id: " << elem.rect.id << ")";
                            std::cout << " - Pos: (" << elem.rect.position.x << ", " << elem.rect.position.y 
                                     << "), Size: " << elem.rect.width << "x" << elem.rect.height;
                            break;
                        case SVGElement::Type::Line:
                            std::cout << "Line";
                            if (!elem.line.id.empty()) std::cout << " (id: " << elem.line.id << ")";
                            std::cout << " - (" << elem.line.start.x << ", " << elem.line.start.y 
                                     << ") to (" << elem.line.end.x << ", " << elem.line.end.y << ")";
                            break;
                        case SVGElement::Type::Ellipse:
                            std::cout << "Ellipse";
                            if (!elem.ellipse.id.empty()) std::cout << " (id: " << elem.ellipse.id << ")";
                            std::cout << " - Center: (" << elem.ellipse.center.x << ", " << elem.ellipse.center.y 
                                     << "), Rx: " << elem.ellipse.rx << ", Ry: " << elem.ellipse.ry;
                            break;
                        case SVGElement::Type::Text:
                            std::cout << "Text";
                            if (!elem.text.id.empty()) std::cout << " (id: " << elem.text.id << ")";
                            std::cout << " - \"" << elem.text.text << "\" at (" 
                                     << elem.text.position.x << ", " << elem.text.position.y << ")";
                            break;
                        case SVGElement::Type::Group:
                            std::cout << "Group";
                            if (!elem.id.empty()) std::cout << " (id: " << elem.id << ")";
                            std::cout << " - Children: " << elem.children.size();
                            break;
                    }
                    
                    // 输出样式信息
                    if (elem.style.fillColor.has_value()) {
                        const auto& c = elem.style.fillColor.value();
                        std::cout << ", Fill: (" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
                    }
                    if (elem.style.strokeColor.has_value()) {
                        const auto& c = elem.style.strokeColor.value();
                        std::cout << ", Stroke: (" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
                    }
                    if (elem.style.strokeWidth.has_value()) {
                        std::cout << ", StrokeWidth: " << elem.style.strokeWidth.value();
                    }
                    
                    // 输出变换信息
                    if (elem.transform.matrix != glm::mat3(1.0f)) {
                        std::cout << ", Has Transform";
                    }
                }
                
                // 输出统计信息
                std::cout << "\n----------------------------------------" << std::endl;
                std::cout << "Element Type Summary:" << std::endl;
                if (pathCount > 0) std::cout << "  Paths: " << pathCount << std::endl;
                if (circleCount > 0) std::cout << "  Circles: " << circleCount << std::endl;
                if (rectCount > 0) std::cout << "  Rects: " << rectCount << std::endl;
                if (lineCount > 0) std::cout << "  Lines: " << lineCount << std::endl;
                if (ellipseCount > 0) std::cout << "  Ellipses: " << ellipseCount << std::endl;
                if (textCount > 0) std::cout << "  Texts: " << textCount << std::endl;
                if (groupCount > 0) std::cout << "  Groups: " << groupCount << std::endl;
                std::cout << "========================================\n" << std::endl;
                
                _fileLoaded = true;  // 解析成功后标记为已加载
                UpdateElementBounds();
                UpdateControlPoints();
            } else {
                std::cerr << "\n========== SVG Parse Failed ==========" << std::endl;
                std::cerr << "Failed to parse SVG from text editor" << std::endl;
                std::cerr << "========================================\n" << std::endl;
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
        
        oss << "<svg width=\"" << _svgDocument.width << "\" height=\"" << _svgDocument.height << "\"";
        if (!_svgDocument.viewBox.empty()) {
            oss << " viewBox=\"" << _svgDocument.viewBox << "\"";
        }
        oss << " xmlns=\"http://www.w3.org/2000/svg\">\n";
        
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
                
                case SVGElement::Type::Ellipse: {
                    oss << "<ellipse";
                    if (!elem.ellipse.id.empty()) oss << " id=\"" << elem.ellipse.id << "\"";
                    oss << " cx=\"" << elem.ellipse.center.x << "\"";
                    oss << " cy=\"" << elem.ellipse.center.y << "\"";
                    oss << " rx=\"" << elem.ellipse.rx << "\"";
                    oss << " ry=\"" << elem.ellipse.ry << "\"";
                    if (elem.ellipse.style.fillColor) {
                        auto c = *elem.ellipse.style.fillColor;
                        oss << " fill=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.ellipse.style.strokeColor) {
                        auto c = *elem.ellipse.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.ellipse.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.ellipse.style.strokeWidth << "\"";
                    }
                    oss << "/>\n";
                    break;
                }
                
                case SVGElement::Type::Text: {
                    oss << "<text";
                    if (!elem.text.id.empty()) oss << " id=\"" << elem.text.id << "\"";
                    oss << " x=\"" << elem.text.position.x << "\"";
                    oss << " y=\"" << elem.text.position.y << "\"";
                    if (elem.text.fontSize > 0) {
                        oss << " font-size=\"" << elem.text.fontSize << "\"";
                    }
                    if (!elem.text.fontFamily.empty()) {
                        oss << " font-family=\"" << elem.text.fontFamily << "\"";
                    }
                    if (elem.text.style.fillColor) {
                        auto c = *elem.text.style.fillColor;
                        oss << " fill=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.text.style.strokeColor) {
                        auto c = *elem.text.style.strokeColor;
                        oss << " stroke=\"rgb(" << (int)(c.r*255) << "," << (int)(c.g*255) << "," << (int)(c.b*255) << ")\"";
                    }
                    if (elem.text.style.strokeWidth) {
                        oss << " stroke-width=\"" << *elem.text.style.strokeWidth << "\"";
                    }
                    oss << ">" << elem.text.text << "</text>\n";
                    break;
                }
                
                case SVGElement::Type::Group:
                    // Group已经被展平处理，不需要单独生成
                    break;
                
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
        
        // 添加调整大小的控制点（bounds 已经是屏幕坐标）
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
            // 直线的两个端点 - 需要转换为屏幕坐标
            Point2D svgStart = elem.line.transform.TransformPoint(elem.line.start);
            Point2D svgEnd = elem.line.transform.TransformPoint(elem.line.end);
            Point2D screenStart = SVGToScreen(svgStart);
            Point2D screenEnd = SVGToScreen(svgEnd);
            _controlPoints.push_back({screenStart, ControlPointType::ResizeTopLeft, _selectedElementIndex, -1});
            _controlPoints.push_back({screenEnd, ControlPointType::ResizeBottomRight, _selectedElementIndex, -1});
        }
        
        // 贝塞尔曲线的控制点 - 需要转换为屏幕坐标
        if (elem.type == SVGElement::Type::Path) {
            Point2D currentPos(0, 0);
            for (size_t i = 0; i < elem.path.commands.size(); i++) {
                const auto& cmd = elem.path.commands[i];
                
                if (cmd.type == PathCommandType::CurveTo && cmd.points.size() >= 3) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    Point2D p3 = cmd.relative ? currentPos + cmd.points[2] : cmd.points[2];
                    
                    // 转换为屏幕坐标
                    Point2D screenP1 = SVGToScreen(p1);
                    Point2D screenP2 = SVGToScreen(p2);
                    
                    _controlPoints.push_back({screenP1, ControlPointType::BezierControl1, _selectedElementIndex, (int)i});
                    _controlPoints.push_back({screenP2, ControlPointType::BezierControl2, _selectedElementIndex, (int)i});
                    
                    currentPos = p3;
                }
                else if (cmd.type == PathCommandType::QuadCurveTo && cmd.points.size() >= 2) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    
                    // 转换为屏幕坐标
                    Point2D screenP1 = SVGToScreen(p1);
                    _controlPoints.push_back({screenP1, ControlPointType::BezierControl1, _selectedElementIndex, (int)i});
                    
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

