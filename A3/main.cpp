#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ============================================================
// Vec2
// ============================================================

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& v) const
    {
        return { x + v.x, y + v.y };
    }

    Vec2 operator-(const Vec2& v) const
    {
        return { x - v.x, y - v.y };
    }

    Vec2 operator*(float s) const
    {
        return { x * s, y * s };
    }
};

float lengthSquared(const Vec2& v)
{
    return v.x * v.x + v.y * v.y;
}

// ============================================================
// Curve Type
// ============================================================

enum class CurveType
{
    Linear,
    Hermite,
    CatmullRom,
    BlendedParabola,
    Bezier
};

const char* getCurveName(CurveType type)
{
    switch (type)
    {
    case CurveType::Linear:
        return "Linear Interpolation";

    case CurveType::Hermite:
        return "Hermite";

    case CurveType::CatmullRom:
        return "Catmull-Rom";

    case CurveType::BlendedParabola:
        return "Blended Parabola";

    case CurveType::Bezier:
        return "Bezier";
    }

    return "Unknown";
}

// ============================================================
// Curve
// ============================================================

struct Curve
{
    CurveType type;

    std::vector<Vec2> points;

    float color[3] =
    {
        1.0f,
        1.0f,
        1.0f
    };

    bool visible = true;

    // Hermite
    float tangentScale = 0.5f;

    // Catmull-Rom
    float tension = 0.0f;

    // Blended Parabola
    float blend = 0.5f;

    // Sampling resolution
    int segments = 160;
};

// ============================================================
// Global
// ============================================================

GLFWwindow* gWindow = nullptr;

int gWindowWidth = 1400;
int gWindowHeight = 800;

GLuint gShaderProgram = 0;

std::vector<Curve> gCurves;

int gSelectedCurve = 0;
int gSelectedPoint = -1;

bool gDraggingPoint = false;

bool gShowGrid = true;
bool gShowControlPolygon = true;
bool gShowControlPoints = true;

// ============================================================
// Shader
// ============================================================

GLuint compileShader(
    GLenum type,
    const char* source)
{
    GLuint shader =
        glCreateShader(type);

    glShaderSource(
        shader,
        1,
        &source,
        nullptr
    );

    glCompileShader(shader);

    GLint success;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        char info[1024];

        glGetShaderInfoLog(
            shader,
            1024,
            nullptr,
            info
        );

        printf(
            "Shader error:\n%s\n",
            info
        );
    }

    return shader;
}

void createShader()
{
    const char* vertexShader =
        R"(
        #version 330 core

        layout(location = 0)
        in vec2 aPos;

        uniform vec2 uScreenSize;

        void main()
        {
            vec2 p =
                aPos / uScreenSize * 2.0 - 1.0;

            p.y = -p.y;

            gl_Position =
                vec4(p, 0.0, 1.0);
        }
        )";

    const char* fragmentShader =
        R"(
        #version 330 core

        uniform vec3 uColor;

        out vec4 FragColor;

        void main()
        {
            FragColor =
                vec4(uColor, 1.0);
        }
        )";

    GLuint vs =
        compileShader(
            GL_VERTEX_SHADER,
            vertexShader
        );

    GLuint fs =
        compileShader(
            GL_FRAGMENT_SHADER,
            fragmentShader
        );

    gShaderProgram =
        glCreateProgram();

    glAttachShader(
        gShaderProgram,
        vs
    );

    glAttachShader(
        gShaderProgram,
        fs
    );

    glLinkProgram(
        gShaderProgram
    );

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ============================================================
// OpenGL Drawing
// ============================================================

