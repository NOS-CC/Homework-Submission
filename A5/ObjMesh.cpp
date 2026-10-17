#include "ObjMesh.h"
#include <fstream>
#include <sstream>
#include <limits>

bool ObjMesh::load(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if(!in) { error = "Cannot open OBJ file: " + path; return false; }

    std::vector<Vec3> positions;
    std::vector<unsigned int> newIndices;
    std::string line;
    while(std::getline(in,line)) {
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if(tag=="v") {
            Vec3 p; if(ss>>p.x>>p.y>>p.z) positions.push_back(p);
        } else if(tag=="f") {
            std::vector<int> face;
            std::string token;
            while(ss>>token) {
                const size_t slash=token.find('/');
                std::string head=token.substr(0,slash);
                if(head.empty()) continue;
                int idx=0;
                try { idx=std::stoi(head); } catch(...) { idx=0; }
                if(idx==0) continue;
                int resolved = idx>0 ? idx-1 : (int)positions.size()+idx;
                if(resolved>=0 && resolved<(int)positions.size()) face.push_back(resolved);
            }
            // Fan triangulation supports triangles, quads, and general convex polygons.
            for(size_t i=1;i+1<face.size();++i) {
                newIndices.push_back((unsigned)face[0]);
                newIndices.push_back((unsigned)face[i]);
                newIndices.push_back((unsigned)face[i+1]);
            }
        }
    }
    if(positions.empty() || newIndices.empty()) {
        error = "OBJ contains no usable vertices/faces."; return false;
    }
    originalVertices = std::move(positions);
    indices = std::move(newIndices);
    deformedVertices = originalVertices;
    calculateBounds();
    recalculateNormals();
    error.clear();
    return true;
}

void ObjMesh::calculateBounds() {
    Vec3 mn(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max());
    Vec3 mx(-mn.x,-mn.y,-mn.z);
    for(const auto& p:originalVertices) {
        mn.x=std::min(mn.x,p.x); mn.y=std::min(mn.y,p.y); mn.z=std::min(mn.z,p.z);
        mx.x=std::max(mx.x,p.x); mx.y=std::max(mx.y,p.y); mx.z=std::max(mx.z,p.z);
    }
    bounds.min=mn; bounds.max=mx; bounds.center=(mn+mx)*0.5f;
    bounds.maxExtent=std::max({mx.x-mn.x,mx.y-mn.y,mx.z-mn.z,0.001f});
}
void ObjMesh::resetDeformation() { deformedVertices=originalVertices; recalculateNormals(); }
void ObjMesh::recalculateNormals() {
    normals.assign(deformedVertices.size(),Vec3(0,0,0));
    for(size_t i=0;i+2<indices.size();i+=3) {
        unsigned a=indices[i],b=indices[i+1],c=indices[i+2];
        if(a>=normals.size()||b>=normals.size()||c>=normals.size()) continue;
        Vec3 n=cross(deformedVertices[b]-deformedVertices[a],deformedVertices[c]-deformedVertices[a]);
        normals[a]+=n; normals[b]+=n; normals[c]+=n;
    }
    for(auto& n:normals) n=normalize(n);
}
