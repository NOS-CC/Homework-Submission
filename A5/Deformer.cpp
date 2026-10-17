#include "Deformer.h"
#include <cmath>
#include <algorithm>

static float component(const Vec3& p,int axis){ return axis==0?p.x:(axis==1?p.y:p.z); }
static void setComponent(Vec3& p,int axis,float v){ if(axis==0)p.x=v;else if(axis==1)p.y=v;else p.z=v; }
static float clamp01(float x){ return std::max(0.0f,std::min(1.0f,x)); }

void FFDGrid::build(const Bounds& b,int resolution) {
    n=std::max(2,std::min(8,resolution)); bounds=b;
    originalPoints.clear(); points.clear();
    for(int k=0;k<n;k++) for(int j=0;j<n;j++) for(int i=0;i<n;i++) {
        float u=float(i)/(n-1),v=float(j)/(n-1),w=float(k)/(n-1);
        Vec3 p(b.min.x+(b.max.x-b.min.x)*u,
               b.min.y+(b.max.y-b.min.y)*v,
               b.min.z+(b.max.z-b.min.z)*w);
        originalPoints.push_back(p);
    }
    points=originalPoints;
}
void FFDGrid::reset(){ points=originalPoints; }

Vec3 Deformer::applyTaper(const Vec3& p,const Bounds& b,int axis,float amount) {
    float lo=component(b.min,axis), hi=component(b.max,axis);
    float t=clamp01((component(p,axis)-lo)/std::max(hi-lo,1e-6f));
    float factor=std::max(0.05f,1.0f+amount*t);
    Vec3 out=p;
    int a=(axis+1)%3, c=(axis+2)%3;
    setComponent(out,a,component(b.center,a)+(component(p,a)-component(b.center,a))*factor);
    setComponent(out,c,component(b.center,c)+(component(p,c)-component(b.center,c))*factor);
    return out;
}
Vec3 Deformer::applyTwist(const Vec3& p,const Bounds& b,int axis,float angleDegrees) {
    float lo=component(b.min,axis), hi=component(b.max,axis);
    float t=clamp01((component(p,axis)-lo)/std::max(hi-lo,1e-6f));
    float a=angleDegrees*3.1415926535f/180.0f*t;
    float cs=std::cos(a),sn=std::sin(a);
    int u=(axis+1)%3,v=(axis+2)%3;
    Vec3 out=p;
    float du=component(p,u)-component(b.center,u);
    float dv=component(p,v)-component(b.center,v);
    setComponent(out,u,component(b.center,u)+cs*du-sn*dv);
    setComponent(out,v,component(b.center,v)+sn*du+cs*dv);
    return out;
}
Vec3 Deformer::applyBend(const Vec3& p,const Bounds& b,int axis,float angleDegrees) {
    // Curves the longitudinal axis around a circular arc; cross-section is preserved.
    float lo=component(b.min,axis), hi=component(b.max,axis);
    float extent=std::max(hi-lo,1e-6f);
    float t=(component(p,axis)-lo)/extent;
    float total=angleDegrees*3.1415926535f/180.0f;
    if(std::abs(total)<1e-5f) return p;
    float radius=extent/total;
    float theta=t*total;
    int bendAxis=(axis+1)%3;
    Vec3 out=p;
    float offset=component(p,bendAxis)-component(b.center,bendAxis);
    setComponent(out,axis,lo+radius*std::sin(theta)-offset*std::sin(theta));
    setComponent(out,bendAxis,component(b.center,bendAxis)+radius*(1.0f-std::cos(theta))+offset*std::cos(theta));
    return out;
}
static float binomial(int n,int k) {
    if(k<0||k>n) return 0;
    float r=1; for(int i=1;i<=k;i++) r=r*(n-i+1)/i; return r;
}
static float bernstein(int i,int n,float t) {
    return binomial(n,i)*std::pow(t,i)*std::pow(1.0f-t,n-i);
}
Vec3 Deformer::applyFFD(const Vec3& p,const FFDGrid& g) {
    const int n=g.n;
    float dx=std::max(g.bounds.max.x-g.bounds.min.x,1e-6f);
    float dy=std::max(g.bounds.max.y-g.bounds.min.y,1e-6f);
    float dz=std::max(g.bounds.max.z-g.bounds.min.z,1e-6f);
    float u=clamp01((p.x-g.bounds.min.x)/dx);
    float v=clamp01((p.y-g.bounds.min.y)/dy);
    float w=clamp01((p.z-g.bounds.min.z)/dz);
    Vec3 result(0,0,0);
    for(int k=0;k<n;k++) for(int j=0;j<n;j++) for(int i=0;i<n;i++) {
        float weight=bernstein(i,n-1,u)*bernstein(j,n-1,v)*bernstein(k,n-1,w);
        result += g.points[g.index(i,j,k)]*weight;
    }
    return result;
}
void Deformer::apply(ObjMesh& mesh,DeformMode mode,int axis,float strength,float angleDegrees,FFDGrid& ffd) {
    if(mesh.originalVertices.empty()) return;
    mesh.deformedVertices.resize(mesh.originalVertices.size());
    for(size_t i=0;i<mesh.originalVertices.size();++i) {
        const Vec3& p=mesh.originalVertices[i];
        switch(mode) {
        case DeformMode::Taper: mesh.deformedVertices[i]=applyTaper(p,mesh.bounds,axis,strength); break;
        case DeformMode::Twist: mesh.deformedVertices[i]=applyTwist(p,mesh.bounds,axis,angleDegrees); break;
        case DeformMode::Bend: mesh.deformedVertices[i]=applyBend(p,mesh.bounds,axis,angleDegrees); break;
        case DeformMode::FFD: mesh.deformedVertices[i]=applyFFD(p,ffd); break;
        default: mesh.deformedVertices[i]=p; break;
        }
    }
    mesh.recalculateNormals();
}