void drawPolyline(
    const std::vector<Vec2>& points,
    float r,
    float g,
    float b,
    float width = 1.0f,
    float offsetX = 0.0f,
    float offsetY = 0.0f)
{
    if (points.size() < 2)
        return;

    std::vector<float> vertices;

    vertices.reserve(points.size() * 2);

    for (const auto& p : points)
    {
        vertices.push_back(
            p.x + offsetX
        );

        vertices.push_back(
            p.y + offsetY
        );
    }

    GLuint vao;
    GLuint vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        vbo
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 2,
        nullptr
    );

    glEnableVertexAttribArray(0);

    glUseProgram(gShaderProgram);

    glUniform2f(
        glGetUniformLocation(
            gShaderProgram,
            "uScreenSize"
        ),
        (float)gWindowWidth,
        (float)gWindowHeight
    );

    glUniform3f(
        glGetUniformLocation(
            gShaderProgram,
            "uColor"
        ),
        r,
        g,
        b
    );

    glLineWidth(width);

    glDrawArrays(
        GL_LINE_STRIP,
        0,
        (GLsizei)points.size()
    );

    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void drawPoints(
    const std::vector<Vec2>& points,
    float r,
    float g,
    float b,
    float size,
    float offsetX = 0.0f,
    float offsetY = 0.0f)
{
    if (points.empty())
        return;

    std::vector<float> vertices;

    vertices.reserve(points.size() * 2);

    for (const auto& p : points)
    {
        vertices.push_back(
            p.x + offsetX
        );

        vertices.push_back(
            p.y + offsetY
        );
    }

    GLuint vao;
    GLuint vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        vbo
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 2,
        nullptr
    );

    glEnableVertexAttribArray(0);

    glUseProgram(gShaderProgram);

    glUniform2f(
        glGetUniformLocation(
            gShaderProgram,
            "uScreenSize"
        ),
        (float)gWindowWidth,
        (float)gWindowHeight
    );

    glUniform3f(
        glGetUniformLocation(
            gShaderProgram,
            "uColor"
        ),
        r,
        g,
        b
    );

    glPointSize(size);

    glDrawArrays(
        GL_POINTS,
        0,
        (GLsizei)points.size()
    );

    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

// ============================================================
// Curve Mathematics
// ============================================================

Vec2 linear(
    const Vec2& p0,
    const Vec2& p1,
    float t)
{
    return
        p0 * (1.0f - t) +
        p1 * t;
}

Vec2 quadraticBezier(
    const Vec2& p0,
    const Vec2& p1,
    const Vec2& p2,
    float t)
{
    float u = 1.0f - t;

    return
        p0 * (u * u) +
        p1 * (2.0f * u * t) +
        p2 * (t * t);
}

Vec2 cubicBezier(
    const Vec2& p0,
    const Vec2& p1,
    const Vec2& p2,
    const Vec2& p3,
    float t)
{
    float u = 1.0f - t;

    return
        p0 * (u * u * u) +
        p1 * (3.0f * u * u * t) +
        p2 * (3.0f * u * t * t) +
        p3 * (t * t * t);
}

Vec2 hermite(
    const Vec2& p0,
    const Vec2& p1,
    const Vec2& t0,
    const Vec2& t1,
    float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    float h00 =
        2.0f * t3 -
        3.0f * t2 +
        1.0f;

    float h10 =
        t3 -
        2.0f * t2 +
        t;

    float h01 =
        -2.0f * t3 +
        3.0f * t2;

    float h11 =
        t3 -
        t2;

    return
        p0 * h00 +
        t0 * h10 +
        p1 * h01 +
        t1 * h11;
}

Vec2 catmullRom(
    const Vec2& p0,
    const Vec2& p1,
    const Vec2& p2,
    const Vec2& p3,
    float tension,
    float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    float s =
        (1.0f - tension) * 0.5f;

    Vec2 m1 =
        (p2 - p0) * s;

    Vec2 m2 =
        (p3 - p1) * s;

    float h00 =
        2.0f * t3 -
        3.0f * t2 +
        1.0f;

    float h10 =
        t3 -
        2.0f * t2 +
        t;

    float h01 =
        -2.0f * t3 +
        3.0f * t2;

    float h11 =
        t3 -
        t2;

    return
        p1 * h00 +
        m1 * h10 +
        p2 * h01 +
        m2 * h11;
}

// ============================================================
// Generate Curve
// ============================================================

