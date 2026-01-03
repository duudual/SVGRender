#include "SVGParser.h"
#include <tinyxml2.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>

namespace VCX::Labs::SVG {

    SVGParser::SVGParser() : _xmlDoc(std::make_unique<tinyxml2::XMLDocument>()) {}

    SVGParser::~SVGParser() = default;

    bool SVGParser::ParseFile(const std::string& filename, SVGDocument& document) {
        tinyxml2::XMLError error = _xmlDoc->LoadFile(filename.c_str());
        if (error != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to load SVG file: " << filename << std::endl;
            return false;
        }

        // 尝试查找svg元素（处理带有XML声明的SVG）
        // FirstChildElement会跳过XML声明和注释
        tinyxml2::XMLElement* svgElement = _xmlDoc->FirstChildElement("svg");
        // //打印出svgElement的内容
        // if (svgElement) {
        //     std::cout << "svgElement: " << svgElement->GetText() << std::endl;
        // }
        // else{
        //     std::cerr << "wrong" << std::endl;
        // }
        
        if (!svgElement) {
            // 某些情况下，尝试使用RootElement
            svgElement = _xmlDoc->RootElement();
            if (!svgElement || std::string(svgElement->Name()) != "svg") {
                std::cerr << "No SVG element found in file" << std::endl;
                return false;
            }
        }

        return ParseSVGElement(svgElement, document);
    }

    bool SVGParser::ParseString(const std::string& svgContent, SVGDocument& document) {
        tinyxml2::XMLError error = _xmlDoc->Parse(svgContent.c_str());
        if (error != tinyxml2::XML_SUCCESS) {
            //打印出error详细信息
            std::cout << "error: " << error << std::endl;
            std::cerr << "Failed to parse SVG content" << std::endl;
            return false;
        }

        // 尝试查找svg元素（处理带有XML声明的SVG）
        // FirstChildElement会跳过XML声明和注释
        tinyxml2::XMLElement* svgElement = _xmlDoc->FirstChildElement("svg");
        
        if (!svgElement) {
            // 某些情况下，尝试使用RootElement
            svgElement = _xmlDoc->RootElement();
            if (!svgElement || std::string(svgElement->Name()) != "svg") {
                std::cerr << "No SVG element found in content" << std::endl;
                return false;
            }
        }

        return ParseSVGElement(svgElement, document);
    }

    bool SVGParser::ParseSVGElement(tinyxml2::XMLElement* svgElement, SVGDocument& document) {
        // 解析基本属性
        document.viewBox = GetAttribute(svgElement, "viewBox");
        
        // 尝试从属性获取 width/height
        std::string widthStr = GetAttribute(svgElement, "width");
        std::string heightStr = GetAttribute(svgElement, "height");
        
        // 如果没有 width/height，尝试从 viewBox 推断
        if (widthStr.empty() || heightStr.empty()) {
            float vbX, vbY, vbW, vbH;
            if (!document.viewBox.empty()) {
                std::istringstream iss(document.viewBox);
                if (iss >> vbX >> vbY >> vbW >> vbH) {
                    if (widthStr.empty()) document.width = vbW;
                    if (heightStr.empty()) document.height = vbH;
                }
            }
        }
        
        // 如果有明确的 width/height 属性，使用它们
        if (!widthStr.empty()) {
            document.width = ParseLength(widthStr, 800.0f);
        } else if (document.width == 0) {
            document.width = 800.0f;  // 最后的默认值
        }
        if (!heightStr.empty()) {
            document.height = ParseLength(heightStr, 600.0f);
        } else if (document.height == 0) {
            document.height = 600.0f;  // 最后的默认值
        }

        // 解析子元素
        for (tinyxml2::XMLElement* child = svgElement->FirstChildElement(); child; child = child->NextSiblingElement()) {
            std::string tagName = child->Name();
            
            // 跳过元数据元素
            if (tagName == "title" || tagName == "desc" || tagName == "metadata" || tagName == "defs") {
                continue;
            }
            
            SVGElement element(SVGElement::Type::Path); // 临时类型
            bool parsed = false;

            if (tagName == "path") {
                element.type = SVGElement::Type::Path;
                parsed = ParsePathElement(child, element);
            } else         if (tagName == "circle") {
                element.type = SVGElement::Type::Circle;
                parsed = ParseCircleElement(child, element);
            } else if (tagName == "ellipse") {
                element.type = SVGElement::Type::Ellipse;
                parsed = ParseEllipseElement(child, element);
            } else if (tagName == "rect") {
                element.type = SVGElement::Type::Rect;
                parsed = ParseRectElement(child, element);
            } else if (tagName == "line") {
                element.type = SVGElement::Type::Line;
                parsed = ParseLineElement(child, element);
            } else if (tagName == "text") {
                element.type = SVGElement::Type::Text;
                parsed = ParseTextElement(child, element);
            } else if (tagName == "g") {
                // 展平处理<g>标签：将子元素直接添加到document，应用组的变换和样式
                ParseGroupElementFlattened(child, document, Transform2D(), SVGStyle());
                continue; // 已处理，继续下一个元素
            }

            if (parsed) {
                document.elements.push_back(std::move(element));
            }
        }

        return true;
    }

