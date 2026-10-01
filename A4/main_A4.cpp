// ============================================================
// Quaternion Rotation Demo
// OpenGL 3.3 + GLFW + GLAD + Dear ImGui
//
// 功能：
// 1. 使用四元数控制 Cube 旋转
// 2. 可以直接修改 X/Y/Z/W
// 3. 四元数归一化
// 4. 常用旋转预设
// 5. 显示四元数长度
// 6. 显示对应的 3x3 旋转矩阵
// ============================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

// ============================================================
// 基本结构
// ============================================================

struct Vec3
{
    float x, y, z;
};

struct Quaternion
{
    float x, y, z, w;
};

// ============================================================
// Quaternion
// ============================================================

static float quaternionLength(const Quaternion& q)
{
    return std::sqrt(
        q.x * q.x +
        q.y * q.y +
        q.z * q.z +
        q.w * q.w
    );
}

static Quaternion normalizeQuaternion(Quaternion q)
{
    float len = quaternionLength(q);

    if (len < 0.000001f)
    {
        q.x = 0.0f;
        q.y = 0.0f;
        q.z = 0.0f;
        q.w = 1.0f;
        return q;
    }

    q.x /= len;
    q.y /= len;
    q.z /= len;
    q.w /= len;

    return q;
}

// ============================================================
// Axis Angle -> Quaternion
// ============================================================

static Quaternion quaternionFromAxisAngle(
    float axisX,
    float axisY,
    float axisZ,
    float angleDegree)
{
    const float PI = 3.14159265358979323846f;

    float angleRad = angleDegree * PI / 180.0f;
    float halfAngle = angleRad * 0.5f;

    float s = std::sin(halfAngle);
    float c = std::cos(halfAngle);

    Quaternion q;

    q.x = axisX * s;
    q.y = axisY * s;
    q.z = axisZ * s;
    q.w = c;

    return normalizeQuaternion(q);
}

// ============================================================
// Quaternion -> 4x4 Rotation Matrix
//
// OpenGL 使用 column-major 数据排列
// ============================================================

static void quaternionToMatrix(
    const Quaternion& input,
    float m[16])
{
    Quaternion q = normalizeQuaternion(input);

    float x = q.x;
    float y = q.y;
    float z = q.z;
    float w = q.w;

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;

    float xy = x * y;
    float xz = x * z;
    float yz = y * z;

    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    // column-major
    m[0] = 1.0f - 2.0f * (yy + zz);
    m[1] = 2.0f * (xy + wz);
    m[2] = 2.0f * (xz - wy);
    m[3] = 0.0f;

    m[4] = 2.0f * (xy - wz);
    m[5] = 1.0f - 2.0f * (xx + zz);
    m[6] = 2.0f * (yz + wx);
    m[7] = 0.0f;

    m[8] = 2.0f * (xz + wy);
    m[9] = 2.0f * (yz - wx);
    m[10] = 1.0f - 2.0f * (xx + yy);
    m[11] = 0.0f;

    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
}

// ============================================================
// Matrix
// ============================================================

