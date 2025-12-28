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

        tinyxml2::XMLElement* svgElement = _xmlDoc->FirstChildElement("svg");
        if (!svgElement) {
            std::cerr << "No SVG element found in file" << std::endl;
            return false;
        }

        return ParseSVGElement(svgElement, document);
    }

    bool SVGParser::ParseString(const std::string& svgContent, SVGDocument& document) {
        tinyxml2::XMLError error = _xmlDoc->Parse(svgContent.c_str());
        if (error != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to parse SVG content" << std::endl;
            return false;
        }

        tinyxml2::XMLElement* svgElement = _xmlDoc->FirstChildElement("svg");
        if (!svgElement) {
            std::cerr << "No SVG element found in content" << std::endl;
            return false;
        }

        return ParseSVGElement(svgElement, document);
    }

    bool SVGParser::ParseSVGElement(tinyxml2::XMLElement* svgElement, SVGDocument& document) {
        // 解析基本属性
        document.width = ParseLength(GetAttribute(svgElement, "width", "800"), 800.0f);
        document.height = ParseLength(GetAttribute(svgElement, "height", "600"), 600.0f);
        document.viewBox = GetAttribute(svgElement, "viewBox");

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
                element.type = SVGElement::Type::Group;
                parsed = ParseGroupElement(child, element);
            }

            if (parsed) {
                document.elements.push_back(std::move(element));
            }
        }

        return true;
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
                    }
                }
            }
        }

        // 解析fill属性（属性优先级高于style中的设置）
        if (HasAttribute(element, "fill")) {
            std::string fill = GetAttribute(element, "fill");
            if (fill != "none") {
                style.fillColor = ParseColor(fill);
            }
        }

        // 解析stroke属性
        if (HasAttribute(element, "stroke")) {
            std::string stroke = GetAttribute(element, "stroke");
            if (stroke != "none") {
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
        char lastCommand = '\0';

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

            PathCommand command(PathCommandType::MoveTo, relative);

            // 如果是连续的相同命令，可以省略命令字符
            if ((cmd >= '0' && cmd <= '9') || cmd == '-' || cmd == '+' || cmd == '.') {
                if (lastCommand != '\0') {
                    upperCmd = lastCommand;
                    relative = std::islower(lastCommand);
                } else {
                    continue; // 无效数据
                }
            } else {
                i++; // 消耗命令字符
                lastCommand = cmd;
            }

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
                    }
                    break;

                case 'C': // Cubic Bezier Curve
                    command.type = PathCommandType::CurveTo;
                    command.points = ParsePoints(data, i, 3);
                    if (command.points.size() >= 3) {
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
                        if (!relative) {
                            currentPos = command.points[1];
                        } else {
                            currentPos = currentPos + command.points[1];
                        }
                    }
                    break;

                case 'A': // Arc
                    command.type = PathCommandType::ArcTo;
                    command.points = ParsePoints(data, i, 2); // 只取端点，忽略弧参数
                    if (command.points.size() >= 2) {
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
                    break;

                default:
                    // 未知命令，跳过
                    while (i < data.size() && !std::isalpha(data[i])) i++;
                    continue;
            }

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