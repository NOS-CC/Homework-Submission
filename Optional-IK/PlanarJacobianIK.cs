using UnityEngine;

/// <summary>
/// Planar multi-joint inverse kinematics using the damped least-squares
/// inverse-Jacobian method. Attach to an empty GameObject.
/// The chain is generated in the XY plane; the root remains fixed.
/// </summary>
public class PlanarJacobianIK : MonoBehaviour
{
    [Header("Chain")]
    [Range(2, 10)] public int jointCount = 4;
    [Min(0.1f)] public float linkLength = 1.2f;
    [Min(0.01f)] public float linkRadius = 0.07f;
    [Min(0.02f)] public float jointRadius = 0.15f;

    [Header("IK Solver")]
    [Range(1, 30)] public int iterationsPerFrame = 10;
    [Range(0.001f, 1f)] public float damping = 0.12f;
    [Range(0.01f, 1f)] public float stepScale = 0.65f;
    [Range(1f, 45f)] public float maxAngleStepDegrees = 12f;
    [Min(0.0001f)] public float positionTolerance = 0.015f;

    [Header("Target")]
    public Vector2 targetPosition = new Vector2(2.5f, 1.5f);
    public float targetMarkerRadius = 0.18f;

    [Header("Optional Materials")]
    public Material linkMaterial;
    public Material jointMaterial;
    public Material rootMaterial;
    public Material targetMaterial;

    private float[] angles; // Relative joint angles, radians
    private Transform[] links;
    private Transform[] joints;
    private Transform targetMarker;
    private Vector2[] jointPositions;
    private Vector2 endPosition;
    private Camera sceneCamera;
    private bool draggingTarget;
    private int builtJointCount = -1;

    private void Start()
    {
        sceneCamera = Camera.main;
        RebuildChain();
    }

    private void Update()
    {
        if (builtJointCount != jointCount)
            RebuildChain();

        HandleMouseInput();
        if (angles == null || angles.Length == 0) return;

        SolveIK();
        UpdateVisuals();
    }

    public void RebuildChain()
    {
        ClearGeneratedObjects();

        jointCount = Mathf.Clamp(jointCount, 2, 10);
        builtJointCount = jointCount;
        angles = new float[jointCount];
        links = new Transform[jointCount];
        joints = new Transform[jointCount + 1];
        jointPositions = new Vector2[jointCount + 1];

        joints[0] = CreateSphere("Root_Joint", jointRadius * 1.15f,
            rootMaterial, new Color(0.95f, 0.25f, 0.2f));

        for (int i = 0; i < jointCount; i++)
        {
            links[i] = CreateCylinder("Link_" + (i + 1), linkMaterial,
                new Color(0.18f, 0.55f, 0.9f));
            joints[i + 1] = CreateSphere("Joint_" + (i + 1), jointRadius,
                jointMaterial, new Color(0.95f, 0.7f, 0.2f));
            angles[i] = (i % 2 == 0 ? 0.12f : -0.12f);
        }

        targetMarker = CreateSphere("IK_Target", targetMarkerRadius,
            targetMaterial, new Color(0.2f, 1f, 0.35f));

        Vector2 root = new Vector2(transform.position.x, transform.position.y);
        targetPosition = root + new Vector2(linkLength * jointCount * 0.65f,
                                             linkLength * jointCount * 0.25f);
        UpdateVisuals();
    }

    private Transform CreateSphere(string objectName, float radius,
        Material supplied, Color fallback)
    {
        GameObject go = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        go.name = objectName;
        go.transform.SetParent(transform, true);
        go.transform.localScale = Vector3.one * radius * 2f;
        SetMaterial(go, supplied, fallback);
        Collider c = go.GetComponent<Collider>();
        if (c != null) Destroy(c);
        return go.transform;
    }

    private Transform CreateCylinder(string objectName, Material supplied, Color fallback)
    {
        GameObject go = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
        go.name = objectName;
        go.transform.SetParent(transform, true);
        SetMaterial(go, supplied, fallback);
        Collider c = go.GetComponent<Collider>();
        if (c != null) Destroy(c);
        return go.transform;
    }

    private void SetMaterial(GameObject go, Material supplied, Color fallback)
    {
        Renderer r = go.GetComponent<Renderer>();
        if (r == null) return;

        if (supplied != null)
        {
            r.material = supplied;
            return;
        }

        Shader shader = Shader.Find("Universal Render Pipeline/Lit");
        if (shader == null) shader = Shader.Find("Standard");
        if (shader == null) return;

        Material m = new Material(shader);
        if (m.HasProperty("_BaseColor")) m.SetColor("_BaseColor", fallback);
        if (m.HasProperty("_Color")) m.SetColor("_Color", fallback);
        r.material = m;
    }

    private void ClearGeneratedObjects()
    {
        for (int i = transform.childCount - 1; i >= 0; i--)
        {
            GameObject child = transform.GetChild(i).gameObject;
            if (Application.isPlaying) Destroy(child);
            else DestroyImmediate(child);
        }
    }

    private void ForwardKinematics()
    {
        Vector2 p = new Vector2(transform.position.x, transform.position.y);
        float cumulative = 0f;
        jointPositions[0] = p;

        for (int i = 0; i < jointCount; i++)
        {
            cumulative += angles[i];
            p += new Vector2(Mathf.Cos(cumulative), Mathf.Sin(cumulative)) * linkLength;
            jointPositions[i + 1] = p;
        }
        endPosition = p;
    }

