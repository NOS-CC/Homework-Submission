using UnityEngine;

/// <summary>Verlet rope with distance constraints. Attach to an empty GameObject.</summary>
[RequireComponent(typeof(LineRenderer))]
public class SimpleRopeSimulation : MonoBehaviour
{
    [Header("Rope Shape")]
    [Min(2)] public int segmentCount = 24;
    [Min(0.1f)] public float ropeLength = 4f;
    public Vector3 initialDirection = Vector3.down;
    public float lineWidth = 0.06f;

    [Header("Physics")]
    public Vector3 gravity = new Vector3(0f, -9.81f, 0f);
    [Range(1, 30)] public int constraintIterations = 12;
    [Range(0f, 1f)] public float damping = 0.015f;
    public float floorHeight = 0f;
    public bool collideWithFloor = true;

    [Header("Initial Motion")]
    public Vector3 initialVelocity = new Vector3(1.5f, 0f, 0f);

    private Vector3[] points;
    private Vector3[] previousPoints;
    private LineRenderer line;
    private int builtSegmentCount = -1;

    private void Start()
    {
        line = GetComponent<LineRenderer>();
        line.useWorldSpace = true;
        line.numCapVertices = 4;
        line.numCornerVertices = 4;
        RebuildRope();
    }

    private void OnValidate()
    {
        segmentCount = Mathf.Max(2, segmentCount);
        ropeLength = Mathf.Max(0.1f, ropeLength);
        lineWidth = Mathf.Max(0.005f, lineWidth);
        if (line != null) { line.startWidth = lineWidth; line.endWidth = lineWidth; }
    }

    private void RebuildRope()
    {
        int count = segmentCount + 1;
        points = new Vector3[count];
        previousPoints = new Vector3[count];
        Vector3 dir = initialDirection.sqrMagnitude < 0.0001f ? Vector3.down : initialDirection.normalized;
        for (int i = 0; i < count; i++)
        {
            Vector3 p = transform.position + dir * (ropeLength * i / segmentCount);
            points[i] = p;
            previousPoints[i] = p - initialVelocity * Time.fixedDeltaTime;
        }
        points[0] = transform.position;
        previousPoints[0] = transform.position;
        line.positionCount = count;
        builtSegmentCount = segmentCount;
        DrawRope();
    }

    private void FixedUpdate()
    {
        if (points == null || builtSegmentCount != segmentCount) RebuildRope();
        float dt = Time.fixedDeltaTime;
        Vector3 anchor = transform.position;
        for (int i = 1; i < points.Length; i++)
        {
            Vector3 current = points[i];
            Vector3 velocity = (points[i] - previousPoints[i]) * (1f - damping);
            previousPoints[i] = current;
            points[i] = current + velocity + gravity * (dt * dt);
        }

        float restLength = ropeLength / segmentCount;
        for (int iteration = 0; iteration < constraintIterations; iteration++)
        {
            points[0] = anchor;
            for (int i = 0; i < points.Length - 1; i++)
            {
                Vector3 delta = points[i + 1] - points[i];
                float distance = delta.magnitude;
                if (distance < 0.000001f) continue;
                float error = (distance - restLength) / distance;
                if (i == 0) points[i + 1] -= delta * error;
                else
                {
                    Vector3 correction = delta * (0.5f * error);
                    points[i] += correction;
                    points[i + 1] -= correction;
                }
            }
            points[0] = anchor;
            if (collideWithFloor)
                for (int i = 1; i < points.Length; i++)
                    if (points[i].y < floorHeight) points[i].y = floorHeight;
        }
        DrawRope();
    }

    private void DrawRope()
    {
        if (line == null || points == null) return;
        line.startWidth = lineWidth;
        line.endWidth = lineWidth;
        line.positionCount = points.Length;
        line.SetPositions(points);
    }

    private void OnDrawGizmos()
    {
        Gizmos.color = Color.yellow;
        Gizmos.DrawSphere(transform.position, 0.09f);
    }
}
