#include "SVG.h"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace VCX::Labs::SVG {

// SVGPath::GetVertices() 实现
std::vector<Point2D> SVGPath::GetVertices() const {
    std::vector<Point2D> vertices;
    Point2D currentPos(0, 0);
    Point2D startPos(0, 0);

    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case PathCommandType::MoveTo: {
                if (!cmd.points.empty()) {
                    Point2D target = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    currentPos = target;
                    startPos = target;
                    vertices.push_back(transform.TransformPoint(target));
                }
                break;
            }
            case PathCommandType::LineTo: {
                if (!cmd.points.empty()) {
                    Point2D target = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    currentPos = target;
                    vertices.push_back(transform.TransformPoint(target));
                }
                break;
            }
            case PathCommandType::ClosePath: {
                // 闭合路径，返回到起始点
                currentPos = startPos;
                vertices.push_back(transform.TransformPoint(startPos));
                break;
            }
            case PathCommandType::CurveTo: {
                // 三次贝塞尔曲线 - 使用细分算法生成平滑曲线
                if (cmd.points.size() >= 3) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    Point2D p3 = cmd.relative ? currentPos + cmd.points[2] : cmd.points[2];

                    // 使用20个采样点生成平滑曲线
                    const int segments = 20;
                    for (int i = 1; i <= segments; i++) {
                        float t = static_cast<float>(i) / segments;
                        float t2 = t * t;
                        float t3 = t2 * t;
                        float mt = 1.0f - t;
                        float mt2 = mt * mt;
                        float mt3 = mt2 * mt;

                        // 三次贝塞尔曲线公式: B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
                        Point2D point(
                            mt3 * p0.x + 3 * mt2 * t * p1.x + 3 * mt * t2 * p2.x + t3 * p3.x,
                            mt3 * p0.y + 3 * mt2 * t * p1.y + 3 * mt * t2 * p2.y + t3 * p3.y
                        );
                        vertices.push_back(transform.TransformPoint(point));
                    }
                    currentPos = p3;
                }
                break;
            }
            case PathCommandType::QuadCurveTo: {
                // 二次贝塞尔曲线 - 使用细分算法生成平滑曲线
                if (cmd.points.size() >= 2) {
                    Point2D p0 = currentPos;
                    Point2D p1 = cmd.relative ? currentPos + cmd.points[0] : cmd.points[0];
                    Point2D p2 = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];

                    // 使用15个采样点生成平滑曲线
                    const int segments = 15;
                    for (int i = 1; i <= segments; i++) {
                        float t = static_cast<float>(i) / segments;
                        float t2 = t * t;
                        float mt = 1.0f - t;
                        float mt2 = mt * mt;

                        // 二次贝塞尔曲线公式: B(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
                        Point2D point(
                            mt2 * p0.x + 2 * mt * t * p1.x + t2 * p2.x,
                            mt2 * p0.y + 2 * mt * t * p1.y + t2 * p2.y
                        );
                        vertices.push_back(transform.TransformPoint(point));
                    }
                    currentPos = p2;
                }
                break;
            }
            case PathCommandType::ArcTo: {
                // 弧线 - 简化为直线
                if (cmd.points.size() >= 2) {
                    Point2D target = cmd.relative ? currentPos + cmd.points[1] : cmd.points[1];
                    vertices.push_back(transform.TransformPoint(target));
                    currentPos = target;
                }
                break;
            }
        }
    }

    return vertices;
}

// SVGDocument::ParseViewBox() 实现
bool SVGDocument::ParseViewBox(float& x, float& y, float& w, float& h) const {
    if (viewBox.empty()) return false;

    std::istringstream iss(viewBox);
    std::string token;
    std::vector<float> values;

    while (iss >> token) {
        try {
            values.push_back(std::stof(token));
        } catch (const std::exception&) {
            return false;
        }
    }

    if (values.size() != 4) return false;

    x = values[0];
    y = values[1];
    w = values[2];
    h = values[3];
    return true;
}

} // namespace VCX::Labs::SVG