std::vector<Vec2> generateCurve(
    const Curve& c)
{
    std::vector<Vec2> result;

    int segments =
        std::max(
            10,
            c.segments
        );

    // 安全检查
    int requiredPoints = 0;

    switch (c.type)
    {
    case CurveType::Linear:
        requiredPoints = 2;
        break;

    case CurveType::Hermite:
        requiredPoints = 4;
        break;

    case CurveType::CatmullRom:
        requiredPoints = 4;
        break;

    case CurveType::BlendedParabola:
        requiredPoints = 5;
        break;

    case CurveType::Bezier:
        requiredPoints = 4;
        break;
    }

    if ((int)c.points.size() < requiredPoints)
        return result;

    for (int i = 0;
        i <= segments;
        ++i)
    {
        float t =
            (float)i /
            (float)segments;

        Vec2 p;

        switch (c.type)
        {
        case CurveType::Linear:

            p =
                linear(
                    c.points[0],
                    c.points[1],
                    t
                );

            break;

        case CurveType::Hermite:
        {
            Vec2 tangent0 =
                (c.points[1] -
                    c.points[0])
                * c.tangentScale;

            Vec2 tangent1 =
                (c.points[3] -
                    c.points[2])
                * c.tangentScale;

            p =
                hermite(
                    c.points[0],
                    c.points[2],
                    tangent0,
                    tangent1,
                    t
                );

            break;
        }

        case CurveType::CatmullRom:

            p =
                catmullRom(
                    c.points[0],
                    c.points[1],
                    c.points[2],
                    c.points[3],
                    c.tension,
                    t
                );

            break;

        case CurveType::BlendedParabola:
        {
            float blend =
                std::clamp(
                    c.blend,
                    0.05f,
                    0.95f
                );

            if (t < blend)
            {
                float localT =
                    t / blend;

                p =
                    quadraticBezier(
                        c.points[0],
                        c.points[1],
                        c.points[2],
                        localT
                    );
            }
            else
            {
                float localT =
                    (t - blend) /
                    (1.0f - blend);

                p =
                    quadraticBezier(
                        c.points[2],
                        c.points[3],
                        c.points[4],
                        localT
                    );
            }

            break;
        }

        case CurveType::Bezier:

            p =
                cubicBezier(
                    c.points[0],
                    c.points[1],
                    c.points[2],
                    c.points[3],
                    t
                );

            break;
        }

        result.push_back(p);
    }

    return result;
}

// ============================================================
// Initialize Curves
// ============================================================

void initializeCurves()
{
    gCurves.clear();

    // --------------------------------------------------------
    // Linear
    // --------------------------------------------------------

    {
        Curve c;

        c.type =
            CurveType::Linear;

        c.points =
        {
            {120, 520},
            {620, 180}
        };

        c.color[0] = 0.20f;
        c.color[1] = 0.85f;
        c.color[2] = 1.00f;

        c.segments = 100;

        gCurves.push_back(c);
    }

    // --------------------------------------------------------
    // Hermite
    // --------------------------------------------------------

    {
        Curve c;

        c.type =
            CurveType::Hermite;

        c.points =
        {
            {100, 520},
            {330, 180},
            {650, 220},
            {820, 520}
        };

        c.color[0] = 1.00f;
        c.color[1] = 0.35f;
        c.color[2] = 0.20f;

        c.tangentScale = 0.5f;
        c.segments = 160;

        gCurves.push_back(c);
    }

    // --------------------------------------------------------
    // Catmull-Rom
    // --------------------------------------------------------

    {
        Curve c;

        c.type =
            CurveType::CatmullRom;

        c.points =
        {
            {80, 420},
            {300, 560},
            {540, 200},
            {820, 420}
        };

        c.color[0] = 0.20f;
        c.color[1] = 1.00f;
        c.color[2] = 0.35f;

        c.tension = 0.0f;
        c.segments = 160;

        gCurves.push_back(c);
    }

    // --------------------------------------------------------
    // Blended Parabola
    // --------------------------------------------------------

    {
        Curve c;

        c.type =
            CurveType::BlendedParabola;

        c.points =
        {
            {80, 500},
            {260, 150},
            {450, 400},
            {650, 680},
            {850, 250}
        };

        c.color[0] = 1.00f;
        c.color[1] = 0.75f;
        c.color[2] = 0.10f;

        c.blend = 0.5f;
        c.segments = 160;

        gCurves.push_back(c);
    }

    // --------------------------------------------------------
    // Bezier
    // --------------------------------------------------------

    {
        Curve c;

        c.type =
            CurveType::Bezier;

        c.points =
        {
            {80, 520},
            {300, 80},
            {620, 700},
            {850, 260}
        };

        c.color[0] = 0.85f;
        c.color[1] = 0.30f;
        c.color[2] = 1.00f;

        c.segments = 160;

        gCurves.push_back(c);
    }
}

