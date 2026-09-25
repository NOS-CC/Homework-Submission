# Homework-Submission

## **A3:Bezier曲线**

### **提交文件构成：**

- A3
  1. `main.cpp`
  2. `DrawCurve.exe`

### **文件说明**

- `main.cpp`:源码文件，但是不能直接运行，涉及到 `OpenGL`、`GLFW`、`GLAD`、`Dear ImGUI` 外部库。
- `DrawCurve.exe`:可运行版本，内部所有曲线均为代码写死的预制曲线，在程序内通过选中不同曲线，然后在 `Viewport` 里操作控制点来实时修改曲线。右侧有参数可以调整。

---

## **A2:模型的表达-OBJ**

### **提交文件构成：**

- A2
  1. `Model_Display.unitypackage`
  2. `ObjGLViwer.cs`
  3. `ObjParser.cs`
  4. `Pyramid.obj`

### **文件说明**

- `Model_Display.unitypackage`:Unity压缩文件，可以直接下载并导入`unity`打开，内含以下两个脚本。
- `ObjGLViwer.cs`:图形绘制脚本，挂载至主摄像机，并设定OBJ文件路径`（绝对路径）`与基础颜色，进入`Play`模式`Game`视窗便能预览模型。
- `ObjParser.cs`:OBJ模型解析类，服务于图形绘制脚本。
- `Pyramid.obj`:示例OBJ模型，仅含顶点与面。

### **运行方式（二选一）**

1. 下载文件1与文件4，在 `Unity Hub` 中新建 `3D Build-in Pipline` 项目，导入`Unitypackage`，选中摄像机，修改 `ObjGLViwer` 组件中的模型绝对路径，点击 `Play` 按钮，进入 `Game` 视窗浏览。 
2. 下载文件2、文件3、文件4，在 `Unity Hub` 中新建 `3D Build-in Pipline` 项目，选中摄像机，挂载 `ObjGLViwer` 脚本组件，设置模型绝对路径，点击 `Play` 按 钮，进入 `Game` 视窗浏览。
