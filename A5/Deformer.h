#pragma once
#include "ObjMesh.h"
#include <vector>

enum class DeformMode { None=0, Taper, Twist, Bend, FFD };

struct FFDGrid {
    int n=4;
    Bounds bounds;
    std::vector<Vec3> originalPoints;
    std::vector<Vec3> points;
    void build(const Bounds& b, int resolution);
    int index(int i,int j,int k) const { return i+n*(j+n*k); }
    void reset();
};

class Deformer {
public:
    static void apply(ObjMesh& mesh, DeformMode mode, int axis,
                      float strength, float angleDegrees, FFDGrid& ffd);
private:
    static Vec3 applyTaper(const Vec3& p,const Bounds& b,int axis,float amount);
    static Vec3 applyTwist(const Vec3& p,const Bounds& b,int axis,float angleDegrees);
    static Vec3 applyBend(const Vec3& p,const Bounds& b,int axis,float angleDegrees);
    static Vec3 applyFFD(const Vec3& p,const FFDGrid& grid);
};