// ============================================================
// Find Control Point
// ============================================================

int findControlPoint(
    const Curve& c,
    float x,
    float y)
{
    const float radius = 18.0f;

    for (int i = 0;
        i < (int)c.points.size();
        ++i)
    {
        Vec2 d =
        {
            c.points[i].x - x,
            c.points[i].y - y
        };

        if (
            lengthSquared(d) <=
            radius * radius)
        {
            return i;
        }
    }

    return -1;
}

// ============================================================
// Curve List UI
// ============================================================

void drawCurvePanel()
{
    ImGui::Begin(
        "Curve Types"
    );

    ImGui::Text(
        "Curve Selection"
    );

    ImGui::Separator();

    for (int i = 0;
        i < (int)gCurves.size();
        ++i)
    {
        Curve& c =
            gCurves[i];

        bool selected =
            gSelectedCurve == i;

        if (ImGui::Selectable(
            getCurveName(c.type),
            selected))
        {
            gSelectedCurve = i;
            gSelectedPoint = -1;
            gDraggingPoint = false;
        }

        ImGui::SameLine();

        ImGui::ColorEdit3(
            (
                "##Color" +
                std::to_string(i)
                ).c_str(),
            c.color,
            ImGuiColorEditFlags_NoInputs
        );

        ImGui::SameLine();

        ImGui::Checkbox(
            (
                "Show##" +
                std::to_string(i)
                ).c_str(),
            &c.visible
        );
    }

    ImGui::Separator();

    ImGui::Text(
        "Display"
    );

    ImGui::Checkbox(
        "Grid",
        &gShowGrid
    );

    ImGui::Checkbox(
        "Control Polygon",
        &gShowControlPolygon
    );

    ImGui::Checkbox(
        "Control Points",
        &gShowControlPoints
    );

    ImGui::Separator();

    ImGui::Text(
        "Selected Curve"
    );

    ImGui::Text(
        "%s",
        getCurveName(
            gCurves[
                gSelectedCurve
            ].type
        )
    );

    ImGui::Separator();

    ImGui::TextWrapped(
        "Interaction:"
    );

    ImGui::TextWrapped(
        "Left-click and drag the white control points."
    );

    ImGui::TextWrapped(
        "The red point is the currently selected point."
    );

    ImGui::End();
}

// ============================================================
// Parameter Panel
// ============================================================