    void SVGParser::ParseGroupElementFlattened(tinyxml2::XMLElement* element, SVGDocument& document, const Transform2D& parentTransform, const SVGStyle& parentStyle) {
        // 获取当前<g>标签的变换和样式
        Transform2D currentTransform = ParseTransform(GetAttribute(element, "transform"));
        SVGStyle currentStyle = ParseStyle(element);
        
        // 组合父级变换（使用矩阵乘法）
        Transform2D combinedTransform = parentTransform * currentTransform;
        
        // 继承样式（如果子元素没有定义的话）
        SVGStyle combinedStyle = currentStyle;
        if (!combinedStyle.fillColor.has_value() && parentStyle.fillColor.has_value()) {
            combinedStyle.fillColor = parentStyle.fillColor;
        }
        if (!combinedStyle.strokeColor.has_value() && parentStyle.strokeColor.has_value()) {
            combinedStyle.strokeColor = parentStyle.strokeColor;
        }
        if (!combinedStyle.strokeWidth.has_value() && parentStyle.strokeWidth.has_value()) {
            combinedStyle.strokeWidth = parentStyle.strokeWidth;
        }
        
        // 递归解析子元素
        for (tinyxml2::XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
            std::string tagName = child->Name();
            
            // 跳过元数据元素
            if (tagName == "title" || tagName == "desc" || tagName == "metadata" || tagName == "defs") {
                continue;
            }
            
            if (tagName == "g") {
                // 递归处理嵌套的<g>标签
                ParseGroupElementFlattened(child, document, combinedTransform, combinedStyle);
            } else {
                // 根据标签类型创建对应的元素
                SVGElement::Type elementType = SVGElement::Type::Path;
                if (tagName == "circle") {
                    elementType = SVGElement::Type::Circle;
                } else if (tagName == "ellipse") {
                    elementType = SVGElement::Type::Ellipse;
                } else if (tagName == "rect") {
                    elementType = SVGElement::Type::Rect;
                } else if (tagName == "line") {
                    elementType = SVGElement::Type::Line;
                } else if (tagName == "text") {
                    elementType = SVGElement::Type::Text;
                } else if (tagName != "path") {
                    // 未知标签类型，跳过
                    continue;
                }
                
                SVGElement childElement(elementType);
                bool parsed = false;
                
                if (tagName == "path") {
                    parsed = ParsePathElement(child, childElement);
                } else if (tagName == "circle") {
                    parsed = ParseCircleElement(child, childElement);
                } else if (tagName == "ellipse") {
                    parsed = ParseEllipseElement(child, childElement);
                } else if (tagName == "rect") {
                    parsed = ParseRectElement(child, childElement);
                } else if (tagName == "line") {
                    parsed = ParseLineElement(child, childElement);
                } else if (tagName == "text") {
                    parsed = ParseTextElement(child, childElement);
                }
                
                if (parsed) {
                    // 应用组合变换到元素（使用矩阵乘法）
                    childElement.transform = combinedTransform * childElement.transform;
                    
                    // 继承样式（如果元素自身没有定义）
                    if (!childElement.style.fillColor.has_value() && combinedStyle.fillColor.has_value()) {
                        childElement.style.fillColor = combinedStyle.fillColor;
                    }
                    if (!childElement.style.strokeColor.has_value() && combinedStyle.strokeColor.has_value()) {
                        childElement.style.strokeColor = combinedStyle.strokeColor;
                    }
                    if (!childElement.style.strokeWidth.has_value() && combinedStyle.strokeWidth.has_value()) {
                        childElement.style.strokeWidth = combinedStyle.strokeWidth;
                    }
                    
                    document.elements.push_back(std::move(childElement));
                }
            }
        }
    }

