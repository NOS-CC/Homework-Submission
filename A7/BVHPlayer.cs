using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using UnityEngine;

/// <summary>
/// BVH runtime importer/player.
/// Place the BVH file in Assets/StreamingAssets, or assign it as a TextAsset.
/// Attach this component to an empty GameObject.
/// </summary>
public class BVHPlayer : MonoBehaviour
{
    [Header("BVH Source")]
    public TextAsset bvhTextAsset;
    public string streamingAssetsFileName = "motion.bvh";

    [Header("Playback")]
    [Min(0.01f)] public float playbackSpeed = 1f;
    public bool playOnStart = true;
    public bool loop = true;

    [Header("Visualization")]
    public bool createBoneVisuals = true;
    [Min(0.001f)] public float boneRadius = 0.025f;
    public Color boneColor = new Color(0.25f, 0.75f, 1f);

    private class Joint
    {
        public string name;
        public Vector3 offset;
        public readonly List<string> channels = new List<string>();
        public readonly List<Joint> children = new List<Joint>();
        public Transform tr;
        public int channelStart;
        public bool endSite;
    }

    private Joint root;
    private readonly List<Joint> allJoints = new List<Joint>();
    private float[][] frames;
    private int frameCount;
    private int channelCount;
    private float frameTime = 1f / 30f;
    private float elapsed;
    private bool playing;
    private bool ready;
    private Material visualMaterial;

    public bool IsPlaying => playing;
    public int FrameCount => frameCount;
    public float Duration => frameCount * frameTime;

    private void Start()
    {
        try
        {
            string source = bvhTextAsset != null
                ? bvhTextAsset.text
                : File.ReadAllText(Path.Combine(Application.streamingAssetsPath, streamingAssetsFileName));

            Parse(source);
            BuildHierarchy();
            ApplyFrame(0);
            ready = true;
            playing = playOnStart;
        }
        catch (Exception ex)
        {
            Debug.LogError("[BVHPlayer] " + ex.Message, this);
        }
    }

    private void Update()
    {
        if (!ready || !playing || frameCount == 0) return;

        elapsed += Time.deltaTime * Mathf.Max(0.01f, playbackSpeed);
        if (loop)
            elapsed %= Duration;
        else if (elapsed >= Duration)
        {
            elapsed = Mathf.Max(0f, Duration - frameTime);
            playing = false;
        }

        float framePos = elapsed / frameTime;
        int a = Mathf.Clamp(Mathf.FloorToInt(framePos), 0, frameCount - 1);
        int b = loop ? (a + 1) % frameCount : Mathf.Min(a + 1, frameCount - 1);
        float t = framePos - Mathf.Floor(framePos);

        float[] values = new float[channelCount];
        for (int i = 0; i < channelCount; i++)
        {
            string channel = channelNames[i];
            if (channel.EndsWith("rotation", StringComparison.OrdinalIgnoreCase))
                values[i] = frames[a][i] + Mathf.DeltaAngle(frames[a][i], frames[b][i]) * t;
            else
                values[i] = Mathf.Lerp(frames[a][i], frames[b][i], t);
        }
        ApplyJoint(root, values);
    }

    private string[] channelNames;

    public void Play() { if (ready) playing = true; }
    public void Pause() { playing = false; }
    public void TogglePlayPause() { if (playing) Pause(); else Play(); }

    public void Stop()
    {
        playing = false;
        elapsed = 0f;
        if (ready) ApplyFrame(0);
    }

    public void SetPlaybackSpeed(float value)
    {
        playbackSpeed = Mathf.Max(0.01f, value);
    }

    public void SetNormalizedTime(float value)
    {
        if (!ready) return;
        elapsed = Mathf.Clamp01(value) * Mathf.Max(0f, Duration - 0.0001f);
        ApplyFrame(Mathf.Clamp(Mathf.FloorToInt(elapsed / frameTime), 0, frameCount - 1));
    }

