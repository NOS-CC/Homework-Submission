#pragma once
#include <cmath>
#include <algorithm>

struct Vec3 {
    float x=0, y=0, z=0;
    Vec3() = default;
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator+(const Vec3& b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(const Vec3& b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
    Vec3 operator/(float s) const { return {x/s,y/s,z/s}; }
    Vec3& operator+=(const Vec3& b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
};
inline float dot(const Vec3& a,const Vec3& b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(const Vec3& a,const Vec3& b){ return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float length(const Vec3& v){ return std::sqrt(dot(v,v)); }
inline Vec3 normalize(const Vec3& v){ float l=length(v); return l>1e-8f?v/l:Vec3(0,1,0); }
inline Vec3 mix(const Vec3& a,const Vec3& b,float t){ return a*(1-t)+b*t; }

struct Mat4 {
    float v[16]{};
    static Mat4 identity() { Mat4 m; m.v[0]=m.v[5]=m.v[10]=m.v[15]=1; return m; }
};
inline Mat4 multiply(const Mat4& a,const Mat4& b) {
    Mat4 r;
    for(int c=0;c<4;c++) for(int row=0;row<4;row++)
        r.v[c*4+row]=a.v[row]*b.v[c*4]+a.v[4+row]*b.v[c*4+1]+a.v[8+row]*b.v[c*4+2]+a.v[12+row]*b.v[c*4+3];
    return r;
}
inline Mat4 perspective(float fovDeg,float aspect,float nearZ,float farZ) {
    Mat4 m; float f=1.0f/std::tan(fovDeg*3.1415926535f/360.0f);
    m.v[0]=f/aspect; m.v[5]=f; m.v[10]=(farZ+nearZ)/(nearZ-farZ); m.v[11]=-1;
    m.v[14]=(2*farZ*nearZ)/(nearZ-farZ); return m;
}
inline Mat4 lookAt(Vec3 eye,Vec3 center,Vec3 up) {
    Vec3 f=normalize(center-eye), s=normalize(cross(f,up)), u=cross(s,f);
    Mat4 m=Mat4::identity();
    m.v[0]=s.x; m.v[1]=u.x; m.v[2]=-f.x;
    m.v[4]=s.y; m.v[5]=u.y; m.v[6]=-f.y;
    m.v[8]=s.z; m.v[9]=u.z; m.v[10]=-f.z;
    m.v[12]=-dot(s,eye); m.v[13]=-dot(u,eye); m.v[14]=dot(f,eye);
    return m;
}
inline Mat4 translation(const Vec3& p) { Mat4 m=Mat4::identity(); m.v[12]=p.x;m.v[13]=p.y;m.v[14]=p.z;return m; }
inline Mat4 scaleMatrix(float s) { Mat4 m=Mat4::identity();m.v[0]=m.v[5]=m.v[10]=s;return m; }
