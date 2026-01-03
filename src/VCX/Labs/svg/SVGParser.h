#pragma once

#include "SVG.h"
#include <string>
#include <memory>

namespace tinyxml2 {
    class XMLDocument;
    class XMLElement;
}

namespace VCX::Labs::SVG {

class SVGParser {
public:
    SVGParser();
    ~SVGParser();

    // 解析SVG文件
    bool ParseFile(const std::string& filename, SVGDocument& document);

    // 解析SVG字符串
    bool ParseString(const std::string& svgContent, SVGDocument& document);

private:
    std::unique_ptr<tinyxml2::XMLDocument> _xmlDoc;

    // 解析SVG根元素
    bool ParseSVGElement(tinyxml2::XMLElement* svgElement, SVGDocument& document);

    // 解析各种SVG元素
    bool ParsePathElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseCircleElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseEllipseElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseRectElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseLineElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseTextElement(tinyxml2::XMLElement* element, SVGElement& svgElement);
    bool ParseGroupElement(tinyxml2::XMLElement* element, SVGElement& svgElement);

    // 递归解析<g>标签并将所有子元素展平添加到document.elements
    void ParseGroupElementFlattened(tinyxml2::XMLElement* element, SVGDocument& document, const Transform2D& parentTransform, const SVGStyle& parentStyle);

    // 解析样式和属性
    SVGStyle ParseStyle(tinyxml2::XMLElement* element);
    Transform2D ParseTransform(const std::string& transformStr);
    glm::vec4 ParseColor(const std::string& colorStr);
    float ParseLength(const std::string& lengthStr, float defaultValue = 0.0f);

    // 解析路径数据 (d属性)
    bool ParsePathData(const std::string& pathData, std::vector<PathCommand>& commands);

    // 工具函数
    std::string GetAttribute(tinyxml2::XMLElement* element, const char* name, const std::string& defaultValue = "");
    bool HasAttribute(tinyxml2::XMLElement* element, const char* name);
    std::vector<Point2D> ParsePoints(const std::string& data, size_t& i, int count);
    float ParseNumber(const std::string& data, size_t& i);
};

} // namespace VCX::Labs::SVG
