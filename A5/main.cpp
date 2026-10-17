#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "Math3D.h"
#include "ObjMesh.h"
#include "Deformer.h"

#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cmath>

struct GPUVertex { float px,py,pz,nx,ny,nz; };
static const char* VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vPos;
void main(){ vec4 world=uModel*vec4(aPos,1.0); vPos=world.xyz; vNormal=mat3(uModel)*aNormal; gl_Position=uMVP*vec4(aPos,1.0); }
)";
static const char* FS = R"(
#version 330 core
in vec3 vNormal;
in vec3 vPos;
uniform vec3 uColor;
out vec4 FragColor;
void main(){
 vec3 n=normalize(vNormal);
 vec3 light=normalize(vec3(0.5,1.0,0.7));
 float diffuse=max(dot(n,light),0.0);
 float lighting=0.25+0.75*diffuse;
 FragColor=vec4(uColor*lighting,1.0);
}
)";
static GLuint makeShader(GLenum type,const char* src) {
    GLuint s=glCreateShader(type); glShaderSource(s,1,&src,nullptr); glCompileShader(s);
    GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[2048]; glGetShaderInfoLog(s,sizeof(log),nullptr,log); std::fprintf(stderr,"Shader error: %s\n",log); }
    return s;
}
static GLuint makeProgram() {
    GLuint vs=makeShader(GL_VERTEX_SHADER,VS), fs=makeShader(GL_FRAGMENT_SHADER,FS);
    GLuint p=glCreateProgram(); glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048];glGetProgramInfoLog(p,sizeof(log),nullptr,log);std::fprintf(stderr,"Link error: %s\n",log);}
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
static void uploadMesh(const ObjMesh& mesh,GLuint vao,GLuint vbo,GLuint ebo) {
    std::vector<GPUVertex> data; data.reserve(mesh.deformedVertices.size());
    for(size_t i=0;i<mesh.deformedVertices.size();++i) {
        Vec3 p=mesh.deformedVertices[i],n= i<mesh.normals.size()?mesh.normals[i]:Vec3(0,1,0);
        data.push_back({p.x,p.y,p.z,n.x,n.y,n.z});
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(GPUVertex),data.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.indices.size()*sizeof(unsigned),mesh.indices.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
static void drawLines(GLuint program,GLuint vao,GLuint vbo,const std::vector<Vec3>& points,const Mat4& mvp,Vec3 color) {
    if(points.empty()) return;
    std::vector<GPUVertex> data; data.reserve(points.size());
    for(auto p:points) data.push_back({p.x,p.y,p.z,0,1,0});
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(GPUVertex),data.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"uMVP"),1,GL_FALSE,mvp.v);
    Mat4 identity=Mat4::identity(); glUniformMatrix4fv(glGetUniformLocation(program,"uModel"),1,GL_FALSE,identity.v);
    glUniform3f(glGetUniformLocation(program,"uColor"),color.x,color.y,color.z);
    glDrawArrays(GL_LINES,0,(GLsizei)data.size());
    glBindVertexArray(0);
}
static std::vector<Vec3> makeFFDLines(const FFDGrid& g) {
    std::vector<Vec3> out;
    for(int k=0;k<g.n;k++) for(int j=0;j<g.n;j++) for(int i=0;i<g.n;i++) {
        Vec3 p=g.points[g.index(i,j,k)];
        if(i+1<g.n){out.push_back(p);out.push_back(g.points[g.index(i+1,j,k)]);}
        if(j+1<g.n){out.push_back(p);out.push_back(g.points[g.index(i,j+1,k)]);}
        if(k+1<g.n){out.push_back(p);out.push_back(g.points[g.index(i,j,k+1)]);}
    }
    return out;
}
static std::vector<Vec3> makeControlPoints(const FFDGrid& g) {
    std::vector<Vec3> out; out.reserve(g.points.size());
    for(auto p:g.points){out.push_back(p);out.push_back(p);}
    return out;
}
static void drawControlPoints(GLuint program,GLuint vao,GLuint vbo,const FFDGrid& g,const Mat4& mvp,int selected) {
    if(g.points.empty()) return;
    std::vector<GPUVertex> data;
    for(size_t i=0;i<g.points.size();++i) {
        Vec3 p=g.points[i]; Vec3 c=(int)i==selected?Vec3(1,0.2f,0.1f):Vec3(1.0f,0.75f,0.2f);
        data.push_back({p.x,p.y,p.z,c.x,c.y,c.z});
    }
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(GPUVertex),data.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"uMVP"),1,GL_FALSE,mvp.v);
    Mat4 identity=Mat4::identity();glUniformMatrix4fv(glGetUniformLocation(program,"uModel"),1,GL_FALSE,identity.v);
    glUniform3f(glGetUniformLocation(program,"uColor"),1,1,1);
    glPointSize(9.0f);glDrawArrays(GL_POINTS,0,(GLsizei)data.size());glBindVertexArray(0);
}