void drawParameterPanel()
{
    ImGui::Begin(
        "Parameters"
    );

    Curve& c =
        gCurves[gSelectedCurve];

    ImGui::Text(
        "%s",
        getCurveName(c.type)
    );

    ImGui::Separator();

    // --------------------------------------------------------
    // Parameters
    // --------------------------------------------------------

    if (c.type ==
        CurveType::Hermite)
    {
        ImGui::SliderFloat(
            "Tangent Scale",
            &c.tangentScale,
            0.0f,
            2.0f,
            "%.2f"
        );
    }

    if (c.type ==
        CurveType::CatmullRom)
    {
        ImGui::SliderFloat(
            "Tension",
            &c.tension,
            -1.0f,
            1.0f,
            "%.2f"
        );
    }

    if (c.type ==
        CurveType::BlendedParabola)
    {
        ImGui::SliderFloat(
            "Blend",
            &c.blend,
            0.05f,
            0.95f,
            "%.2f"
        );
    }

    ImGui::SliderInt(
        "Resolution",
        &c.segments,
        20,
        500
    );

    ImGui::Separator();

    // --------------------------------------------------------
    // Color
    // --------------------------------------------------------

    ImGui::Text(
        "Curve Color"
    );

    ImGui::ColorEdit3(
        "Color",
        c.color
    );

    ImGui::Separator();

    // --------------------------------------------------------
    // Control Points
    // --------------------------------------------------------

    ImGui::Text(
        "Control Points"
    );

    for (int i = 0;
        i < (int)c.points.size();
        ++i)
    {
        ImGui::PushID(i);

        char label[64];

        std::snprintf(
            label,
            sizeof(label),
            "Point %d",
            i + 1
        );

        ImGui::DragFloat2(
            label,
            &c.points[i].x,
            1.0f,
            0.0f,
            1000.0f,
            "%.1f"
        );

        ImGui::PopID();
    }

    ImGui::Separator();

    // --------------------------------------------------------
    // Reset Selected Curve
    // --------------------------------------------------------

    if (ImGui::Button(
        "Reset Selected Curve",
        ImVec2(-1, 0)))
    {
        std::vector<Curve> backup =
            gCurves;

        initializeCurves();

        if (gSelectedCurve >=
            (int)gCurves.size())
        {
            gSelectedCurve = 0;
        }

        gSelectedPoint = -1;
        gDraggingPoint = false;
    }

    // --------------------------------------------------------
    // Reset All
    // --------------------------------------------------------

    if (ImGui::Button(
        "Reset All Curves",
        ImVec2(-1, 0)))
    {
        initializeCurves();

        gSelectedCurve = 0;
        gSelectedPoint = -1;
        gDraggingPoint = false;
    }

    ImGui::End();
}

// ============================================================
// Viewport
// ============================================================