    private void Parse(string text)
    {
        string[] tok = Regex.Split(text, @"\s+");
        int p = 0;

        string Next()
        {
            while (p < tok.Length && tok[p].Length == 0) p++;
            if (p >= tok.Length) throw new Exception("BVH 文件意外结束。");
            return tok[p++].Trim();
        }

        void Expect(string value)
        {
            string got = Next().TrimEnd(':');
            if (!got.Equals(value, StringComparison.OrdinalIgnoreCase))
                throw new Exception("预期 " + value + "，实际读到 " + got);
        }

        float Number()
        {
            if (!float.TryParse(Next(), NumberStyles.Float, CultureInfo.InvariantCulture, out float v))
                throw new Exception("BVH 中存在无法解析的数字。");
            return v;
        }

        Expect("HIERARCHY");
        Expect("ROOT");
        root = ReadJoint("ROOT", Next, Number, Expect);

        Expect("MOTION");
        Expect("Frames");
        frameCount = int.Parse(Next(), CultureInfo.InvariantCulture);
        Expect("Frame");
        Expect("Time");
        frameTime = Number();
        if (frameTime <= 0f) frameTime = 1f / 30f;

        channelNames = new string[channelCount];
        FillChannelNames(root, channelNames);

        frames = new float[frameCount][];
        for (int f = 0; f < frameCount; f++)
        {
            frames[f] = new float[channelCount];
            for (int c = 0; c < channelCount; c++)
                frames[f][c] = Number();
        }
    }

    private Joint ReadJoint(string type, Func<string> next, Func<float> number, Action<string> expect)
    {
        Joint j = new Joint();
        j.name = type == "ROOT" || type == "JOINT" ? next() : "End Site";
        j.endSite = type == "END";
        allJoints.Add(j);
        expect("{");

        while (true)
        {
            string key = next().TrimEnd(':');
            if (key == "}") break;

            if (key.Equals("OFFSET", StringComparison.OrdinalIgnoreCase))
                j.offset = new Vector3(number(), number(), number());
            else if (key.Equals("CHANNELS", StringComparison.OrdinalIgnoreCase))
            {
                int n = int.Parse(next(), CultureInfo.InvariantCulture);
                j.channelStart = channelCount;
                for (int i = 0; i < n; i++)
                {
                    string ch = next();
                    j.channels.Add(ch);
                }
                channelCount += n;
            }
            else if (key.Equals("JOINT", StringComparison.OrdinalIgnoreCase))
                j.children.Add(ReadJoint("JOINT", next, number, expect));
            else if (key.Equals("End", StringComparison.OrdinalIgnoreCase))
            {
                string site = next();
                if (!site.Equals("Site", StringComparison.OrdinalIgnoreCase))
                    throw new Exception("无法识别 End Site。");
                j.children.Add(ReadJoint("END", next, number, expect));
            }
            else
                throw new Exception("无法识别层级字段：" + key);
        }
        return j;
    }

    private void FillChannelNames(Joint j, string[] output)
    {
        foreach (string channel in j.channels)
            output[j.channelStart++] = channel;
        // Restore the channel start after using it as a temporary cursor.
        j.channelStart -= j.channels.Count;
        foreach (Joint child in j.children)
            FillChannelNames(child, output);
    }

    private void BuildHierarchy()
    {
        for (int i = transform.childCount - 1; i >= 0; i--)
            Destroy(transform.GetChild(i).gameObject);

        GameObject rootObject = new GameObject(root.name);
        rootObject.transform.SetParent(transform, false);
        root.tr = rootObject.transform;
        CreateTransforms(root);

        if (!createBoneVisuals) return;

        Shader shader = Shader.Find("Universal Render Pipeline/Lit");
        if (shader == null) shader = Shader.Find("Standard");
        if (shader == null) return;

        visualMaterial = new Material(shader);
        visualMaterial.color = boneColor;
        CreateVisuals(root);
    }

    private void CreateTransforms(Joint j)
    {
        foreach (Joint child in j.children)
        {
            GameObject go = new GameObject(child.name);
            go.transform.SetParent(j.tr, false);
            go.transform.localPosition = child.offset;
            child.tr = go.transform;
            CreateTransforms(child);
        }
    }