static void identityMatrix(float m[16])
{
    for (int i = 0; i < 16; ++i)
        m[i] = 0.0f;

    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void multiplyMatrix(
    const float a[16],
    const float b[16],
    float result[16])
{
    float temp[16];

    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            temp[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }

    std::memcpy(result, temp, sizeof(temp));
}

// ============================================================
// Perspective
// ============================================================

static void perspectiveMatrix(
    float fovDegree,
    float aspect,
    float nearPlane,
    float farPlane,
    float m[16])
{
    const float PI = 3.14159265358979323846f;

    float fovRad = fovDegree * PI / 180.0f;
    float f = 1.0f / std::tan(fovRad * 0.5f);

    for (int i = 0; i < 16; ++i)
        m[i] = 0.0f;

    m[0] = f / aspect;
    m[5] = f;

    m[10] =
        (farPlane + nearPlane) /
        (nearPlane - farPlane);

    m[11] = -1.0f;

    m[14] =
        (2.0f * farPlane * nearPlane) /
        (nearPlane - farPlane);
}

// ============================================================
// Look At
// ============================================================

static Vec3 vecSub(Vec3 a, Vec3 b)
{
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

static Vec3 vecCross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float vecDot(Vec3 a, Vec3 b)
{
    return a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

static Vec3 vecNormalize(Vec3 v)
{
    float len =
        std::sqrt(
            v.x * v.x +
            v.y * v.y +
            v.z * v.z
        );

    if (len < 0.000001f)
        return { 0, 0, 0 };

    return {
        v.x / len,
        v.y / len,
        v.z / len
    };
}

static void lookAt(
    Vec3 eye,
    Vec3 center,
    Vec3 up,
    float m[16])
{
    Vec3 f = vecNormalize(vecSub(center, eye));
    Vec3 s = vecNormalize(vecCross(f, up));
    Vec3 u = vecCross(s, f);

    identityMatrix(m);

    m[0] = s.x;
    m[1] = s.y;
    m[2] = s.z;

    m[4] = u.x;
    m[5] = u.y;
    m[6] = u.z;

    m[8] = -f.x;
    m[9] = -f.y;
    m[10] = -f.z;

    m[12] = -vecDot(s, eye);
    m[13] = -vecDot(u, eye);
    m[14] = vecDot(f, eye);
}

// ============================================================
// Shader
// ============================================================

static const char* vertexShaderSource = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uMVP;

out vec3 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* fragmentShaderSource = R"(
#version 330 core

in vec3 vColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
)";

// ============================================================
// Shader 编译
// ============================================================

static GLuint compileShader(
    GLenum type,
    const char* source)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(
        shader,
        1,
        &source,
        nullptr
    );

    glCompileShader(shader);

    GLint success = 0;

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        char infoLog[1024];

        glGetShaderInfoLog(
            shader,
            1024,
            nullptr,
            infoLog
        );

        std::printf(
            "Shader compile error:\n%s\n",
            infoLog
        );
    }

    return shader;
}

static GLuint createShaderProgram()
{
    GLuint vs =
        compileShader(
            GL_VERTEX_SHADER,
            vertexShaderSource
        );

    GLuint fs =
        compileShader(
            GL_FRAGMENT_SHADER,
            fragmentShaderSource
        );

    GLuint program =
        glCreateProgram();

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    GLint success = 0;

    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &success
    );

    if (!success)
    {
        char infoLog[1024];

        glGetProgramInfoLog(
            program,
            1024,
            nullptr,
            infoLog
        );

        std::printf(
            "Shader link error:\n%s\n",
            infoLog
        );
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

// ============================================================
// Cube 数据
//
// 每个面独立定义颜色
// ============================================================

static float cubeVertices[] =
{
    // position              color

    // Front
    -1,-1, 1,                1,0,0,
     1,-1, 1,                1,0,0,
     1, 1, 1,                1,0,0,

     1, 1, 1,                1,0,0,
    -1, 1, 1,                1,0,0,
    -1,-1, 1,                1,0,0,

    // Back
    -1,-1,-1,                0,1,0,
    -1, 1,-1,                0,1,0,
     1, 1,-1,                0,1,0,

     1, 1,-1,                0,1,0,
     1,-1,-1,                0,1,0,
    -1,-1,-1,                0,1,0,

    // Left
    -1,-1,-1,                0,0,1,
    -1,-1, 1,                0,0,1,
    -1, 1, 1,                0,0,1,

    -1, 1, 1,                0,0,1,
    -1, 1,-1,                0,0,1,
    -1,-1,-1,                0,0,1,

    // Right
     1,-1,-1,                1,1,0,
     1, 1,-1,                1,1,0,
     1, 1, 1,                1,1,0,

     1, 1, 1,                1,1,0,
     1,-1, 1,                1,1,0,
     1,-1,-1,                1,1,0,

     // Top
     -1, 1,-1,                1,0,1,
     -1, 1, 1,                1,0,1,
      1, 1, 1,                1,0,1,

      1, 1, 1,                1,0,1,
      1, 1,-1,                1,0,1,
     -1, 1,-1,                1,0,1,

     // Bottom
     -1,-1,-1,                0,1,1,
      1,-1,-1,                0,1,1,
      1,-1, 1,                0,1,1,

      1,-1, 1,                0,1,1,
     -1,-1, 1,                0,1,1,
     -1,-1,-1,                0,1,1
};

// ============================================================
// Axis 数据
// ============================================================

static float axisVertices[] =
{
    // X
    0,0,0,   1,0,0,
    3,0,0,   1,0,0,

    // Y
    0,0,0,   0,1,0,
    0,3,0,   0,1,0,

    // Z
    0,0,0,   0,0,1,
    0,0,3,   0,0,1
};

// ============================================================
// Draw Cube
// ============================================================

static void drawCube(
    GLuint shader,
    GLuint vao,
    float mvp[16])
{
    glUseProgram(shader);

    GLint location =
        glGetUniformLocation(
            shader,
            "uMVP"
        );

    glUniformMatrix4fv(
        location,
        1,
        GL_FALSE,
        mvp
    );

    glBindVertexArray(vao);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        36
    );

    glBindVertexArray(0);
}