    bool SVGParser::ParsePathElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.path) SVGPath();

        svgElement.path.id = GetAttribute(element, "id");
        svgElement.path.style = ParseStyle(element);
        svgElement.path.transform = ParseTransform(GetAttribute(element, "transform"));

        std::string pathData = GetAttribute(element, "d");
        if (pathData.empty()) return false;

        return ParsePathData(pathData, svgElement.path.commands);
    }

    bool SVGParser::ParseCircleElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.circle) SVGCircle();

        svgElement.circle.id = GetAttribute(element, "id");
        svgElement.circle.center.x = ParseLength(GetAttribute(element, "cx", "0"));
        svgElement.circle.center.y = ParseLength(GetAttribute(element, "cy", "0"));
        svgElement.circle.radius = ParseLength(GetAttribute(element, "r", "0"));
        svgElement.circle.style = ParseStyle(element);
        svgElement.circle.transform = ParseTransform(GetAttribute(element, "transform"));

        return true;
    }

    bool SVGParser::ParseEllipseElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.ellipse) SVGEllipse();

        svgElement.ellipse.id = GetAttribute(element, "id");
        svgElement.ellipse.center.x = ParseLength(GetAttribute(element, "cx", "0"));
        svgElement.ellipse.center.y = ParseLength(GetAttribute(element, "cy", "0"));
        svgElement.ellipse.rx = ParseLength(GetAttribute(element, "rx", "0"));
        svgElement.ellipse.ry = ParseLength(GetAttribute(element, "ry", "0"));
        svgElement.ellipse.style = ParseStyle(element);
        svgElement.ellipse.transform = ParseTransform(GetAttribute(element, "transform"));

        return true;
    }

    bool SVGParser::ParseRectElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.rect) SVGRect();

        svgElement.rect.id = GetAttribute(element, "id");
        svgElement.rect.position.x = ParseLength(GetAttribute(element, "x", "0"));
        svgElement.rect.position.y = ParseLength(GetAttribute(element, "y", "0"));
        svgElement.rect.width = ParseLength(GetAttribute(element, "width", "0"));
        svgElement.rect.height = ParseLength(GetAttribute(element, "height", "0"));
        svgElement.rect.rx = ParseLength(GetAttribute(element, "rx", "0"));
        svgElement.rect.ry = ParseLength(GetAttribute(element, "ry", "0"));
        svgElement.rect.style = ParseStyle(element);
        svgElement.rect.transform = ParseTransform(GetAttribute(element, "transform"));

        return true;
    }

    bool SVGParser::ParseLineElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.line) SVGLine();

        svgElement.line.id = GetAttribute(element, "id");
        svgElement.line.start.x = ParseLength(GetAttribute(element, "x1", "0"));
        svgElement.line.start.y = ParseLength(GetAttribute(element, "y1", "0"));
        svgElement.line.end.x = ParseLength(GetAttribute(element, "x2", "0"));
        svgElement.line.end.y = ParseLength(GetAttribute(element, "y2", "0"));
        svgElement.line.style = ParseStyle(element);
        svgElement.line.transform = ParseTransform(GetAttribute(element, "transform"));

        return true;
    }

    bool SVGParser::ParseTextElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        new (&svgElement.text) SVGText();

        svgElement.text.id = GetAttribute(element, "id");
        svgElement.text.position.x = ParseLength(GetAttribute(element, "x", "0"));
        svgElement.text.position.y = ParseLength(GetAttribute(element, "y", "0"));
        svgElement.text.text = element->GetText() ? element->GetText() : "";
        svgElement.text.fontSize = ParseLength(GetAttribute(element, "font-size", "12"), 12.0f);
        svgElement.text.fontFamily = GetAttribute(element, "font-family", "Arial");
        svgElement.text.style = ParseStyle(element);
        svgElement.text.transform = ParseTransform(GetAttribute(element, "transform"));

        return true;
    }

    bool SVGParser::ParseGroupElement(tinyxml2::XMLElement* element, SVGElement& svgElement) {
        svgElement.id = GetAttribute(element, "id");
        svgElement.style = ParseStyle(element);
        svgElement.transform = ParseTransform(GetAttribute(element, "transform"));

        // 递归解析子元素
        for (tinyxml2::XMLElement* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
            SVGElement childElement(SVGElement::Type::Path);
            std::string tagName = child->Name();
            bool parsed = false;

            if (tagName == "path") {
                childElement.type = SVGElement::Type::Path;
                parsed = ParsePathElement(child, childElement);
            } else if (tagName == "circle") {
                childElement.type = SVGElement::Type::Circle;
                parsed = ParseCircleElement(child, childElement);
            } else if (tagName == "rect") {
                childElement.type = SVGElement::Type::Rect;
                parsed = ParseRectElement(child, childElement);
            } else if (tagName == "line") {
                childElement.type = SVGElement::Type::Line;
                parsed = ParseLineElement(child, childElement);
            } else if (tagName == "text") {
                childElement.type = SVGElement::Type::Text;
                parsed = ParseTextElement(child, childElement);
            } else if (tagName == "g") {
                // 递归处理嵌套的 <g> 标签
                childElement.type = SVGElement::Type::Group;
                parsed = ParseGroupElement(child, childElement);
            }

            if (parsed) {
                svgElement.children.push_back(std::move(childElement));
            }
        }

        return true;
    }

    SVGStyle SVGParser::ParseStyle(tinyxml2::XMLElement* element) {
        SVGStyle style;

        // 首先解析style属性（内联CSS样式）
        if (HasAttribute(element, "style")) {
            std::string styleStr = GetAttribute(element, "style");
            std::istringstream iss(styleStr);
            std::string prop;
            
            while (std::getline(iss, prop, ';')) {
                // 分割属性名和值
                size_t colonPos = prop.find(':');
                if (colonPos != std::string::npos) {
                    std::string name = prop.substr(0, colonPos);
                    std::string value = prop.substr(colonPos + 1);
                    
                    // 去除前后空白
                    name.erase(0, name.find_first_not_of(" \t"));
                    name.erase(name.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    // 解析各种CSS属性
                    if (name == "fill" && value != "none") {
                        style.fillColor = ParseColor(value);
                    } else if (name == "stroke" && value != "none") {
                        style.strokeColor = ParseColor(value);
                    } else if (name == "stroke-width") {
                        style.strokeWidth = ParseLength(value, 1.0f);
                    } else if (name == "opacity") {
                        style.opacity = std::stof(value);
                    } else if (name == "fill-opacity") {
                        style.fillOpacity = std::stof(value);
                    } else if (name == "stroke-opacity") {
                        style.strokeOpacity = std::stof(value);
                    } else if (name == "fill-rule") {
                        style.fillRule = value;
                    } else if (name == "stroke-linecap") {
                        style.strokeLineCap = value;
                    } else if (name == "stroke-linejoin") {
                        style.strokeLineJoin = value;
                    } else if (name == "stroke-miterlimit") {
                        style.strokeMiterLimit = std::stof(value);
                    } else if (name == "stroke-dasharray" && value != "none") {
                        // Parse comma/space separated values
                        std::vector<float> dashValues;
                        std::istringstream dashStream(value);
                        std::string dashToken;
                        while (std::getline(dashStream, dashToken, ',')) {
                            // Also handle space-separated values
                            std::istringstream tokenStream(dashToken);
                            float v;
                            while (tokenStream >> v) {
                                dashValues.push_back(v);
                            }
                        }
                        if (!dashValues.empty()) {
                            style.strokeDashArray = dashValues;
                        }
                    } else if (name == "stroke-dashoffset") {
                        style.strokeDashOffset = ParseLength(value, 0.0f);
                    } else if (name == "fill" && value == "none") {
                        style.fillNone = true;
                    } else if (name == "stroke" && value == "none") {
                        style.strokeNone = true;
                    }
                }
            }
        }

        // 解析fill属性（属性优先级高于style中的设置）
        if (HasAttribute(element, "fill")) {
            std::string fill = GetAttribute(element, "fill");
            if (fill == "none") {
                style.fillNone = true;
            } else {
                style.fillColor = ParseColor(fill);
            }
        }

        // 解析stroke属性
        if (HasAttribute(element, "stroke")) {
            std::string stroke = GetAttribute(element, "stroke");
            if (stroke == "none") {
                style.strokeNone = true;
            } else {
                style.strokeColor = ParseColor(stroke);
            }
        }

        // 解析stroke-width属性
        if (HasAttribute(element, "stroke-width")) {
            style.strokeWidth = ParseLength(GetAttribute(element, "stroke-width"), 1.0f);
        }

        // 解析opacity属性
        if (HasAttribute(element, "opacity")) {
            style.opacity = std::stof(GetAttribute(element, "opacity"));
        }

        // 解析fill-opacity属性
        if (HasAttribute(element, "fill-opacity")) {
            style.fillOpacity = std::stof(GetAttribute(element, "fill-opacity"));
        }

        // 解析stroke-opacity属性
        if (HasAttribute(element, "stroke-opacity")) {
            style.strokeOpacity = std::stof(GetAttribute(element, "stroke-opacity"));
        }

        // 解析fill-rule属性
        if (HasAttribute(element, "fill-rule")) {
            style.fillRule = GetAttribute(element, "fill-rule");
        }

        // 解析stroke-linecap属性 (butt, round, square)
        if (HasAttribute(element, "stroke-linecap")) {
            style.strokeLineCap = GetAttribute(element, "stroke-linecap");
        }

        // 解析stroke-linejoin属性 (miter, round, bevel)
        if (HasAttribute(element, "stroke-linejoin")) {
            style.strokeLineJoin = GetAttribute(element, "stroke-linejoin");
        }

        // 解析stroke-miterlimit属性
        if (HasAttribute(element, "stroke-miterlimit")) {
            style.strokeMiterLimit = std::stof(GetAttribute(element, "stroke-miterlimit"));
        }

        // 解析stroke-dasharray属性
        if (HasAttribute(element, "stroke-dasharray")) {
            std::string dashStr = GetAttribute(element, "stroke-dasharray");
            if (dashStr != "none") {
                std::vector<float> dashValues;
                std::istringstream dashStream(dashStr);
                std::string dashToken;
                while (std::getline(dashStream, dashToken, ',')) {
                    std::istringstream tokenStream(dashToken);
                    float v;
                    while (tokenStream >> v) {
                        dashValues.push_back(v);
                    }
                }
                if (!dashValues.empty()) {
                    style.strokeDashArray = dashValues;
                }
            }
        }

        // 解析stroke-dashoffset属性
        if (HasAttribute(element, "stroke-dashoffset")) {
            style.strokeDashOffset = ParseLength(GetAttribute(element, "stroke-dashoffset"), 0.0f);
        }

        return style;
    }

    Transform2D SVGParser::ParseTransform(const std::string& transformStr) {
        if (transformStr.empty()) return Transform2D();

        Transform2D result;

        // 使用正则表达式解析变换字符串
        std::regex transformRegex(R"((translate|scale|rotate|matrix)\s*\(([^)]+)\))");
        std::sregex_iterator iter(transformStr.begin(), transformStr.end(), transformRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            std::string type = (*iter)[1].str();
            std::string params = (*iter)[2].str();

            std::istringstream iss(params);
            std::vector<float> values;
            std::string token;

            while (std::getline(iss, token, ',')) {
                // 移除空白字符
                token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                if (!token.empty()) {
                    values.push_back(std::stof(token));
                }
            }

            if (type == "translate" && values.size() >= 1) {
                float tx = values[0];
                float ty = values.size() > 1 ? values[1] : 0.0f;
                result = Transform2D::Translate(tx, ty) * result;
            } else if (type == "scale" && values.size() >= 1) {
                float sx = values[0];
                float sy = values.size() > 1 ? values[1] : sx;
                result = Transform2D::Scale(sx, sy) * result;
            } else if (type == "rotate" && values.size() >= 1) {
                float angle = glm::radians(values[0]);
                result = Transform2D::Rotate(angle) * result;
            } else if (type == "matrix" && values.size() >= 6) {
                glm::mat3 m(values[0], values[2], values[4],
                        values[1], values[3], values[5],
                        0.0f,     0.0f,     1.0f);
                result = Transform2D(m) * result;
            }
        }

        return result;
    }

    glm::vec4 SVGParser::ParseColor(const std::string& colorStr) {
        if (colorStr.empty()) return glm::vec4(0, 0, 0, 1);

        // 处理currentColor关键字（使用默认黑色）
        if (colorStr == "currentColor") {
            return glm::vec4(0, 0, 0, 1);
        }

        // 处理十六进制颜色
        if (colorStr[0] == '#') {
            std::string hex = colorStr.substr(1);
            if (hex.length() == 3) {
                // 缩写形式 #RGB
                int r = std::stoi(hex.substr(0, 1) + hex.substr(0, 1), nullptr, 16);
                int g = std::stoi(hex.substr(1, 1) + hex.substr(1, 1), nullptr, 16);
                int b = std::stoi(hex.substr(2, 1) + hex.substr(2, 1), nullptr, 16);
                return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            } else if (hex.length() == 6) {
                // 完整形式 #RRGGBB
                int r = std::stoi(hex.substr(0, 2), nullptr, 16);
                int g = std::stoi(hex.substr(2, 2), nullptr, 16);
                int b = std::stoi(hex.substr(4, 2), nullptr, 16);
                return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            }
        }

        // 处理rgb()函数
        if (colorStr.find("rgb(") == 0) {
            std::string params = colorStr.substr(4, colorStr.find(')') - 4);
            std::istringstream iss(params);
            std::vector<float> values;
            std::string token;

            while (std::getline(iss, token, ',')) {
                token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                if (!token.empty()) {
                    if (token.back() == '%') {
                        values.push_back(std::stof(token.substr(0, token.size() - 1)) / 100.0f);
                    } else {
                        values.push_back(std::stof(token) / 255.0f);
                    }
                }
            }

            if (values.size() >= 3) {
                return glm::vec4(values[0], values[1], values[2], 1.0f);
            }
        }

        // 预定义颜色名称映射
        static std::unordered_map<std::string, glm::vec4> colorMap = {
            {"black",   glm::vec4(0, 0, 0, 1)},
            {"white",   glm::vec4(1, 1, 1, 1)},
            {"red",     glm::vec4(1, 0, 0, 1)},
            {"green",   glm::vec4(0, 1, 0, 1)},
            {"blue",    glm::vec4(0, 0, 1, 1)},
            {"yellow",  glm::vec4(1, 1, 0, 1)},
            {"cyan",    glm::vec4(0, 1, 1, 1)},
            {"magenta", glm::vec4(1, 0, 1, 1)},
            {"gray",    glm::vec4(0.5f, 0.5f, 0.5f, 1)},
            {"orange",  glm::vec4(1, 0.647f, 0, 1)},
            {"purple",  glm::vec4(0.5f, 0, 0.5f, 1)}
        };

        auto it = colorMap.find(colorStr);
        if (it != colorMap.end()) {
            return it->second;
        }

        // 默认返回黑色
        return glm::vec4(0, 0, 0, 1);
    }

    float SVGParser::ParseLength(const std::string& lengthStr, float defaultValue) {
        if (lengthStr.empty()) return defaultValue;

        try {
            std::string numStr;
            std::string unit;

            // 分离数字和单位
            size_t i = 0;
            while (i < lengthStr.size() && (std::isdigit(lengthStr[i]) || lengthStr[i] == '.' || lengthStr[i] == '-')) {
                numStr += lengthStr[i];
                i++;
            }
            while (i < lengthStr.size()) {
                unit += lengthStr[i];
                i++;
            }

            float value = std::stof(numStr);

            // 处理单位 (这里简化处理，大部分单位按像素处理)
            if (unit == "px" || unit.empty()) {
                return value;
            } else if (unit == "pt") {
                return value * 1.333f; // 1pt = 1.333px (approx)
            } else if (unit == "pc") {
                return value * 16.0f;  // 1pc = 16px
            } else if (unit == "in") {
                return value * 96.0f;  // 1in = 96px (assuming 96dpi)
            } else if (unit == "cm") {
                return value * 37.795f; // 1cm = 37.795px
            } else if (unit == "mm") {
                return value * 3.7795f; // 1mm = 3.7795px
            } else if (unit == "em" || unit == "ex" || unit == "%") {
                // 相对单位，暂时按像素处理
                return value;
            }

            return value;
        } catch (const std::exception&) {
            return defaultValue;
        }
    }

    bool SVGParser::ParsePathData(const std::string& pathData, std::vector<PathCommand>& commands) {
        commands.clear();

        std::string data = pathData;
        // 不要移除空白字符，因为它们是分隔符
        // data.erase(std::remove_if(data.begin(), data.end(), ::isspace), data.end());

        size_t i = 0;
        Point2D currentPos(0, 0);
        Point2D startPos(0, 0);
        Point2D lastControlPoint(0, 0);  // 用于S/T命令的控制点反射
        char lastCommand = '\0';
        char lastUpperCommand = '\0';    // 用于跟踪上一个命令类型（用于S/T的控制点计算）

        while (i < data.size()) {
            // 跳过空白字符
            while (i < data.size() && std::isspace(data[i])) i++;
            if (i >= data.size()) break;

            // 跳过逗号
            if (data[i] == ',') {
                i++;
                continue;
            }

            char cmd = data[i];
            bool relative = std::islower(cmd);
            char upperCmd = std::toupper(cmd);

            // 如果是连续的相同命令，可以省略命令字符
            if ((cmd >= '0' && cmd <= '9') || cmd == '-' || cmd == '+' || cmd == '.') {
                if (lastCommand != '\0') {
                    upperCmd = std::toupper(lastCommand);
                    relative = std::islower(lastCommand);
                } else {
                    continue; // 无效数据
                }
            } else {
                i++; // 消耗命令字符
                lastCommand = cmd;
            }

            // 在确定 relative 值后再创建 command
            PathCommand command(PathCommandType::MoveTo, relative);

            // 根据命令类型解析参数
            switch (upperCmd) {
                case 'M': // MoveTo
                    command.type = PathCommandType::MoveTo;
                    command.points = ParsePoints(data, i, 1);
                    if (!command.points.empty()) {
                        if (!relative) {
                            currentPos = command.points[0];
                            startPos = currentPos;
                        } else {
                            currentPos = currentPos + command.points[0];
                            startPos = currentPos;
                        }
                        lastControlPoint = currentPos;  // 重置控制点
                        
                        // MoveTo 后面的隐式命令是 LineTo
                        lastCommand = relative ? 'l' : 'L';
                    }
                    break;

                case 'L': // LineTo
                    command.type = PathCommandType::LineTo;
                    command.points = ParsePoints(data, i, 1);
                    if (!command.points.empty()) {
                        if (!relative) {
                            currentPos = command.points[0];
                        } else {
                            currentPos = currentPos + command.points[0];
                        }
                        lastControlPoint = currentPos;  // 重置控制点
                    }
                    break;

                case 'C': // Cubic Bezier Curve
                    command.type = PathCommandType::CurveTo;
                    command.points = ParsePoints(data, i, 3);
                    if (command.points.size() >= 3) {
                        // 保存第二控制点用于S命令
                        Point2D cp2 = relative ? currentPos + command.points[1] : command.points[1];
                        lastControlPoint = cp2;
                        if (!relative) {
                            currentPos = command.points[2];
                        } else {
                            currentPos = currentPos + command.points[2];
                        }
                    }
                    break;

                case 'Q': // Quadratic Bezier Curve
                    command.type = PathCommandType::QuadCurveTo;
                    command.points = ParsePoints(data, i, 2);
                    if (command.points.size() >= 2) {
                        Point2D controlPoint = relative ? currentPos + command.points[0] : command.points[0];
                        Point2D endPoint = relative ? currentPos + command.points[1] : command.points[1];
                        lastControlPoint = controlPoint;  // 保存控制点用于T命令
                        currentPos = endPoint;
                    }
                    break;

                case 'H': // Horizontal Line To
                {
                    command.type = PathCommandType::LineTo;
                    // H/h 只有一个x坐标参数
                    float x = ParseNumber(data, i);
                    if (relative) {
                        command.points.push_back(Point2D(x, 0));  // 相对坐标，y增量为0
                        currentPos.x += x;
                    } else {
                        command.points.push_back(Point2D(x, currentPos.y));  // 绝对坐标，y保持不变
                        command.relative = false;
                        currentPos.x = x;
                    }
                    lastControlPoint = currentPos;  // 重置控制点
                    break;
                }

                case 'V': // Vertical Line To
                {
                    command.type = PathCommandType::LineTo;
                    // V/v 只有一个y坐标参数
                    float y = ParseNumber(data, i);
                    if (relative) {
                        command.points.push_back(Point2D(0, y));  // 相对坐标，x增量为0
                        currentPos.y += y;
                    } else {
                        command.points.push_back(Point2D(currentPos.x, y));  // 绝对坐标，x保持不变
                        command.relative = false;
                        currentPos.y = y;
                    }
                    lastControlPoint = currentPos;  // 重置控制点
                    break;
                }

                case 'S': // Smooth Cubic Bezier Curve (Shorthand)
                {
                    command.type = PathCommandType::CurveTo;
                    // S/s 有两个点参数：第二控制点和终点
                    // 第一控制点是上一个C/S命令的第二控制点的反射
                    std::vector<Point2D> parsedPoints = ParsePoints(data, i, 2);
                    if (parsedPoints.size() >= 2) {
                        Point2D cp2 = relative ? currentPos + parsedPoints[0] : parsedPoints[0];
                        Point2D endPoint = relative ? currentPos + parsedPoints[1] : parsedPoints[1];
                        
                        // 计算第一控制点（上一个控制点的反射）
                        Point2D cp1;
                        if (lastUpperCommand == 'C' || lastUpperCommand == 'S') {
                            // 反射上一个控制点
                            cp1 = Point2D(2 * currentPos.x - lastControlPoint.x, 
                                         2 * currentPos.y - lastControlPoint.y);
                        } else {
                            // 如果上一个命令不是C或S，第一控制点等于当前点
                            cp1 = currentPos;
                        }
                        
                        // 转换为相对坐标存储（如果原命令是相对的）
                        if (relative) {
                            command.points.push_back(cp1 - currentPos);
                            command.points.push_back(parsedPoints[0]);
                            command.points.push_back(parsedPoints[1]);
                        } else {
                            command.points.push_back(cp1);
                            command.points.push_back(cp2);
                            command.points.push_back(endPoint);
                            command.relative = false;
                        }
                        
                        lastControlPoint = cp2;
                        currentPos = endPoint;
                    }
                    break;
                }

                case 'T': // Smooth Quadratic Bezier Curve (Shorthand)
                {
                    command.type = PathCommandType::QuadCurveTo;
                    // T/t 只有一个终点参数
                    // 控制点是上一个Q/T命令的控制点的反射
                    std::vector<Point2D> parsedPoints = ParsePoints(data, i, 1);
                    if (parsedPoints.size() >= 1) {
                        Point2D endPoint = relative ? currentPos + parsedPoints[0] : parsedPoints[0];
                        
                        // 计算控制点（上一个控制点的反射）
                        Point2D cp;
                        if (lastUpperCommand == 'Q' || lastUpperCommand == 'T') {
                            // 反射上一个控制点
                            cp = Point2D(2 * currentPos.x - lastControlPoint.x,
                                        2 * currentPos.y - lastControlPoint.y);
                        } else {
                            // 如果上一个命令不是Q或T，控制点等于当前点
                            cp = currentPos;
                        }
                        
                        // 存储控制点和终点
                        if (relative) {
                            command.points.push_back(cp - currentPos);
                            command.points.push_back(parsedPoints[0]);
                        } else {
                            command.points.push_back(cp);
                            command.points.push_back(endPoint);
                            command.relative = false;
                        }
                        
                        lastControlPoint = cp;
                        currentPos = endPoint;
                    }
                    break;
                }

                case 'A': // Arc
                    command.type = PathCommandType::ArcTo;
                    // A/a rx ry x-axis-rotation large-arc-flag sweep-flag x y
                    {
                        float rx = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float ry = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float rot = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float largeArc = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float sweep = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float x = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;
                        float y = ParseNumber(data, i);
                        while (i < data.size() && std::isspace(data[i])) i++; if (i < data.size() && data[i] == ',') i++;

                        command.points.push_back(Point2D(rx, ry));
                        command.points.push_back(Point2D(x, y));

                        if (!relative) {
                            currentPos = command.points[1];
                        } else {
                            currentPos = currentPos + command.points[1];
                        }
                    }
                    break;

                case 'Z': // ClosePath
                    command.type = PathCommandType::ClosePath;
                    command.points.clear();
                    currentPos = startPos;
                    lastControlPoint = currentPos;  // 重置控制点
                    break;

                default:
                    // 未知命令，跳过
                    i++;
                    while (i < data.size() && !std::isalpha(data[i])) i++;
                    continue;
            }

            // 记录上一个命令类型（用于S/T命令的控制点计算）
            lastUpperCommand = upperCmd;
            commands.push_back(command);
        }

        return !commands.empty();
    }

    std::vector<Point2D> SVGParser::ParsePoints(const std::string& data, size_t& i, int count) {
        std::vector<Point2D> points;

        for (int p = 0; p < count; p++) {
            // 解析x坐标
            float x = ParseNumber(data, i);
            
            // 跳过分隔符（空白和逗号）
            while (i < data.size() && std::isspace(data[i])) i++;
            if (i < data.size() && data[i] == ',') i++;

            // 解析y坐标
            float y = ParseNumber(data, i);

            points.emplace_back(x, y);

            // 跳过分隔符（空白和逗号），为下一个点做准备
            while (i < data.size() && std::isspace(data[i])) i++;
            if (i < data.size() && data[i] == ',') i++;
        }

        return points;
    }

    float SVGParser::ParseNumber(const std::string& data, size_t& i) {
        std::string numStr;

        // 跳过空白字符
        while (i < data.size() && std::isspace(data[i])) i++;

        // 处理符号
        if (i < data.size() && (data[i] == '-' || data[i] == '+')) {
            numStr += data[i];
            i++;
        }

        // 处理数字部分
        bool hasDot = false;
        while (i < data.size()) {
            char c = data[i];
            if (std::isdigit(c)) {
                numStr += c;
                i++;
            } else if (c == '.' && !hasDot) {
                numStr += c;
                hasDot = true;
                i++;
            } else if (c == 'e' || c == 'E') {
                numStr += c;
                i++;
                // 处理指数符号
                if (i < data.size() && (data[i] == '-' || data[i] == '+')) {
                    numStr += data[i];
                    i++;
                }
                // 处理指数数字
                while (i < data.size() && std::isdigit(data[i])) {
                    numStr += data[i];
                    i++;
                }
                break;
            } else {
                break;
            }
        }

        try {
            return std::stof(numStr);
        } catch (const std::exception&) {
            return 0.0f;
        }
    }

    std::string SVGParser::GetAttribute(tinyxml2::XMLElement* element, const char* name, const std::string& defaultValue) {
        const char* value = element->Attribute(name);
        return value ? std::string(value) : defaultValue;
    }

    bool SVGParser::HasAttribute(tinyxml2::XMLElement* element, const char* name) {
        return element->Attribute(name) != nullptr;
    }
}