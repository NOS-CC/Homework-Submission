using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// A small particle system implemented from scratch (does NOT use Unity's ParticleSystem).
/// Physics: explicit Euler integration, p += v*dt; v += g*dt.
/// Rendering: one dynamically updated billboard mesh.
/// Attach to an empty GameObject.
/// </summary>
[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class CustomParticleSimulation : MonoBehaviour
{
    [Header("Emission")]
    [Min(0f)] public float emissionRate = 60f;       // particles per second
    [Min(1)] public int maxParticles = 1500;
    [Min(0.01f)] public float particleLifetime = 2.5f;
    [Min(0f)] public float emissionConeAngle = 12f;
    [Min(0f)] public float initialSpeed = 4f;
    [Min(0f)] public float speedRandomness = 0.5f;

    [Header("Particle Physics")]
    public Vector3 gravity = Vector3.zero;
    [Range(0f, 1f)] public float airResistance = 0f;
    [Min(0.001f)] public float particleSize = 0.12f;
    [Min(0f)] public float sizeRandomness = 0.03f;

    [Header("Appearance")]
    public Color colorA = new Color(0.15f, 0.8f, 1f, 1f);
    public Color colorB = new Color(1f, 0.25f, 0.35f, 1f);
    public bool fadeOut = true;

    [Header("Direction")]
    public Vector3 emissionDirection = Vector3.up;

    private class Particle
    {
        public Vector3 position;
        public Vector3 velocity;
        public float age;
        public float lifetime;
        public float size;
        public Color color;
    }

    private readonly List<Particle> particles = new List<Particle>();
    private float emissionAccumulator;
    private bool useColorB;
    private Mesh mesh;
    private MeshFilter meshFilter;
    private MeshRenderer meshRenderer;
    private Material runtimeMaterial;

    // Reused mesh buffers. Each particle is rendered as one camera-facing quad.
    private Vector3[] vertices;
    private Vector2[] uvs;
    private Color[] colors;
    private int[] triangles;
    private Camera renderCamera;

    private void Awake()
    {
        meshFilter = GetComponent<MeshFilter>();
        meshRenderer = GetComponent<MeshRenderer>();

        mesh = new Mesh();
        mesh.name = "Custom Particle Mesh";
        mesh.MarkDynamic();
        meshFilter.sharedMesh = mesh;

        Shader shader = Shader.Find("Universal Render Pipeline/Unlit");
        if (shader == null) shader = Shader.Find("Unlit/Color");
        if (shader == null) shader = Shader.Find("Sprites/Default");

        if (shader != null)
        {
            runtimeMaterial = new Material(shader);
            runtimeMaterial.color = Color.white;
            meshRenderer.sharedMaterial = runtimeMaterial;
        }

        AllocateBuffers();
        renderCamera = Camera.main;
    }

    private void Update()
    {
        HandleKeyboard();
        SimulateParticles(Time.deltaTime);
        EmitParticles(Time.deltaTime);
        UpdateMesh();
    }

    private void HandleKeyboard()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow)) emissionDirection = Vector3.up;
        if (Input.GetKeyDown(KeyCode.DownArrow)) emissionDirection = Vector3.down;
        if (Input.GetKeyDown(KeyCode.LeftArrow)) emissionDirection = Vector3.left;
        if (Input.GetKeyDown(KeyCode.RightArrow)) emissionDirection = Vector3.right;

        if (Input.GetKeyDown(KeyCode.Space))
        {
            useColorB = !useColorB;
            Color newColor = useColorB ? colorB : colorA;
            // Change the color of existing particles as well as newly emitted ones.
            for (int i = 0; i < particles.Count; i++)
                particles[i].color = newColor;
        }

        if (Input.GetKeyDown(KeyCode.Alpha1) || Input.GetKeyDown(KeyCode.Keypad1))
            particleSize = Mathf.Max(0.02f, particleSize - 0.02f);
        if (Input.GetKeyDown(KeyCode.Alpha2) || Input.GetKeyDown(KeyCode.Keypad2))
            particleSize = Mathf.Min(1f, particleSize + 0.02f);

        if (Input.GetKeyDown(KeyCode.Alpha3) || Input.GetKeyDown(KeyCode.Keypad3))
            initialSpeed = Mathf.Max(0.1f, initialSpeed - 0.5f);
        if (Input.GetKeyDown(KeyCode.Alpha4) || Input.GetKeyDown(KeyCode.Keypad4))
            initialSpeed = Mathf.Min(20f, initialSpeed + 0.5f);
    }

    private void EmitParticles(float dt)
    {
        if (emissionRate <= 0f || maxParticles <= 0) return;

        // Accumulate fractional particles to keep emission rate independent of frame rate.
        emissionAccumulator += emissionRate * dt;
        int amount = Mathf.FloorToInt(emissionAccumulator);
        emissionAccumulator -= amount;

        for (int i = 0; i < amount && particles.Count < maxParticles; i++)
            SpawnParticle();
    }

    private void SpawnParticle()
    {
        Vector3 axis = emissionDirection.sqrMagnitude > 0.0001f
            ? emissionDirection.normalized : Vector3.up;

        // Random direction inside a cone around the selected axis.
        Vector3 direction = RandomDirectionInCone(axis, emissionConeAngle);
        float speed = Mathf.Max(0f, initialSpeed + Random.Range(-speedRandomness, speedRandomness));

        Particle p = new Particle();
        p.position = transform.position;
        p.velocity = direction * speed;
        p.age = 0f;
        p.lifetime = Mathf.Max(0.01f, particleLifetime * Random.Range(0.8f, 1.2f));
        p.size = Mathf.Max(0.001f, particleSize + Random.Range(-sizeRandomness, sizeRandomness));
        p.color = useColorB ? colorB : colorA;
        particles.Add(p);
    }

    private Vector3 RandomDirectionInCone(Vector3 axis, float angleDegrees)
    {
        if (angleDegrees <= 0.001f) return axis;

        float cosMax = Mathf.Cos(angleDegrees * Mathf.Deg2Rad);
        float cosTheta = Random.Range(cosMax, 1f);
        float sinTheta = Mathf.Sqrt(Mathf.Max(0f, 1f - cosTheta * cosTheta));
        float phi = Random.Range(0f, Mathf.PI * 2f);

        Vector3 helper = Mathf.Abs(Vector3.Dot(axis, Vector3.up)) > 0.99f
            ? Vector3.right : Vector3.up;
        Vector3 tangent = Vector3.Cross(helper, axis).normalized;
        Vector3 bitangent = Vector3.Cross(axis, tangent).normalized;

        return (axis * cosTheta
              + tangent * (Mathf.Cos(phi) * sinTheta)
              + bitangent * (Mathf.Sin(phi) * sinTheta)).normalized;
    }

    private void SimulateParticles(float dt)
    {
        // Backward iteration allows expired particles to be removed safely.
        for (int i = particles.Count - 1; i >= 0; i--)
        {
            Particle p = particles[i];
            p.age += dt;

            if (p.age >= p.lifetime)
            {
                particles.RemoveAt(i);
                continue;
            }

            // Simple particle motion model:
            // acceleration = gravity - airResistance * velocity
            Vector3 acceleration = gravity - airResistance * p.velocity;
            p.velocity += acceleration * dt;
            p.position += p.velocity * dt;
        }
    }

    private void AllocateBuffers()
    {
        int vertexCapacity = Mathf.Max(4, maxParticles * 4);
        vertices = new Vector3[vertexCapacity];
        uvs = new Vector2[vertexCapacity];
        colors = new Color[vertexCapacity];
        triangles = new int[Mathf.Max(6, maxParticles * 6)];

        for (int i = 0; i < maxParticles; i++)
        {
            int v = i * 4;
            int t = i * 6;
            uvs[v] = new Vector2(0, 0);
            uvs[v + 1] = new Vector2(1, 0);
            uvs[v + 2] = new Vector2(1, 1);
            uvs[v + 3] = new Vector2(0, 1);

            triangles[t] = v;
            triangles[t + 1] = v + 2;
            triangles[t + 2] = v + 1;
            triangles[t + 3] = v;
            triangles[t + 4] = v + 3;
            triangles[t + 5] = v + 2;
        }
    }

    private void UpdateMesh()
    {
        if (renderCamera == null) renderCamera = Camera.main;
        if (renderCamera == null || mesh == null) return;

        int count = Mathf.Min(particles.Count, maxParticles);
        int vertexCount = count * 4;
        Vector3 cameraRight = renderCamera.transform.right;
        Vector3 cameraUp = renderCamera.transform.up;

        for (int i = 0; i < count; i++)
        {
            Particle p = particles[i];
            int v = i * 4;

            float half = p.size * 0.5f;
            Vector3 right = cameraRight * half;
            Vector3 up = cameraUp * half;

            // Mesh vertices are local to this emitter.
            Vector3 center = transform.InverseTransformPoint(p.position);
            vertices[v] = center - right - up;
            vertices[v + 1] = center + right - up;
            vertices[v + 2] = center + right + up;
            vertices[v + 3] = center - right + up;

            float alpha = fadeOut ? Mathf.Clamp01(1f - p.age / p.lifetime) : 1f;
            Color c = p.color;
            c.a *= alpha;
            colors[v] = colors[v + 1] = colors[v + 2] = colors[v + 3] = c;
        }

        mesh.Clear();
        if (vertexCount == 0) return;

        mesh.vertices = CopyPrefix(vertices, vertexCount);
        mesh.uv = CopyPrefix(uvs, vertexCount);
        mesh.colors = CopyPrefix(colors, vertexCount);
        mesh.triangles = CopyPrefix(triangles, count * 6);
        mesh.RecalculateBounds();
    }

    private static T[] CopyPrefix<T>(T[] source, int count)
    {
        T[] result = new T[count];
        System.Array.Copy(source, result, count);
        return result;
    }

    private void OnGUI()
    {
        GUILayout.BeginArea(new Rect(12, 12, 300, 245), GUI.skin.box);
        GUILayout.Label("自编程粒子系统（非 Unity Particle System）");
        GUILayout.Label("粒子数量：" + particles.Count + " / " + maxParticles);
        GUILayout.Label("发射方向：" + DirectionName());
        GUILayout.Label("粒子大小：" + particleSize.ToString("F2"));
        GUILayout.Label("初速度：" + initialSpeed.ToString("F1"));
        GUILayout.Space(5);
        GUILayout.Label("方向：↑ ↓ ← →");
        GUILayout.Label("颜色：空格切换");
        GUILayout.Label("大小：1 缩小 / 2 放大");
        GUILayout.Label("速度：3 减速 / 4 加速");
        GUILayout.EndArea();
    }

    private string DirectionName()
    {
        if (emissionDirection == Vector3.up) return "上";
        if (emissionDirection == Vector3.down) return "下";
        if (emissionDirection == Vector3.left) return "左";
        return "右";
    }

    private void OnDestroy()
    {
        if (mesh != null)
        {
            if (Application.isPlaying) Destroy(mesh);
            else DestroyImmediate(mesh);
        }
        if (runtimeMaterial != null)
        {
            if (Application.isPlaying) Destroy(runtimeMaterial);
            else DestroyImmediate(runtimeMaterial);
        }
    }
}