void drawViewport()
{
    // ========================================================
    // 关键修改：
    // Viewport ImGui Window 本身不绘制深色背景。
    // OpenGL 负责绘制真正的 Viewport 背景。
    // ========================================================

    ImGui::PushStyleColor(
        ImGuiCol_WindowBg,
        ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
    );

    ImGui::Begin(
        "Viewport"
    );

    ImGui::PopStyleColor();

    ImVec2 canvasPos =
        ImGui::GetCursorScreenPos();

    ImVec2 canvasSize =
        ImGui::GetContentRegionAvail();

    if (canvasSize.x < 100.0f)
        canvasSize.x = 100.0f;

    if (canvasSize.y < 100.0f)
        canvasSize.y = 100.0f;

    // ========================================================
    // Mouse Area
    // ========================================================

    ImGui::InvisibleButton(
        "Canvas",
        canvasSize
    );

    bool hovered =
        ImGui::IsItemHovered();

    ImVec2 mouse =
        ImGui::GetMousePos();

    float localX =
        mouse.x - canvasPos.x;

    float localY =
        mouse.y - canvasPos.y;

    // ========================================================
    // Click Control Point
    // ========================================================

    if (
        hovered &&
        ImGui::IsMouseClicked(
            ImGuiMouseButton_Left))
    {
        Curve& c =
            gCurves[gSelectedCurve];

        int index =
            findControlPoint(
                c,
                localX,
                localY
            );

        if (index >= 0)
        {
            gSelectedPoint = index;

            gDraggingPoint = true;
        }
        else
        {
            gSelectedPoint = -1;
        }
    }

    // ========================================================
    // Drag Control Point
    // ========================================================

    if (
        gDraggingPoint &&
        ImGui::IsMouseDown(
            ImGuiMouseButton_Left))
    {
        Curve& c =
            gCurves[gSelectedCurve];

        if (
            gSelectedPoint >= 0 &&
            gSelectedPoint <
            (int)c.points.size())
        {
            c.points[gSelectedPoint] =
            {
                localX,
                localY
            };
        }
    }

    // ========================================================
    // Release
    // ========================================================

    if (
        ImGui::IsMouseReleased(
            ImGuiMouseButton_Left))
    {
        gDraggingPoint = false;
    }

    // ========================================================
    // OpenGL Viewport
    // ========================================================

    int viewportX =
        (int)canvasPos.x;

    int viewportY =
        gWindowHeight -
        (int)(
            canvasPos.y +
            canvasSize.y
            );

    int viewportWidth =
        (int)canvasSize.x;

    int viewportHeight =
        (int)canvasSize.y;

    glEnable(
        GL_SCISSOR_TEST
    );

    glScissor(
        viewportX,
        viewportY,
        viewportWidth,
        viewportHeight
    );

    // ========================================================
    // Viewport Background
    // ========================================================
    //
    // 比原来的 0.055 更亮。
    // 同时与 ImGui Window 背景完全分离。
    //

    glClearColor(
        0.12f,
        0.14f,
        0.18f,
        1.0f
    );

    glClear(
        GL_COLOR_BUFFER_BIT
    );

    // ========================================================
    // Grid
    // ========================================================

    if (gShowGrid)
    {
        const float gridSize = 50.0f;

        // Vertical
        for (
            float x = 0.0f;
            x <= canvasSize.x;
            x += gridSize)
        {
            drawPolyline(
                {
                    {x, 0.0f},
                    {x, canvasSize.y}
                },
                0.24f,
                0.27f,
                0.33f,
                1.0f,
                canvasPos.x,
                canvasPos.y
            );
        }

        // Horizontal
        for (
            float y = 0.0f;
            y <= canvasSize.y;
            y += gridSize)
        {
            drawPolyline(
                {
                    {0.0f, y},
                    {canvasSize.x, y}
                },
                0.24f,
                0.27f,
                0.33f,
                1.0f,
                canvasPos.x,
                canvasPos.y
            );
        }
    }

    // ========================================================
    // Draw Curves
    // ========================================================

    for (
        int i = 0;
        i < (int)gCurves.size();
        ++i)
    {
        Curve& c =
            gCurves[i];

        if (!c.visible)
            continue;

        // ----------------------------------------------------
        // Control Polygon
        // ----------------------------------------------------

        if (
            gShowControlPolygon &&
            c.points.size() >= 2)
        {
            drawPolyline(
                c.points,
                0.48f,
                0.50f,
                0.56f,
                1.0f,
                canvasPos.x,
                canvasPos.y
            );
        }

        // ----------------------------------------------------
        // Actual Curve
        // ----------------------------------------------------

        std::vector<Vec2> samples =
            generateCurve(c);

        if (!samples.empty())
        {
            drawPolyline(
                samples,
                c.color[0],
                c.color[1],
                c.color[2],
                4.0f,
                canvasPos.x,
                canvasPos.y
            );
        }

        // ----------------------------------------------------
        // Control Points
        // ----------------------------------------------------

        if (gShowControlPoints)
        {
            drawPoints(
                c.points,
                0.95f,
                0.95f,
                1.0f,
                10.0f,
                canvasPos.x,
                canvasPos.y
            );

            // Selected Point
            if (
                i == gSelectedCurve &&
                gSelectedPoint >= 0 &&
                gSelectedPoint <
                (int)c.points.size())
            {
                std::vector<Vec2> selected =
                {
                    c.points[gSelectedPoint]
                };

                drawPoints(
                    selected,
                    1.0f,
                    0.20f,
                    0.15f,
                    16.0f,
                    canvasPos.x,
                    canvasPos.y
                );
            }
        }
    }

    // ========================================================
    // Viewport Border
    // ========================================================

    std::vector<Vec2> border =
    {
        {0.0f, 0.0f},
        {canvasSize.x, 0.0f},
        {canvasSize.x, canvasSize.y},
        {0.0f, canvasSize.y},
        {0.0f, 0.0f}
    };

    drawPolyline(
        border,
        0.40f,
        0.44f,
        0.52f,
        1.0f,
        canvasPos.x,
        canvasPos.y
    );

    glDisable(
        GL_SCISSOR_TEST
    );

    ImGui::End();
}

// ============================================================
// Main
// ============================================================