    private void CreateVisuals(Joint j)
    {
        foreach (Joint child in j.children)
        {
            if (!child.endSite)
            {
                GameObject ball = GameObject.CreatePrimitive(PrimitiveType.Sphere);
                ball.name = child.name + "_Joint";
                ball.transform.SetParent(child.tr, false);
                ball.transform.localScale = Vector3.one * boneRadius * 2f;
                SetVisual(ball);
            }

            GameObject bone = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            bone.name = j.name + "_to_" + child.name;
            BoneVisual updater = bone.AddComponent<BoneVisual>();
            updater.Setup(j.tr, child.tr, boneRadius);
            SetVisual(bone);
            CreateVisuals(child);
        }
    }

    private void SetVisual(GameObject go)
    {
        Renderer r = go.GetComponent<Renderer>();
        if (r != null) r.sharedMaterial = visualMaterial;
        Collider c = go.GetComponent<Collider>();
        if (c != null) Destroy(c);
    }

    private void ApplyFrame(int index)
    {
        if (frames == null || frameCount == 0) return;
        ApplyJoint(root, frames[Mathf.Clamp(index, 0, frameCount - 1)]);
    }

    private void ApplyJoint(Joint j, float[] values)
    {
        Vector3 pos = j.offset;
        Quaternion rot = Quaternion.identity;

        for (int i = 0; i < j.channels.Count; i++)
        {
            string ch = j.channels[i];
            float v = values[j.channelStart + i];

            if (ch.Equals("Xposition", StringComparison.OrdinalIgnoreCase)) pos.x = j.offset.x + v;
            else if (ch.Equals("Yposition", StringComparison.OrdinalIgnoreCase)) pos.y = j.offset.y + v;
            else if (ch.Equals("Zposition", StringComparison.OrdinalIgnoreCase)) pos.z = j.offset.z + v;
            else if (ch.Equals("Xrotation", StringComparison.OrdinalIgnoreCase)) rot *= Quaternion.AngleAxis(v, Vector3.right);
            else if (ch.Equals("Yrotation", StringComparison.OrdinalIgnoreCase)) rot *= Quaternion.AngleAxis(v, Vector3.up);
            else if (ch.Equals("Zrotation", StringComparison.OrdinalIgnoreCase)) rot *= Quaternion.AngleAxis(v, Vector3.forward);
        }

        j.tr.localPosition = pos;
        j.tr.localRotation = rot;
        foreach (Joint child in j.children)
            ApplyJoint(child, values);
    }

    private void OnGUI()
    {
        if (!ready) return;
        GUILayout.BeginArea(new Rect(12, 12, 250, 175), GUI.skin.box);
        GUILayout.Label("BVH 动作播放器");
        GUILayout.Label("帧数：" + frameCount + "    时长：" + Duration.ToString("F2") + "s");
        GUILayout.BeginHorizontal();
        if (GUILayout.Button(playing ? "暂停" : "播放")) TogglePlayPause();
        if (GUILayout.Button("停止")) Stop();
        GUILayout.EndHorizontal();

        GUILayout.Label("播放速度：" + playbackSpeed.ToString("F2") + "x");
        playbackSpeed = GUILayout.HorizontalSlider(playbackSpeed, 0.1f, 3f);
        GUILayout.Label("播放进度");
        float normalized = Duration > 0f ? elapsed / Duration : 0f;
        float next = GUILayout.HorizontalSlider(normalized, 0f, 1f);
        if (Mathf.Abs(next - normalized) > 0.003f) SetNormalizedTime(next);
        GUILayout.EndArea();
    }

    private void OnDestroy()
    {
        if (visualMaterial != null) Destroy(visualMaterial);
    }
}

public class BoneVisual : MonoBehaviour
{
    private Transform a;
    private Transform b;
    private float radius;

    public void Setup(Transform start, Transform end, float r)
    {
        a = start;
        b = end;
        radius = r;
    }

    private void LateUpdate()
    {
        if (a == null || b == null) return;
        Vector3 start = a.position;
        Vector3 delta = b.position - start;
        float length = delta.magnitude;
        transform.position = start + delta * 0.5f;
        if (length > 0.00001f)
            transform.rotation = Quaternion.FromToRotation(Vector3.up, delta / length);
        transform.localScale = new Vector3(radius * 2f, length * 0.5f, radius * 2f);
    }
}