// ============================================================
// Main
// ============================================================

int main()
{
    // --------------------------------------------------------
    // GLFW
    // --------------------------------------------------------

    if (!glfwInit())
    {
        std::printf("Failed to initialize GLFW.\n");
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

    GLFWwindow* window =
        glfwCreateWindow(
            1400,
            800,
            "Quaternion Rotation Demo",
            nullptr,
            nullptr
        );

    if (!window)
    {
        std::printf("Failed to create GLFW window.\n");

        glfwTerminate();

        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    // --------------------------------------------------------
    // GLAD
    // --------------------------------------------------------

    if (!gladLoadGLLoader(
        (GLADloadproc)glfwGetProcAddress))
    {
        std::printf("Failed to initialize GLAD.\n");

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }

    // --------------------------------------------------------
    // OpenGL
    // --------------------------------------------------------

    glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);

    glCullFace(GL_BACK);

    glFrontFace(GL_CCW);

    glClearColor(
        0.08f,
        0.09f,
        0.12f,
        1.0f
    );

    // --------------------------------------------------------
    // Shader
    // --------------------------------------------------------

    GLuint shader =
        createShaderProgram();

    // --------------------------------------------------------
    // Cube VAO / VBO
    // --------------------------------------------------------

    GLuint cubeVAO = 0;
    GLuint cubeVBO = 0;

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        cubeVBO
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(cubeVertices),
        cubeVertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );

    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // --------------------------------------------------------
    // Axis VAO / VBO
    // --------------------------------------------------------

    GLuint axisVAO = 0;
    GLuint axisVBO = 0;

    glGenVertexArrays(1, &axisVAO);
    glGenBuffers(1, &axisVBO);

    glBindVertexArray(axisVAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        axisVBO
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(axisVertices),
        axisVertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );

    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // --------------------------------------------------------
    // Dear ImGui
    // --------------------------------------------------------

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    (void)io;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(
        window,
        true
    );

    ImGui_ImplOpenGL3_Init(
        "#version 330"
    );

    // --------------------------------------------------------
    // Quaternion
    // --------------------------------------------------------

    Quaternion q;

    q.x = 0.0f;
    q.y = 0.0f;
    q.z = 0.0f;
    q.w = 1.0f;

    // --------------------------------------------------------
    // Main Loop
    // --------------------------------------------------------

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // ----------------------------------------------------
        // 获取窗口尺寸
        // ----------------------------------------------------

        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(
            window,
            &framebufferWidth,
            &framebufferHeight
        );

        if (framebufferHeight <= 0)
            framebufferHeight = 1;

        glViewport(
            0,
            0,
            framebufferWidth,
            framebufferHeight
        );

        // ----------------------------------------------------
        // Clear
        // ----------------------------------------------------

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        // ----------------------------------------------------
        // ImGui Frame
        // ----------------------------------------------------

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ====================================================
        // 左侧：四元数参数
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(10, 10),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(250, 780),
            ImGuiCond_Always
        );

        ImGui::Begin(
            "Quaternion Parameters",
            nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove
        );

        ImGui::Text(
            "Quaternion Rotation"
        );

        ImGui::Separator();

        ImGui::Text(
            "Edit quaternion values:"
        );

        ImGui::Spacing();

        ImGui::Text("X");

        ImGui::SameLine(45);

        ImGui::SetNextItemWidth(170);

        ImGui::DragFloat(
            "##qx",
            &q.x,
            0.01f,
            -1.0f,
            1.0f,
            "%.4f"
        );

        ImGui::Text("Y");

        ImGui::SameLine(45);

        ImGui::SetNextItemWidth(170);

        ImGui::DragFloat(
            "##qy",
            &q.y,
            0.01f,
            -1.0f,
            1.0f,
            "%.4f"
        );

        ImGui::Text("Z");

        ImGui::SameLine(45);

        ImGui::SetNextItemWidth(170);

        ImGui::DragFloat(
            "##qz",
            &q.z,
            0.01f,
            -1.0f,
            1.0f,
            "%.4f"
        );

        ImGui::Text("W");

        ImGui::SameLine(45);

        ImGui::SetNextItemWidth(170);

        ImGui::DragFloat(
            "##qw",
            &q.w,
            0.01f,
            -1.0f,
            1.0f,
            "%.4f"
        );

        ImGui::Spacing();

        float qLength =
            quaternionLength(q);

        ImGui::Text(
            "Length: %.6f",
            qLength
        );

        if (std::abs(qLength - 1.0f) > 0.0001f)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                "Quaternion is not normalized"
            );
        }
        else
        {
            ImGui::TextColored(
                ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                "Quaternion is normalized"
            );
        }

        ImGui::Spacing();

        if (ImGui::Button(
            "Normalize",
            ImVec2(-1, 32)))
        {
            q = normalizeQuaternion(q);
        }

        if (ImGui::Button(
            "Reset",
            ImVec2(-1, 32)))
        {
            q.x = 0.0f;
            q.y = 0.0f;
            q.z = 0.0f;
            q.w = 1.0f;
        }

        ImGui::Spacing();

        ImGui::Separator();

        ImGui::Text(
            "Rotation Presets"
        );

        ImGui::Spacing();

        if (ImGui::Button(
            "Identity",
            ImVec2(-1, 30)))
        {
            q.x = 0.0f;
            q.y = 0.0f;
            q.z = 0.0f;
            q.w = 1.0f;
        }

        if (ImGui::Button(
            "X 90 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    1, 0, 0, 90
                );
        }

        if (ImGui::Button(
            "Y 90 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    0, 1, 0, 90
                );
        }

        if (ImGui::Button(
            "Z 90 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    0, 0, 1, 90
                );
        }

        if (ImGui::Button(
            "X 180 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    1, 0, 0, 180
                );
        }

        if (ImGui::Button(
            "Y 180 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    0, 1, 0, 180
                );
        }

        if (ImGui::Button(
            "Z 180 deg",
            ImVec2(-1, 30)))
        {
            q =
                quaternionFromAxisAngle(
                    0, 0, 1, 180
                );
        }

        ImGui::End();

        // ====================================================
        // 右侧：旋转矩阵
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(
                (float)framebufferWidth - 300.0f,
                10.0f
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(290, 780),
            ImGuiCond_Always
        );

        ImGui::Begin(
            "Rotation Information",
            nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove
        );

        ImGui::Text(
            "Quaternion"
        );

        ImGui::Separator();

        ImGui::Text(
            "X = %.5f",
            q.x
        );

        ImGui::Text(
            "Y = %.5f",
            q.y
        );

        ImGui::Text(
            "Z = %.5f",
            q.z
        );

        ImGui::Text(
            "W = %.5f",
            q.w
        );

        ImGui::Spacing();

        ImGui::Separator();

        ImGui::Text(
            "Rotation Matrix"
        );

        float rotationMatrix[16];

        quaternionToMatrix(
            q,
            rotationMatrix
        );

        ImGui::Spacing();

        ImGui::Text(
            "[ %.3f   %.3f   %.3f ]",
            rotationMatrix[0],
            rotationMatrix[4],
            rotationMatrix[8]
        );

        ImGui::Text(
            "[ %.3f   %.3f   %.3f ]",
            rotationMatrix[1],
            rotationMatrix[5],
            rotationMatrix[9]
        );

        ImGui::Text(
            "[ %.3f   %.3f   %.3f ]",
            rotationMatrix[2],
            rotationMatrix[6],
            rotationMatrix[10]
        );

        ImGui::Spacing();

        ImGui::Separator();

        ImGui::Text(
            "Formula"
        );

        ImGui::Spacing();

        ImGui::TextWrapped(
            "Quaternion q = (x, y, z, w)"
        );

        ImGui::Spacing();

        ImGui::TextWrapped(
            "The quaternion is converted "
            "to a rotation matrix and "
            "applied to the cube."
        );

        ImGui::Spacing();

        ImGui::Separator();

        ImGui::Text(
            "Controls"
        );

        ImGui::BulletText(
            "Edit X/Y/Z/W"
        );

        ImGui::BulletText(
            "Normalize quaternion"
        );

        ImGui::BulletText(
            "Use rotation presets"
        );

        ImGui::BulletText(
            "Cube updates in real time"
        );

        ImGui::End();

        // ====================================================
        // 中间 OpenGL 场景
        // ====================================================

        // 中间区域宽度
        float leftWidth = 250.0f;
        float rightWidth = 300.0f;

        float centerLeft = leftWidth + 10.0f;
        float centerRight =
            (float)framebufferWidth -
            rightWidth -
            10.0f;

        float centerWidth =
            centerRight - centerLeft;

        float centerHeight =
            (float)framebufferHeight;

        if (centerWidth < 100.0f)
            centerWidth = 100.0f;

        // ----------------------------------------------------
        // Camera
        // ----------------------------------------------------

        float aspect =
            centerWidth / centerHeight;

        float projection[16];

        perspectiveMatrix(
            45.0f,
            aspect,
            0.1f,
            100.0f,
            projection
        );

        float view[16];

        lookAt(
            { 5.0f, 4.0f, 7.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            view
        );

        // ----------------------------------------------------
        // Model = Quaternion Rotation
        // ----------------------------------------------------

        float model[16];

        quaternionToMatrix(
            q,
            model
        );

        // ----------------------------------------------------
        // MVP
        // ----------------------------------------------------

        float viewModel[16];
        float mvp[16];

        multiplyMatrix(
            view,
            model,
            viewModel
        );

        multiplyMatrix(
            projection,
            viewModel,
            mvp
        );

        // ----------------------------------------------------
        // OpenGL 场景区域
        // ----------------------------------------------------

        glViewport(
            (int)centerLeft,
            0,
            (int)centerWidth,
            framebufferHeight
        );

        // 清除中间区域
        glEnable(GL_SCISSOR_TEST);

        glScissor(
            (int)centerLeft,
            0,
            (int)centerWidth,
            framebufferHeight
        );

        glClearColor(
            0.10f,
            0.11f,
            0.15f,
            1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        glDisable(GL_SCISSOR_TEST);

        // ----------------------------------------------------
        // Draw Cube
        // ----------------------------------------------------

        drawCube(
            shader,
            cubeVAO,
            mvp
        );

        // ----------------------------------------------------
        // Draw Axis
        //
        // 使用无旋转的坐标轴，
        // 方便观察 Cube 的旋转。
        // ----------------------------------------------------

        float axisModel[16];
        float axisViewModel[16];
        float axisMVP[16];

        identityMatrix(axisModel);

        multiplyMatrix(
            view,
            axisModel,
            axisViewModel
        );

        multiplyMatrix(
            projection,
            axisViewModel,
            axisMVP
        );

        glUseProgram(shader);

        GLint axisMvpLocation =
            glGetUniformLocation(
                shader,
                "uMVP"
            );

        glUniformMatrix4fv(
            axisMvpLocation,
            1,
            GL_FALSE,
            axisMVP
        );

        glBindVertexArray(axisVAO);

        glLineWidth(3.0f);

        glDrawArrays(
            GL_LINES,
            0,
            6
        );

        glBindVertexArray(0);

        // ----------------------------------------------------
        // 恢复默认 framebuffer viewport
        // ----------------------------------------------------

        glViewport(
            0,
            0,
            framebufferWidth,
            framebufferHeight
        );

        // ====================================================
        // ImGui 中央标题
        // ====================================================

        ImGui::SetNextWindowPos(
            ImVec2(
                centerLeft + 10.0f,
                15.0f
            ),
            ImGuiCond_Always
        );

        ImGui::SetNextWindowSize(
            ImVec2(
                centerWidth - 20.0f,
                55.0f
            ),
            ImGuiCond_Always
        );

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(
                0.05f,
                0.05f,
                0.05f,
                0.65f
            )
        );

        ImGui::Begin(
            "SceneTitle",
            nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar
        );

        ImGui::Text(
            "Quaternion Rotation Demonstration"
        );

        ImGui::SameLine();

        ImGui::Text(
            "    Cube"
        );

        ImGui::End();

        ImGui::PopStyleColor();

        // ----------------------------------------------------
        // ImGui Render
        // ----------------------------------------------------

        ImGui::Render();

        glDisable(GL_SCISSOR_TEST);

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
    }

    // ========================================================
    // Cleanup
    // ========================================================

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glDeleteVertexArrays(
        1,
        &cubeVAO
    );

    glDeleteBuffers(
        1,
        &cubeVBO
    );

    glDeleteVertexArrays(
        1,
        &axisVAO
    );

    glDeleteBuffers(
        1,
        &axisVBO
    );

    glDeleteProgram(shader);

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}
