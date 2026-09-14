using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using UnityEngine;

public class ObjParser
{
    public List<Vector3> vertices = new List<Vector3>();
    public List<Vector3> normals = new List<Vector3>();
    public List<Vector2> uvs = new List<Vector2>();
    public List<int> triangles = new List<int>();   // 三角形索引（指向最终顶点列表）
    public List<Vector3> finalVertices = new List<Vector3>(); // 重组后的顶点
    public List<Vector3> finalNormals = new List<Vector3>();
    public List<Vector2> finalUVs = new List<Vector2>();
    public bool hasNormals = false;

    public void Parse(string filePath)
    {
        vertices.Clear(); normals.Clear(); uvs.Clear();
        triangles.Clear(); finalVertices.Clear(); finalNormals.Clear(); finalUVs.Clear();

        string[] lines = File.ReadAllLines(filePath);

        foreach (string line in lines)
        {
            string trimmed = line.Trim();
            if (trimmed.Length == 0 || trimmed.StartsWith("#")) continue;

            string[] parts = trimmed.Split(new char[] { ' ' }, System.StringSplitOptions.RemoveEmptyEntries);

            switch (parts[0])
            {
                case "v":
                    vertices.Add(new Vector3(
                        float.Parse(parts[1], CultureInfo.InvariantCulture),
                        float.Parse(parts[2], CultureInfo.InvariantCulture),
                        float.Parse(parts[3], CultureInfo.InvariantCulture)));
                    break;
                case "vn":
                    normals.Add(new Vector3(
                        float.Parse(parts[1], CultureInfo.InvariantCulture),
                        float.Parse(parts[2], CultureInfo.InvariantCulture),
                        float.Parse(parts[3], CultureInfo.InvariantCulture)));
                    hasNormals = true;
                    break;
                case "vt":
                    uvs.Add(new Vector2(
                        float.Parse(parts[1], CultureInfo.InvariantCulture),
                        float.Parse(parts[2], CultureInfo.InvariantCulture)));
                    break;
                case "f":
                    // 面的顶点数可能是3或4，统一拆成三角形
                    List<int> faceIndices = new List<int>();
                    for (int i = 1; i < parts.Length; i++)
                    {
                        string[] sub = parts[i].Split('/');
                        int vIdx = int.Parse(sub[0]) - 1; // OBJ索引从1开始

                        Vector3 pos = vertices[vIdx];
                        Vector3 nor = (sub.Length > 2 && sub[2] != "") ? normals[int.Parse(sub[2]) - 1] : Vector3.up;
                        Vector2 uv = (sub.Length > 1 && sub[1] != "") ? uvs[int.Parse(sub[1]) - 1] : Vector2.zero;

                        finalVertices.Add(pos);
                        finalNormals.Add(nor);
                        finalUVs.Add(uv);
                        faceIndices.Add(finalVertices.Count - 1);
                    }

                    // 扇形三角化：0-1-2, 0-2-3, ...
                    for (int i = 1; i < faceIndices.Count - 1; i++)
                    {
                        triangles.Add(faceIndices[0]);
                        triangles.Add(faceIndices[i]);
                        triangles.Add(faceIndices[i + 1]);
                    }
                    break;
            }
        }
    }
}