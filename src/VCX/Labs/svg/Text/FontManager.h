#pragma once

#include "Core/Math2D.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <glm/glm.hpp>

namespace VCX::Labs::SVG {

//=============================================================================
// Text Metrics
//=============================================================================
struct TextMetrics {
    float width;            // Total width of text
    float height;           // Total height (ascent + descent)
    float ascent;           // Distance from baseline to top
    float descent;          // Distance from baseline to bottom (positive down)
    float lineHeight;       // Recommended line spacing
};

//=============================================================================
// Glyph Metrics
//=============================================================================
struct GlyphMetrics {
    float width;            // Glyph width
    float height;           // Glyph height
    float bearingX;         // Left side bearing
    float bearingY;         // Top side bearing
    float advance;          // Horizontal advance to next glyph
};

//=============================================================================
// Glyph Data - Contains outline or bitmap
//=============================================================================
struct GlyphData {
    char32_t codepoint;
    GlyphMetrics metrics;
    
    // Outline as path commands (for vector rendering)
    std::vector<Vec2> outline;
    std::vector<std::vector<Vec2>> contours;
    
    // Bitmap (for raster rendering)
    std::vector<uint8_t> bitmap;
    int bitmapWidth = 0;
    int bitmapHeight = 0;
    
    // SDF texture (for GPU rendering)
    std::vector<float> sdf;
    int sdfWidth = 0;
    int sdfHeight = 0;
    float sdfPadding = 0;
};

//=============================================================================
// Font Face - Represents a loaded font
//=============================================================================
class FontFace {
public:
    virtual ~FontFace() = default;
    
    virtual bool IsLoaded() const = 0;
    virtual std::string GetFamilyName() const = 0;
    virtual std::string GetStyleName() const = 0;
    
    // Get glyph data at specified size
    virtual GlyphData GetGlyph(char32_t codepoint, float fontSize) = 0;
    
    // Get text metrics
    virtual TextMetrics MeasureText(const std::u32string& text, float fontSize) = 0;
    
    // Get kerning between two glyphs
    virtual float GetKerning(char32_t left, char32_t right, float fontSize) = 0;
    
    // Check if glyph exists
    virtual bool HasGlyph(char32_t codepoint) = 0;
};

//=============================================================================
// Font Manager - Manages loaded fonts
//=============================================================================
class FontManager {
public:
    static FontManager& Instance() {
        static FontManager instance;
        return instance;
    }
    
    // Load a font from file
    bool LoadFont(const std::string& path, const std::string& name = "");
    
    // Load a font from memory
    bool LoadFontFromMemory(const std::vector<uint8_t>& data, const std::string& name);
    
    // Get a loaded font
    std::shared_ptr<FontFace> GetFont(const std::string& name);
    
    // Get default font
    std::shared_ptr<FontFace> GetDefaultFont();
    
    // Set default font
    void SetDefaultFont(const std::string& name);
    
    // List loaded fonts
    std::vector<std::string> GetLoadedFonts() const;
    
    // Unload a font
    void UnloadFont(const std::string& name);
    
    // Clear all fonts
    void ClearAll();

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    std::map<std::string, std::shared_ptr<FontFace>> _fonts;
    std::string _defaultFontName;
};

//=============================================================================
// Simple Bitmap Font (fallback when FreeType not available)
//=============================================================================
class BitmapFontFace : public FontFace {
public:
    BitmapFontFace();
    
    bool IsLoaded() const override { return true; }
    std::string GetFamilyName() const override { return "Bitmap"; }
    std::string GetStyleName() const override { return "Regular"; }
    
    GlyphData GetGlyph(char32_t codepoint, float fontSize) override;
    TextMetrics MeasureText(const std::u32string& text, float fontSize) override;
    float GetKerning(char32_t left, char32_t right, float fontSize) override;
    bool HasGlyph(char32_t codepoint) override;

private:
    // 5x7 bitmap font data
    static const int CHAR_WIDTH = 5;
    static const int CHAR_HEIGHT = 7;
    static const int CHAR_SPACING = 1;
    
    std::map<char32_t, std::array<uint8_t, 5>> _glyphData;
    
    void InitializeGlyphData();
};

//=============================================================================
// Text Renderer - Renders text to image
//=============================================================================
class TextRenderer {
public:
    enum class RenderMode {
        Bitmap,     // Use bitmap font
        Outline,    // Render glyph outlines as paths
        SDF         // Use signed distance field (GPU)
    };
    
    TextRenderer();
    