int main() {
    if(!glfwInit()){std::fprintf(stderr,"GLFW init failed\n");return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window=glfwCreateWindow(1440,900,"Global Transform & Deformation Demo",nullptr,nullptr);
    if(!window){glfwTerminate();return -1;}
    glfwMakeContextCurrent(window);glfwSwapInterval(1);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){std::fprintf(stderr,"GLAD init failed\n");return -1;}
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
    GLuint program=makeProgram(),vao=0,vbo=0,ebo=0,lineVao=0,lineVbo=0;
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ebo);
    glGenVertexArrays(1,&lineVao);glGenBuffers(1,&lineVbo);

    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding=5;ImGui::GetStyle().FrameRounding=4;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");

    ObjMesh mesh; FFDGrid ffd; DeformMode mode=DeformMode::None;
    int axis=1,ffdN=4,selectedPoint=0; float taperAmount=-0.65f,angle=90.0f;
    bool showWire=false,showFFD=true; char path[512]="";
    std::string status="Import an OBJ model to begin.";
    bool meshUploaded=false; int lastMode=-1,lastN=-1;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int fw,fh;glfwGetFramebufferSize(window,&fw,&fh); if(fh<1)fh=1;
        glViewport(0,0,fw,fh);glClearColor(0.075f,0.085f,0.11f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10,10),ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(310,(float)fh-20),ImGuiCond_Always);
        ImGui::Begin("Model / Deformation",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        ImGui::TextWrapped("Global Transform and Deformation");
        ImGui::Separator();
        ImGui::InputText("OBJ path",path,sizeof(path));
        if(ImGui::Button("Import OBJ",ImVec2(-1,32))) {
            std::string err;
            if(mesh.load(path,err)) {
                ffd.build(mesh.bounds,ffdN);mode=DeformMode::None;selectedPoint=0;
                status="Loaded: "+path;meshUploaded=false;
            } else status=err;
        }
        ImGui::TextWrapped("%s",status.c_str());
        if(!mesh.empty()) {
            ImGui::Text("Vertices: %d",(int)mesh.originalVertices.size());
            ImGui::Text("Triangles: %d",(int)mesh.indices.size()/3);
            ImGui::Text("Bounds size: %.3f, %.3f, %.3f",
                mesh.bounds.max.x-mesh.bounds.min.x,mesh.bounds.max.y-mesh.bounds.min.y,mesh.bounds.max.z-mesh.bounds.min.z);
        }
        ImGui::Separator();ImGui::Text("Select deformation");
        if(ImGui::Button("No deformation",ImVec2(-1,28)))mode=DeformMode::None;
        if(ImGui::Button("Taper (conical)",ImVec2(-1,28)))mode=DeformMode::Taper;
        if(ImGui::Button("Twist",ImVec2(-1,28)))mode=DeformMode::Twist;
        if(ImGui::Button("Bend",ImVec2(-1,28)))mode=DeformMode::Bend;
        if(ImGui::Button("FFD",ImVec2(-1,28))) {
            mode=DeformMode::FFD;
            if(!mesh.empty()) ffd.build(mesh.bounds,ffdN);
        }
        const char* axes[]={"X axis","Y axis","Z axis"};
        ImGui::Combo("Deform axis",&axis,axes,3);
        if(mode==DeformMode::Taper) ImGui::SliderFloat("Taper amount",&taperAmount,-0.9f,2.0f);
        if(mode==DeformMode::Twist || mode==DeformMode::Bend) ImGui::SliderFloat("Angle (degrees)",&angle,-180.0f,180.0f);
        if(mode==DeformMode::FFD) {
            if(ImGui::SliderInt("Grid resolution",&ffdN,2,6)) {
                if(!mesh.empty()) ffd.build(mesh.bounds,ffdN);
                selectedPoint=0;
            }
            ImGui::Checkbox("Show control cage",&showFFD);
            if(!ffd.points.empty()) {
                selectedPoint=std::max(0,std::min(selectedPoint,(int)ffd.points.size()-1));
                ImGui::Text("Control points: %d",(int)ffd.points.size());
                ImGui::SliderInt("Selected point",&selectedPoint,0,(int)ffd.points.size()-1);
                Vec3& p=ffd.points[selectedPoint];
                float step=std::max(mesh.bounds.maxExtent*0.01f,0.001f);
                ImGui::DragFloat("Point X",&p.x,step);
                ImGui::DragFloat("Point Y",&p.y,step);
                ImGui::DragFloat("Point Z",&p.z,step);
                if(ImGui::Button("Reset control cage",ImVec2(-1,28)))ffd.reset();
                ImGui::TextWrapped("Grid indexing: i + n * (j + n * k)");
            }
        }
        if(ImGui::Button("Reset deformation",ImVec2(-1,30))) {
            if(!mesh.empty()){mode=DeformMode::None;ffd.reset();mesh.resetDeformation();meshUploaded=false;}
        }
        ImGui::Checkbox("Wireframe overlay",&showWire);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2((float)fw-270,10),ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(260,(float)fh-20),ImGuiCond_Always);
        ImGui::Begin("Information",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        ImGui::Text("Current mode:");
        switch(mode) {
            case DeformMode::None:ImGui::TextColored(ImVec4(.5f,1,.6f,1),"None");break;
            case DeformMode::Taper:ImGui::TextColored(ImVec4(.5f,1,.6f,1),"Taper");break;
            case DeformMode::Twist:ImGui::TextColored(ImVec4(.5f,1,.6f,1),"Twist");break;
            case DeformMode::Bend:ImGui::TextColored(ImVec4(.5f,1,.6f,1),"Bend");break;
            case DeformMode::FFD:ImGui::TextColored(ImVec4(.5f,1,.6f,1),"Free Form Deformation");break;
        }
        ImGui::Separator();
        ImGui::TextWrapped("OBJ import supports v and f records, including negative face indices and polygon triangulation.");
        ImGui::Spacing();
        ImGui::TextWrapped("FFD pipeline: model bounds -> n*n*n lattice -> edit lattice points -> evaluate Bernstein basis for each original vertex.");
        ImGui::Spacing();
        ImGui::TextWrapped("Tip: for a visible FFD result, select a lattice point away from the center and change one coordinate.");
        ImGui::End();

        // Re-evaluate deformation from original vertices every frame (no accumulated error).
        if(!mesh.empty()) {
            Deformer::apply(mesh,mode,axis,taperAmount,angle,ffd);
            uploadMesh(mesh,vao,vbo,ebo);
            meshUploaded=true;
        }

        // Viewport occupies the area between the two ImGui panels.
        float left=330.0f,right=fw-280.0f;
        float vpW=std::max(100.0f,right-left),vpH=(float)fh;
        glViewport((int)left,0,(int)vpW,fh);
        glEnable(GL_SCISSOR_TEST);glScissor((int)left,0,(int)vpW,fh);
        glClearColor(0.09f,0.105f,0.135f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
        float aspect=vpW/vpH;
        Mat4 proj=perspective(45,aspect,0.01f,1000);
        Vec3 center=mesh.empty()?Vec3(0,0,0):mesh.bounds.center;
        float radius=mesh.empty()?2.0f:std::max(mesh.bounds.maxExtent,0.5f)*2.2f;
        Mat4 view=lookAt(center+Vec3(radius*1.5f,radius*1.15f,radius*2.0f),center,Vec3(0,1,0));
        Mat4 model=Mat4::identity();
        Mat4 mvp=multiply(proj,multiply(view,model));
        if(!mesh.empty()) {
            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program,"uMVP"),1,GL_FALSE,mvp.v);
            glUniformMatrix4fv(glGetUniformLocation(program,"uModel"),1,GL_FALSE,model.v);
            glUniform3f(glGetUniformLocation(program,"uColor"),0.35f,0.72f,0.95f);
            glBindVertexArray(vao);
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glDrawElements(GL_TRIANGLES,(GLsizei)mesh.indices.size(),GL_UNSIGNED_INT,0);
            if(showWire) {
                glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
                glUniform3f(glGetUniformLocation(program,"uColor"),0.025f,0.04f,0.06f);
                glDrawElements(GL_TRIANGLES,(GLsizei)mesh.indices.size(),GL_UNSIGNED_INT,0);
                glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            }
            glBindVertexArray(0);
            if(mode==DeformMode::FFD && showFFD) {
                auto lines=makeFFDLines(ffd);
                drawLines(program,lineVao,lineVbo,lines,mvp,Vec3(1.0f,0.65f,0.15f));
                // Point sprites are rendered in the same viewport, with colors encoded in the vertex buffer.
                std::vector<GPUVertex> pdata;
                for(size_t i=0;i<ffd.points.size();++i) {
                    Vec3 p=ffd.points[i]; Vec3 c=(int)i==selectedPoint?Vec3(1,0.18f,0.12f):Vec3(1,0.78f,0.22f);
                    pdata.push_back({p.x,p.y,p.z,c.x,c.y,c.z});
                }
                glBindVertexArray(lineVao);glBindBuffer(GL_ARRAY_BUFFER,lineVbo);
                glBufferData(GL_ARRAY_BUFFER,pdata.size()*sizeof(GPUVertex),pdata.data(),GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)0);glEnableVertexAttribArray(0);
                glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(GPUVertex),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
                glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"uMVP"),1,GL_FALSE,mvp.v);
                glUniformMatrix4fv(glGetUniformLocation(program,"uModel"),1,GL_FALSE,model.v);
                glUniform3f(glGetUniformLocation(program,"uColor"),1,1,1);
                glPointSize(9);glDrawArrays(GL_POINTS,0,(GLsizei)pdata.size());glBindVertexArray(0);
            }
        } else {
            // Helpful viewport placeholder.
            ImGui::SetNextWindowPos(ImVec2(left+20,fh*0.42f),ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(vpW-40,100),ImGuiCond_Always);
            ImGui::Begin("Welcome",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs);
            ImGui::SetWindowFontScale(1.3f);
            ImGui::Text("Import an OBJ model to start deforming.");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::End();
        }
        glViewport(0,0,fw,fh);
        ImGui::Render();glDisable(GL_SCISSOR_TEST);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();
    glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ebo);glDeleteVertexArrays(1,&vao);
    glDeleteBuffers(1,&lineVbo);glDeleteVertexArrays(1,&lineVao);glDeleteProgram(program);
    glfwDestroyWindow(window);glfwTerminate();return 0;
}