int main()
{
    // ========================================================
    // GLFW
    // ========================================================

    if (!glfwInit())
    {
        printf(
            "GLFW initialization failed.\n"
        );

        return -1;
    }

    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MAJOR,
        3
    );

    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MINOR,
        3
    );

    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

#ifdef __APPLE__

    glfwWindowHint(
        GLFW_OPENGL_FORWARD_COMPAT,
        GL_TRUE
    );

#endif

    gWindow =
        glfwCreateWindow(
            gWindowWidth,
            gWindowHeight,
            "Curve Editor",
            nullptr,
            nullptr
        );

    if (!gWindow)
    {
        glfwTerminate();

        return -1;
    }

    glfwMakeContextCurrent(
        gWindow
    );

    glfwSwapInterval(1);

    // ========================================================
    // GLAD
    // ========================================================

    if (!gladLoadGLLoader(
        (GLADloadproc)
        glfwGetProcAddress))
    {
        printf(
            "GLAD initialization failed.\n"
        );

        glfwDestroyWindow(
            gWindow
        );

        glfwTerminate();

        return -1;
    }

    // ========================================================
    // OpenGL
    // ========================================================

    glViewport(
        0,
        0,
        gWindowWidth,
        gWindowHeight
    );

    glDisable(
        GL_DEPTH_TEST
    );

    createShader();

    // ========================================================
    // ImGui
    // ========================================================

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io =
        ImGui::GetIO();

    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Slightly improve ImGui appearance
    ImGuiStyle& style =
        ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    // ========================================================
    // ImGui GLFW Backend
    // ========================================================

    ImGui_ImplGlfw_InitForOpenGL(
        gWindow,
        true
    );

    // ========================================================
    // ImGui OpenGL3 Backend
    // ========================================================

    ImGui_ImplOpenGL3_Init(
        "#version 330"
    );

    // ========================================================
    // Curves
    // ========================================================

    initializeCurves();

    // ========================================================
    // Main Loop
    // ========================================================

    while (
        !glfwWindowShouldClose(
            gWindow))
    {
        glfwPollEvents();

        // ----------------------------------------------------
        // Actual framebuffer size
        // ----------------------------------------------------

        glfwGetFramebufferSize(
            gWindow,
            &gWindowWidth,
            &gWindowHeight
        );

        glViewport(
            0,
            0,
            gWindowWidth,
            gWindowHeight
        );

        // ----------------------------------------------------
        // ImGui New Frame
        // ----------------------------------------------------

        ImGui_ImplOpenGL3_NewFrame();

        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        // ----------------------------------------------------
        // Layout
        // ----------------------------------------------------

        float leftWidth = 250.0f;
        float rightWidth = 300.0f;

        float centerWidth =
            (float)gWindowWidth -
            leftWidth -
            rightWidth;

        if (centerWidth < 200.0f)
            centerWidth = 200.0f;

        // ====================================================
        // Left
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(0, 0),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(
                leftWidth,
                (float)gWindowHeight
            ),
            ImGuiCond_Always
        );

        drawCurvePanel();

        // ====================================================
        // Right
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(
                (float)gWindowWidth -
                rightWidth,
                0
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(
                rightWidth,
                (float)gWindowHeight
            ),
            ImGuiCond_Always
        );

        drawParameterPanel();

        // ====================================================
        // Center Viewport
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(
                leftWidth,
                0
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(
                centerWidth,
                (float)gWindowHeight
            ),
            ImGuiCond_Always
        );

        drawViewport();

        // ====================================================
        // Render ImGui
        // ====================================================

        ImGui::Render();

        glDisable(
            GL_SCISSOR_TEST
        );

        // 注意：
        // 这里绝对不要再 glClear。
        // Viewport 已经在 drawViewport() 中清理。
        // 否则会把刚画好的曲线清掉。

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(
            gWindow
        );
    }

    // ========================================================
    // Cleanup
    // ========================================================

    ImGui_ImplOpenGL3_Shutdown();

    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glDeleteProgram(
        gShaderProgram
    );

    glfwDestroyWindow(
        gWindow
    );

    glfwTerminate();

    return 0;
}