    /// <summary>
    /// deltaTheta = J^T * (J*J^T + lambda^2 I)^-1 * (target - end).
    /// Since the end-effector is 2D, the matrix inversion is 2x2.
    /// </summary>
    private void SolveIK()
    {
        float lambda2 = damping * damping;

        for (int iteration = 0; iteration < iterationsPerFrame; iteration++)
        {
            ForwardKinematics();
            Vector2 error = targetPosition - endPosition;
            if (error.sqrMagnitude <= positionTolerance * positionTolerance)
                break;

            float a00 = lambda2;
            float a01 = 0f;
            float a11 = lambda2;

            // Build A = J * J^T + lambda^2 I.
            for (int i = 0; i < jointCount; i++)
            {
                Vector2 r = endPosition - jointPositions[i];
                Vector2 col = new Vector2(-r.y, r.x);
                a00 += col.x * col.x;
                a01 += col.x * col.y;
                a11 += col.y * col.y;
            }

            float det = a00 * a11 - a01 * a01;
            if (Mathf.Abs(det) < 1e-10f) break;

            // y = inverse(A) * error.
            Vector2 y = new Vector2(
                (a11 * error.x - a01 * error.y) / det,
                (-a01 * error.x + a00 * error.y) / det);

            // deltaTheta = J^T * y.
            for (int i = 0; i < jointCount; i++)
            {
                Vector2 r = endPosition - jointPositions[i];
                Vector2 jacobianColumn = new Vector2(-r.y, r.x);
                float delta = Vector2.Dot(jacobianColumn, y) * stepScale;
                float maxStep = maxAngleStepDegrees * Mathf.Deg2Rad;
                angles[i] += Mathf.Clamp(delta, -maxStep, maxStep);
            }
        }
        ForwardKinematics();
    }

    private void UpdateVisuals()
    {
        if (angles == null || joints == null || links == null) return;
        ForwardKinematics();

        for (int i = 0; i < jointCount; i++)
        {
            Vector2 a = jointPositions[i];
            Vector2 b = jointPositions[i + 1];
            Vector2 d = b - a;
            Vector2 mid = (a + b) * 0.5f;

            links[i].position = new Vector3(mid.x, mid.y, 0f);
            links[i].rotation = Quaternion.FromToRotation(Vector3.up,
                new Vector3(d.x, d.y, 0f).normalized);
            // Unity cylinder's default height is 2 units.
            links[i].localScale = new Vector3(linkRadius * 2f,
                d.magnitude * 0.5f, linkRadius * 2f);
            joints[i].position = new Vector3(a.x, a.y, 0f);
        }

        joints[jointCount].position = new Vector3(endPosition.x, endPosition.y, 0f);
        if (targetMarker != null)
            targetMarker.position = new Vector3(targetPosition.x, targetPosition.y, -0.05f);
    }

    private void HandleMouseInput()
    {
        if (sceneCamera == null) sceneCamera = Camera.main;
        if (sceneCamera == null) return;

        // Ignore clicks in the top-left runtime control panel.
        Vector2 mouse = Input.mousePosition;
        bool overPanel = mouse.x < 325f && mouse.y > Screen.height - 165f;

        if (Input.GetMouseButtonDown(0) && !overPanel)
        {
            Ray ray = sceneCamera.ScreenPointToRay(Input.mousePosition);
            Plane plane = new Plane(Vector3.forward, Vector3.zero);
            if (plane.Raycast(ray, out float distance))
            {
                Vector3 hit = ray.GetPoint(distance);
                targetPosition = new Vector2(hit.x, hit.y);
                draggingTarget = true;
            }
        }

        if (draggingTarget && Input.GetMouseButton(0))
        {
            Ray ray = sceneCamera.ScreenPointToRay(Input.mousePosition);
            Plane plane = new Plane(Vector3.forward, Vector3.zero);
            if (plane.Raycast(ray, out float distance))
            {
                Vector3 hit = ray.GetPoint(distance);
                targetPosition = new Vector2(hit.x, hit.y);
            }
        }

        if (Input.GetMouseButtonUp(0))
            draggingTarget = false;
    }

    private void OnGUI()
    {
        GUILayout.BeginArea(new Rect(12, 12, 300, 145), GUI.skin.box);
        GUILayout.Label("Planar IK - Damped Least Squares");

        GUILayout.BeginHorizontal();
        GUILayout.Label("Joint count: " + jointCount, GUILayout.Width(105));
        int count = Mathf.RoundToInt(GUILayout.HorizontalSlider(jointCount, 2, 10));
        GUILayout.EndHorizontal();

        if (count != jointCount)
        {
            jointCount = count;
            RebuildChain();
        }

        GUILayout.Label("Drag in the Scene/Game view to move the target.");
        GUILayout.Label("End: " + endPosition.ToString("F2"));
        GUILayout.Label("Target: " + targetPosition.ToString("F2"));

        if (GUILayout.Button("Reset pose"))
        {
            for (int i = 0; i < angles.Length; i++) angles[i] = 0f;
        }
        GUILayout.EndArea();
    }
}
