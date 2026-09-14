using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[RequireComponent(typeof(Camera))]
public class ObjGLViewer : MonoBehaviour
{
    [Header("OBJAbsolotePath")]
    public string objFilePath = "C:/Models/model.obj";

    [Header("ModelColor")]
    public Color modelColor = Color.white;

    [Header("LightDirection(WCS)")]
    public Vector3 lightDir = new Vector3(1f, 1f, 1f);

    [Header("CamOrbitParame")]
    public Vector3 target = Vector3.zero; // 相机环绕的中心点
    public float distance = 5f;           // 相机到中心的距离
    public float yaw = 0f;           // 水平角（度）
    public float pitch = 20f;          // 俯仰角（度）
    public float rotateSpeed = 200f;
    public float panSpeed = 0.005f;
    public float zoomSpeed = 5f;

    [Header("ProjectionParame")]
    public float fov = 60f;
    public float nearClip = 0.1f;
    public float farClip = 200f;

    private ObjParser parser;
    private Material glMat;
    private bool loaded;

    void Awake()
    {
        // 用 Unity 内置的 GL 专用 shader，支持顶点颜色 + 深度测试
        glMat = new Material(Shader.Find("Hidden/Internal-Colored"));
        glMat.hideFlags = HideFlags.HideAndDontSave;
        glMat.SetInt("_Cull", (int)UnityEngine.Rendering.CullMode.Off); // 不剔除，绕序可能不统一
        glMat.SetInt("_ZWrite", 1);
        glMat.SetInt("_ZTest", (int)UnityEngine.Rendering.CompareFunction.LessEqual);

        if (System.IO.File.Exists(objFilePath))
        {
            parser = new ObjParser();
            parser.Parse(objFilePath);
            ComputeNormalsIfMissing();
            loaded = true;
            Debug.Log($"Load_OK: {parser.finalVertices.Count} Vertices, {parser.triangles.Count / 3} Triangles");
        }
        else
        {
            Debug.LogError("OBJ_NOTFOUND: " + objFilePath);
        }
    }

    // 如果 OBJ 里没有法线，自己用叉积算（面法线累加到顶点，得到顶点法线）
    void ComputeNormalsIfMissing()
    {
        if (parser.hasNormals && parser.finalNormals.Count == parser.finalVertices.Count)
            return;

        parser.finalNormals.Clear();
        for (int i = 0; i < parser.finalVertices.Count; i++)
            parser.finalNormals.Add(Vector3.zero);

        for (int i = 0; i < parser.triangles.Count; i += 3)
        {
            int i0 = parser.triangles[i];
            int i1 = parser.triangles[i + 1];
            int i2 = parser.triangles[i + 2];

            Vector3 v0 = parser.finalVertices[i0];
            Vector3 v1 = parser.finalVertices[i1];
            Vector3 v2 = parser.finalVertices[i2];

            // 面法线 = (v1 - v0) × (v2 - v0)
            Vector3 n = Vector3.Cross(v1 - v0, v2 - v0);
            parser.finalNormals[i0] += n;
            parser.finalNormals[i1] += n;
            parser.finalNormals[i2] += n;
        }

        for (int i = 0; i < parser.finalNormals.Count; i++)
            parser.finalNormals[i] = parser.finalNormals[i].normalized;
    }

    void Update()
    {
        if (!loaded) return;

        // 左键拖动：绕 target 旋转相机
        if (Input.GetMouseButton(0))
        {
            yaw += Input.GetAxis("Mouse X") * rotateSpeed * Time.deltaTime;
            pitch -= Input.GetAxis("Mouse Y") * rotateSpeed * Time.deltaTime;
            pitch = Mathf.Clamp(pitch, -89f, 89f);
        }

        // 右键拖动：沿相机右/上方向平移 target
        if (Input.GetMouseButton(1))
        {
            Quaternion rot = Quaternion.Euler(pitch, yaw, 0f);
            Vector3 right = rot * Vector3.right;
            Vector3 up = rot * Vector3.up;
            float scale = distance * panSpeed;
            target -= right * Input.GetAxis("Mouse X") * scale;
            target -= up * Input.GetAxis("Mouse Y") * scale;
        }

        // 滚轮缩放
        float scroll = Input.GetAxis("Mouse ScrollWheel");
        if (Mathf.Abs(scroll) > 1e-4f)
        {
            distance -= scroll * zoomSpeed * distance;
            distance = Mathf.Clamp(distance, 0.3f, 200f);
        }
    }

    void OnPostRender()
    {
        if (!loaded || glMat == null) return;

        // ---------- 1. 构造相机在世界空间的位置和朝向 ----------
        Quaternion camRot = Quaternion.Euler(pitch, yaw, 0f);
        Vector3 camPos = target + camRot * new Vector3(0f, 0f, -distance);
        Vector3 forward = (target - camPos).normalized;
        Vector3 right = Vector3.Cross(forward, Vector3.up).normalized;
        Vector3 up = Vector3.Cross(right, forward);

        // ---------- 2. 视图矩阵 (World → Camera) ----------
        // 行向量 = 相机基在世界中的表示；第 4 列是 -R·camPos（把相机平移到原点）
        Matrix4x4 view = new Matrix4x4();
        view.m00 = right.x; view.m01 = right.y; view.m02 = right.z; view.m03 = -Vector3.Dot(right, camPos);
        view.m10 = up.x; view.m11 = up.y; view.m12 = up.z; view.m13 = -Vector3.Dot(up, camPos);
        view.m20 = -forward.x; view.m21 = -forward.y; view.m22 = -forward.z; view.m23 = Vector3.Dot(forward, camPos);
        view.m30 = 0f; view.m31 = 0f; view.m32 = 0f; view.m33 = 1f;

        // ---------- 3. 投影矩阵 (Camera → Clip) ----------
        float aspect = (float)Screen.width / Screen.height;
        Matrix4x4 proj = Matrix4x4.Perspective(fov, aspect, nearClip, farClip);

        // ---------- 4. 模型矩阵 (Model → World) ----------
        // 这里模型就在原点，如果需要自己施加旋转/平移就改这个矩阵
        Matrix4x4 model = Matrix4x4.identity;

        // ---------- 5. 提交绘制 ----------
        glMat.SetPass(0);

        GL.PushMatrix();
        GL.LoadProjectionMatrix(proj);
        GL.modelview = view * model;  // 注意顺序：先模型后视图

        GL.Begin(GL.TRIANGLES);
        Vector3 L = lightDir.normalized;

        for (int i = 0; i < parser.triangles.Count; i++)
        {
            int idx = parser.triangles[i];
            Vector3 v = parser.finalVertices[idx];
            Vector3 n = parser.finalNormals[idx];

            // Lambert 漫反射：max(0, N·L)，加一个环境光底
            float ndl = Mathf.Max(0f, Vector3.Dot(n, L));
            Color c = modelColor * (0.15f + 0.85f * ndl);
            c.a = 1f;

            GL.Color(c);
            GL.Vertex3(v.x, v.y, v.z); // 提交的是模型空间顶点，GPU 会自动乘 modelview 和 projection
        }

        GL.End();
        GL.PopMatrix();
    }
}