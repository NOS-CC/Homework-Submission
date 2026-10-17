#pragma once
#include "Math3D.h"
#include <string>
#include <vector>

struct Bounds {
    Vec3 min, max, center;
    float maxExtent = 1.0f;
};

class ObjMesh {
public:
    std::vector<Vec3> originalVertices;
    std::vector<Vec3> deformedVertices;
    std::vector<unsigned int> indices;
    std::vector<Vec3> normals;
    Bounds bounds;
    bool load(const std::string& path, std::string& error);
    void resetDeformation();
    void recalculateNormals();
    bool empty() const { return originalVertices.empty() || indices.empty(); }
private:
    void calculateBounds();
};