    void SetFont(std::shared_ptr<FontFace> font) { _font = font; }
    void SetRenderMode(RenderMode mode) { _mode = mode; }
    void SetFontSize(float size) { _fontSize = size; }
    void SetColor(const glm::vec4& color) { _color = color; }
    
    // Get glyph outline as polygon vertices
    std::vector<std::vector<Vec2>> GetTextOutlines(const std::string& text,
                                                     const Vec2& position);
    
    // Measure text
    TextMetrics MeasureText(const std::string& text);
    
    // Convert UTF-8 to UTF-32
    static std::u32string UTF8ToUTF32(const std::string& utf8);
    
private:
    std::shared_ptr<FontFace> _font;
    RenderMode _mode;
    float _fontSize;
    glm::vec4 _color;
};

//=============================================================================
// Implementation stubs
//=============================================================================

inline bool FontManager::LoadFont(const std::string& path, const std::string& name) {
    // TODO: Implement FreeType font loading
    // For now, create a bitmap font
    std::string fontName = name.empty() ? path : name;
    _fonts[fontName] = std::make_shared<BitmapFontFace>();
    
    if (_defaultFontName.empty()) {
        _defaultFontName = fontName;
    }
    
    return true;
}

inline bool FontManager::LoadFontFromMemory(const std::vector<uint8_t>& data, 
                                             const std::string& name) {
    // TODO: Implement FreeType font loading from memory
    _fonts[name] = std::make_shared<BitmapFontFace>();
    return true;
}

inline std::shared_ptr<FontFace> FontManager::GetFont(const std::string& name) {
    auto it = _fonts.find(name);
    if (it != _fonts.end()) {
        return it->second;
    }
    return nullptr;
}

inline std::shared_ptr<FontFace> FontManager::GetDefaultFont() {
    if (_defaultFontName.empty()) {
        // Create default bitmap font
        _fonts["default"] = std::make_shared<BitmapFontFace>();
        _defaultFontName = "default";
    }
    return GetFont(_defaultFontName);
}

inline void FontManager::SetDefaultFont(const std::string& name) {
    if (_fonts.find(name) != _fonts.end()) {
        _defaultFontName = name;
    }
}

inline std::vector<std::string> FontManager::GetLoadedFonts() const {
    std::vector<std::string> names;
    for (const auto& pair : _fonts) {
        names.push_back(pair.first);
    }
    return names;
}

inline void FontManager::UnloadFont(const std::string& name) {
    _fonts.erase(name);
    if (_defaultFontName == name) {
        _defaultFontName.clear();
    }
}

inline void FontManager::ClearAll() {
    _fonts.clear();
    _defaultFontName.clear();
}

inline BitmapFontFace::BitmapFontFace() {
    InitializeGlyphData();
}

inline void BitmapFontFace::InitializeGlyphData() {
    // Initialize with basic ASCII characters (same as original renderer)
    // This is the 5x7 font data
    static const unsigned char font5x7[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // space
        {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
        {0x00, 0x07, 0x00, 0x07, 0x00}, // "
        {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
        {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
        {0x23, 0x13, 0x08, 0x64, 0x62}, // %
        {0x36, 0x49, 0x55, 0x22, 0x50}, // &
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '
        {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
        {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
        {0x14, 0x08, 0x3e, 0x08, 0x14}, // *
        {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
        {0x08, 0x08, 0x08, 0x08, 0x08}, // -
        {0x00, 0x60, 0x60, 0x00, 0x00}, // .
        {0x20, 0x10, 0x08, 0x04, 0x02}, // /
        {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
        {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
        {0x08, 0x14, 0x22, 0x41, 0x00}, // <
        {0x14, 0x14, 0x14, 0x14, 0x14}, // =
        {0x00, 0x41, 0x22, 0x14, 0x08}, // >
        {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
        {0x32, 0x49, 0x79, 0x41, 0x3e}, // @
        {0x7e, 0x11, 0x11, 0x11, 0x7e}, // A
        {0x7f, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3e, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7f, 0x41, 0x41, 0x22, 0x1c}, // D
        {0x7f, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7f, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3e, 0x41, 0x49, 0x49, 0x7a}, // G
        {0x7f, 0x08, 0x08, 0x08, 0x7f}, // H
        {0x00, 0x41, 0x7f, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3f, 0x01}, // J
        {0x7f, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7f, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7f, 0x02, 0x0c, 0x02, 0x7f}, // M
        {0x7f, 0x04, 0x08, 0x10, 0x7f}, // N
        {0x3e, 0x41, 0x41, 0x41, 0x3e}, // O
        {0x7f, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3e, 0x41, 0x51, 0x21, 0x5e}, // Q
        {0x7f, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7f, 0x01, 0x01}, // T
        {0x3f, 0x40, 0x40, 0x40, 0x3f}, // U
        {0x1f, 0x20, 0x40, 0x20, 0x1f}, // V
        {0x3f, 0x40, 0x38, 0x40, 0x3f}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
        {0x00, 0x7f, 0x41, 0x41, 0x00}, // [
        {0x02, 0x04, 0x08, 0x10, 0x20}, // \
        {0x00, 0x41, 0x41, 0x7f, 0x00}, // ]
        {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
        {0x40, 0x40, 0x40, 0x40, 0x40}, // _
        {0x00, 0x01, 0x02, 0x04, 0x00}, // `
        {0x20, 0x54, 0x54, 0x54, 0x78}, // a
        {0x7f, 0x48, 0x44, 0x44, 0x38}, // b
        {0x38, 0x44, 0x44, 0x44, 0x20}, // c
        {0x38, 0x44, 0x44, 0x48, 0x7f}, // d
        {0x38, 0x54, 0x54, 0x54, 0x18}, // e
        {0x08, 0x7e, 0x09, 0x01, 0x02}, // f
        {0x0c, 0x52, 0x52, 0x52, 0x3e}, // g
        {0x7f, 0x08, 0x04, 0x04, 0x78}, // h
        {0x00, 0x44, 0x7d, 0x40, 0x00}, // i
        {0x20, 0x40, 0x44, 0x3d, 0x00}, // j
        {0x7f, 0x10, 0x28, 0x44, 0x00}, // k
        {0x00, 0x41, 0x7f, 0x40, 0x00}, // l
        {0x7c, 0x04, 0x18, 0x04, 0x78}, // m
        {0x7c, 0x08, 0x04, 0x04, 0x78}, // n
        {0x38, 0x44, 0x44, 0x44, 0x38}, // o
        {0x7c, 0x14, 0x14, 0x14, 0x08}, // p
        {0x08, 0x14, 0x14, 0x18, 0x7c}, // q
        {0x7c, 0x08, 0x04, 0x04, 0x08}, // r
        {0x48, 0x54, 0x54, 0x54, 0x20}, // s
        {0x04, 0x3f, 0x44, 0x40, 0x20}, // t
        {0x3c, 0x40, 0x40, 0x20, 0x7c}, // u
        {0x1c, 0x20, 0x40, 0x20, 0x1c}, // v
        {0x3c, 0x40, 0x30, 0x40, 0x3c}, // w
        {0x44, 0x28, 0x10, 0x28, 0x44}, // x
        {0x0c, 0x50, 0x50, 0x50, 0x3c}, // y
        {0x44, 0x64, 0x54, 0x4c, 0x44}, // z
        {0x00, 0x08, 0x36, 0x41, 0x00}, // {
        {0x00, 0x00, 0x7f, 0x00, 0x00}, // |
        {0x00, 0x41, 0x36, 0x08, 0x00}, // }
        {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
    };
    
    for (int i = 0; i < 95; ++i) {
        char32_t ch = static_cast<char32_t>(32 + i);
        std::array<uint8_t, 5> data;
        for (int j = 0; j < 5; ++j) {
            data[j] = font5x7[i][j];
        }
        _glyphData[ch] = data;
    }
}

inline GlyphData BitmapFontFace::GetGlyph(char32_t codepoint, float fontSize) {
    GlyphData glyph;
    glyph.codepoint = codepoint;
    
    float scale = fontSize / CHAR_HEIGHT;
    
    glyph.metrics.width = CHAR_WIDTH * scale;
    glyph.metrics.height = CHAR_HEIGHT * scale;
    glyph.metrics.bearingX = 0;
    glyph.metrics.bearingY = glyph.metrics.height;
    glyph.metrics.advance = (CHAR_WIDTH + CHAR_SPACING) * scale;
    
    // Generate bitmap
    auto it = _glyphData.find(codepoint);
    if (it == _glyphData.end()) {
        it = _glyphData.find(U'?');
    }
    
    if (it != _glyphData.end()) {
        glyph.bitmapWidth = CHAR_WIDTH;
        glyph.bitmapHeight = CHAR_HEIGHT;
        glyph.bitmap.resize(CHAR_WIDTH * CHAR_HEIGHT);
        
        for (int col = 0; col < CHAR_WIDTH; ++col) {
            uint8_t bits = it->second[col];
            for (int row = 0; row < CHAR_HEIGHT; ++row) {
                glyph.bitmap[row * CHAR_WIDTH + col] = (bits & (1 << row)) ? 255 : 0;
            }
        }
    }
    
    return glyph;
}

inline TextMetrics BitmapFontFace::MeasureText(const std::u32string& text, float fontSize) {
    TextMetrics metrics;
    float scale = fontSize / CHAR_HEIGHT;
    
    metrics.width = text.length() * (CHAR_WIDTH + CHAR_SPACING) * scale;
    metrics.height = CHAR_HEIGHT * scale;
    metrics.ascent = CHAR_HEIGHT * scale;
    metrics.descent = 0;
    metrics.lineHeight = (CHAR_HEIGHT + 2) * scale;
    
    return metrics;
}

inline float BitmapFontFace::GetKerning(char32_t left, char32_t right, float fontSize) {
    // Simple bitmap font has no kerning
    return 0;
}

inline bool BitmapFontFace::HasGlyph(char32_t codepoint) {
    return _glyphData.find(codepoint) != _glyphData.end();
}

inline TextRenderer::TextRenderer()
    : _mode(RenderMode::Bitmap)
    , _fontSize(12.0f)
    , _color(0, 0, 0, 1) {
}

inline std::vector<std::vector<Vec2>> TextRenderer::GetTextOutlines(const std::string& text,
                                                                      const Vec2& position) {
    std::vector<std::vector<Vec2>> outlines;
    
    if (!_font) {
        _font = FontManager::Instance().GetDefaultFont();
    }
    
    std::u32string text32 = UTF8ToUTF32(text);
    float x = position.x;
    float y = position.y;
    
    for (char32_t ch : text32) {
        GlyphData glyph = _font->GetGlyph(ch, _fontSize);
        
        // For bitmap font, convert bitmap to outline (simple box)
        if (glyph.bitmap.empty()) continue;
        
        std::vector<Vec2> charOutline;
        
        float scale = _fontSize / 7.0f;  // Assuming 7px height
        float charX = x;
        float charY = y - glyph.metrics.bearingY;
        
        // Create outline from bitmap
        for (int row = 0; row < glyph.bitmapHeight; ++row) {
            for (int col = 0; col < glyph.bitmapWidth; ++col) {
                if (glyph.bitmap[row * glyph.bitmapWidth + col] > 127) {
                    float px = charX + col * scale;
                    float py = charY + row * scale;
                    
                    std::vector<Vec2> pixelBox = {
                        Vec2(px, py),
                        Vec2(px + scale, py),
                        Vec2(px + scale, py + scale),
                        Vec2(px, py + scale)
                    };
                    outlines.push_back(pixelBox);
                }
            }
        }
        
        x += glyph.metrics.advance;
    }
    
    return outlines;
}

inline TextMetrics TextRenderer::MeasureText(const std::string& text) {
    if (!_font) {
        _font = FontManager::Instance().GetDefaultFont();
    }
    
    std::u32string text32 = UTF8ToUTF32(text);
    return _font->MeasureText(text32, _fontSize);
}

inline std::u32string TextRenderer::UTF8ToUTF32(const std::string& utf8) {
    std::u32string result;
    size_t i = 0;
    
    while (i < utf8.size()) {
        char32_t codepoint;
        unsigned char c = utf8[i];
        
        if ((c & 0x80) == 0) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = (c & 0x1F) << 6;
            if (i + 1 < utf8.size()) {
                codepoint |= (utf8[i + 1] & 0x3F);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = (c & 0x0F) << 12;
            if (i + 1 < utf8.size()) {
                codepoint |= (utf8[i + 1] & 0x3F) << 6;
            }
            if (i + 2 < utf8.size()) {
                codepoint |= (utf8[i + 2] & 0x3F);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = (c & 0x07) << 18;
            if (i + 1 < utf8.size()) {
                codepoint |= (utf8[i + 1] & 0x3F) << 12;
            }
            if (i + 2 < utf8.size()) {
                codepoint |= (utf8[i + 2] & 0x3F) << 6;
            }
            if (i + 3 < utf8.size()) {
                codepoint |= (utf8[i + 3] & 0x3F);
            }
            i += 4;
        } else {
            codepoint = '?';
            i += 1;
        }
        
        result += codepoint;
    }
    
    return result;
}

} // namespace VCX::Labs::SVG
