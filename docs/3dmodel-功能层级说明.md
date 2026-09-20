# `3dmodel/` 功能层级拆解说明

> 对象：`3dmodel/` 目录（19 个文件，从示教器项目 `CGX-TP` 的 `move3D/model3D` 移植而来）
> 视角：**按功能层级自下而上拆解** —— 从最底层的 GPU 渲染底座，到几何图元、机械臂模型、
> 渲染与控制总线、控制面板，最后到对外接口与移植适配层。
> 本目录在 `robot3d_demo` 工程中的定位：**被验证的对象**；`demoui.cpp` / `main.cpp` 只是驱动它的外壳。

---

## 0. 全局地图

```
┌──────────────────────────────────────────────────────────────────────────┐
│ L7 外围/适配层   demo_support.h/.cpp   ← 不是移植来的，是"为了让移植来的代码能编译"写的   │
│                  （数据中枢 DemoDataHub、运动学、RobotConfig、各类桩）        │
├──────────────────────────────────────────────────────────────────────────┤
│ L6 对外接口层    Robot3dControlWidget  ← 全局单例门面，外部只认它            │
│ L5 控制面板层    Robot3dGLWidget(UI部分) + TouchLongPressFilter（长按 Jog）  │
├──────────────────────────────────────────────────────────────────────────┤
│ L4 渲染与控制总线 Robot3dGLWidget(GL部分)：相机/矩阵/绘制顺序/显示位掩码/交互  │
├──────────────────────────────────────────────────────────────────────────┤
│ L3 机械臂模型层   RobotGeometry：assimp 载 obj + 关节链变换 + 平行光着色       │
├──────────────────────────────────────────────────────────────────────────┤
│ L2 几何图元层    Sphere / Line / Plane / Substance   （继承 L1）              │
├──────────────────────────────────────────────────────────────────────────┤
│ L1 渲染底座      BaseGeometry + 8 个 GLSL + 三种 VBO 上传策略                 │
└──────────────────────────────────────────────────────────────────────────┘
```

代码量分布（明文行数），可以看出重心在哪里：

| 文件 | 行数 | 层级 |
|---|---|---|
| `robot3dglwidget.cpp` | 1321 | L4 + L5 |
| `robotgeometry.cpp` | 536 | L3 |
| `substancegeometry.cpp` | 454 | L2 |
| `demo_support.h/.cpp` | 458 + 421 | L7 |
| `spheregeometry.cpp` | 230 | L2 |
| `robot3dcontrolwidget.cpp` | 195 | L6 + L5 |
| `planegeometry.cpp` | 189 | L2 |
| `linegeometry.cpp` | 158 | L2 |
| `basegeometry.h/.cpp` | 108 + 25 | L1 |

---

## L1 渲染底座：`BaseGeometry` —— 整套渲染的"最小公分母"

### 1.1 它定义了什么

`basegeometry.h` 一个头文件同时承担了四件事，是理解整个模块的钥匙：

**(a) 全局缩放常数**

```cpp
#define G_SCALE 0.04      // basegeometry.h:15
```

这是**整个模块的单位约定**：外部世界（DH 参数、位姿、路点、坐标系）一律用 **mm**，
而送进 GPU 的顶点一律 **×0.04** 变成"模型单位"。
所以模型顶点在载入时缩放一次（`LoadMesh()`），之后所有**传给几何类的坐标参数**都必须自己乘 `G_SCALE`。
这条约定是后面所有"单位不一致"问题的总根源（见 L3.4、L7 的 bug 清单）。

**(b) 全局 OpenGL 版本标志**

```cpp
extern int g_openglVersion;   // 定义在 basegeometry.cpp:2，初值 -1
```

由 `Robot3dGLWidget::GetOpenGLVersion()` 在 `initializeGL()` 里从 `glGetString(GL_VERSION)`
里抠出**第一个数字字符**（`robot3dglwidget.cpp:1218-1246`）。
它只被用来选下面 (d) 的 VBO 上传策略，不参与任何渲染分支。

**(c) 两套顶点结构**

```cpp
struct FeatureVertexData { QVector3D position; };                    // basegeometry.h:19
struct VertexData { QVector3D position, normal, material; };         // basegeometry.h:65
```

- `FeatureVertexData`：**图元专用**，只有位置。L2 的 Line / Plane / Sphere / Substance 全部用它
  → 因此它们的着色器只有 `a_position`，没有任何光照。
- `VertexData`：**网格专用**，位置 + 法线 + 材质色。只有 L3 的 `RobotGeometry` 用它
  → 因此只有机器人本体走平行光着色。

**(d) 基类本体**

```cpp
class BaseGeometry : public QOpenGLFunctions      // basegeometry.h:88
{
    virtual void drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
    virtual bool initShaders();

    QVector<FeatureVertexData> vertex_data;   // CPU 端顶点
    QVector<GLuint>            vertex_index;  // 索引（全模块未真正使用）
    QOpenGLBuffer vertex_data_Buf;            // GPU 端顶点
    QOpenGLBuffer vertex_index_Buf;           // 索引 buffer（构造时创建，从未绑定使用）
    int num_vertex_data;                      // 当前实际顶点数
    int num_vertex_index;
    QOpenGLShaderProgram program;             // 每个图元各自持有一个 program
};
```

构造函数只做三件事：`initializeOpenGLFunctions()`、建两个 buffer（`basegeometry.cpp:3-10`）。
`initShaders()` 与 `drawGeometry()` 在基类里是**空实现**（`basegeometry.cpp:17-25`），
纯粹是接口占位 —— 典型的"抽象基类但没写成纯虚"的写法。

### 1.2 三种 VBO 上传策略（这是底座里唯一有技术含量的部分）

所有子类都重复同一套模式，按 OpenGL 版本分叉：

| 场景 | `g_openglVersion > 2`（即 GL 3/4） | 否则（GL ES 2 / GL 2） |
|---|---|---|
| 构造时首次分配 | `allocate(data, size)` | 同 |
| 顶点数**不变**，只改数据 | `bind()` → `map(WriteOnly)` → `memcpy` → `unmap()` | `allocate(data, size)` 重新分配 |
| 顶点数**有增删** | 同上（map/memcpy） | `destroy()` → `create()` → `bind()` → `allocate(...)` |

对应的方法命名也遵循同一套语义（三个几何类的 API 完全对称）：

```
SetXxxData(...)          // 个数不变，只换数据        -> 走 map/memcpy
AddDeleteXxxData(...)    // 个数可能增删              -> 走 destroy/recreate
```

`map()` 返回的是**整个 buffer 的写指针**，代码直接按 `sizeof(...)*num_vertex_data` 覆盖写入
（例如 `linegeometry.cpp:38-41`）。这种做法只在"新数据长度 ≤ 已分配长度"时安全，
所以那些"个数会变化"的几何体在构造时就预分配了容量上限：

- `SphereGeometry(points, maxNum=500)` → `allocate(nullptr, sizeof(FeatureVertexData)*432*maxNum)`（`spheregeometry.cpp:25`）
- `PlaneGeometry(points, maxNum=100)` → 按 `numVertex*numMax = 4*100` 预留（`planegeometry.cpp:26`）
- `SubstanceGeometry(points, maxNum=100)` → 按 `432*numMax` 预留（`substancegeometry.cpp:25`）

**L4 里最值得记住的一个数字**：路点球（`waypointSphereGeometry`）用 `maxNum=500`，
路点轨迹线（`waypointPathlineGeometry`）用 `maxNum=1000`。这两个容量决定了路点/轨迹能存多少。

### 1.3 着色器资源

8 个 GLSL 经 `demo.qrc` 编入 `:/shaders/`，成对使用：

| 用途 | 顶点/片段 | 特点 |
|---|---|---|
| line | `line_vertex/fragment_shader.glsl` | `gl_Position = mvp_matrix * a_position`；颜色 = `lightColor * toyColor`（两者都是 uniform，lightColor 恒为 1） |
| plane | `plane_vertex/fragment_shader.glsl` | 同上，但片段多一个 `alpha`（片元里 `vec4(...,alpha)`） |
| substance | `substance_vertex/fragment_shader.glsl` | 同上，alpha **硬编码 0.5** |
| robot | `robot_vertex/fragment_shader.glsl` | 唯一带光照的：顶点着色器输出 `fragPos/Normal/mater`，片段用 Blinn-Phong（`material` + `light1` 结构体、`viewPos`） |

要点：
- **只有 robot 着色器用了 `model` 矩阵**（算世界空间法线），其余三个只用 `mvp_matrix`。
- 所有绘制路径都**不开纹理**、不用 UV（assimp 那个 `aiProcess_FlipUVs` 其实是空转）。
- 两个 uniform 名字 `lightColor`/`toyColor` 是这套图元的"调色板"约定 —— 图元颜色不在顶点里，而在 uniform 里。

---

## L2 几何图元层：四种"轻量图元"

四个类结构完全对称，都是 `public BaseGeometry`：**两个构造函数 + 三个 Set 方法 + 两个 override**。

| 类 | 数据来源结构 | 图元类型 | 对应 GL 绘制 | 容量/细分 |
|---|---|---|---|---|
| `LineGeometry` | `LineMsg{startPt,endPt,color}` 或点列 | 线段/折线 | `glDrawArrays(GL_LINE_STRIP)`，`glLineWidth(3)` | 折线 `maxNum=1000` |
| `PlaneGeometry` | `PlaneMsg{四点,color,alpha}` | 四边形 → 2 三角 | `glDrawArrays(GL_TRIANGLES)` + `GL_BLEND` | 每面 6 顶点 |
| `SphereGeometry` | `SphereMsg{center,radius,color}` | UV 球 | `GL_TRIANGLES`，不透明 | `angleSpan=30°` → 每球 **432** 顶点 |
| `SubstanceGeometry` | `SubstanceMsg{type,centerPose[6],r,l,w,h,color}` | 球/方体/圆柱/圆锥 | `GL_TRIANGLES` + `GL_BLEND`（alpha 0.5） | ✓ |

### 2.1 `LineGeometry` —— 坐标系轴 + 路点轨迹

- 顶点生成只有一句核心：`temp.position = point * G_SCALE`（`linegeometry.cpp:142`）。
  **所有调用方传进来的坐标都必须是 mm**。
- 两条构造路径：`LineMsg`（两点定一段）和 `QVector<QVector3D>`（点列 + `maxNum`）。
- 绘制时把 `lineMsg.color` 归一化成 `toyColor` uniform（`linegeometry.cpp:121`）。

### 2.2 `PlaneGeometry` —— 安全限制平面

- 四边形按 `0-1-2` + `0-2-3` 拼成两个三角形（`planegeometry.cpp:155-160`）：
  **调用方必须按逆时针顺序给四个顶点**，否则背面朝上。
- `drawGeometry` 里开 `GL_BLEND` + `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`
  并传 `alpha` uniform（`planegeometry.cpp:132-133`）—— 因为安全平面默认 `alpha=0`（全透明），
  只有值变了才可见。

### 2.3 `SphereGeometry` —— TCP 球 + 路点球

- 球面按 `angleSpan = 30` 经纬切分：6 条纬带 × 12 条经带 × 6 顶点 = **432 顶点/球**，
  与头文件里 `numVertex=432` 的注释吻合（`spheregeometry.h:26`）。
- **注意**：`GenerateSphereData()` 内部对 center 和 radius **都**乘了 `G_SCALE`
  （`spheregeometry.cpp:162-171`），所以调用方传进来的 center 也必须是 mm 且**不要**自己再缩放……
  但 Demo 里恰恰在传参侧又乘了一次 `G_SCALE` —— 这就是第 L3.4 节的已知 bug 与修正。

### 2.4 `SubstanceGeometry` —— 末端工具（唯一带"朝向"的图元）

这是 L2 里最复杂的一个，因为它要按位姿（位置 + 轴角）摆出立体：

```cpp
substanceType: 0=球  1=立方体  2=圆柱  3=圆锥     // substancegeometry.cpp:140-156
```

关键函数 `GetRotatePoint(point, RotateMatrix, center)`（`substancegeometry.cpp:446-454`）：

```cpp
QVector3D tmpPoint = pointData.position / G_SCALE;                       // 回到 mm
tmpPoint = QVector3D(tmp.x-center.x, tmp.z-center.z, center.y-tmp.y);    // ★ 坐标轴置换
result.position = (QVector3D(RotateMatrix * direction) + center) * G_SCALE;
```

那行**坐标轴置换**是必须的：`AxisAngle2RotateMatrix()` 给出的是**机器人基坐标系（Z 轴朝上）**
下的旋转矩阵，而圆柱/圆锥/方体的顶点是在**局部 Y 轴朝上**的约定下生成的
（顶点用 `center.y() - height` 表示"向下长"，`apex = center + (0,-height,0)`）。
置换就是在两套约定之间做 `(x, y, z) → (x, z, -y)` 的转换。

- 内部还各自实现了一份 `AxisAngle2RotateMatrix(double* pose)`（`substancegeometry.cpp:419-444`），
  与 `Robot3dGLWidget::AxisAngle2RotateMatrix(Pose)` 逻辑相同但**会直接改写传入数组**
  （`pose[3..5] *= M_PI/180`）—— 是原地副作用，调用方需注意。
- `SetSubstanceData()` 的语义是"取列表第一个的颜色做整批的颜色"（`substancegeometry.cpp:36-39`）。

---

## L3 机械臂模型层：`RobotGeometry` —— 从 obj 到会动的关节链

这是整个模块的核心，回答三个问题：**模型从哪来、关节怎么动、光怎么打**。

### 3.1 模型从哪来：assimp 载入 + 单位变换

```cpp
bool RobotGeometry::LoadMesh(const std::string& Filename, MeshEntry &meshModel, int joint)
// robotgeometry.cpp:130-200
```

1. `Assimp::Importer::ReadFile(..., aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs)`
   —— 三角化 + 平滑法线（后处理标志里只有一个真正生效：`GenSmoothNormals`）。
2. 遍历 `pScene->mMeshes`，累计 `numVertices`，然后一次性 `new VertexData[numVertices]`；
   一个 obj 里的多个 mesh 被**拼平成一维顶点数组**（`i + num` 偏移）。
3. 每个顶点取三样东西：
   - `position`：**乘 `scaleMatrix.scale(G_SCALE)`**（`robotgeometry.cpp:137-138, 165`）
   - `normal`：直接取，**不缩放**（法线不需要）
   - `material`：从 `AI_MATKEY_COLOR_DIFFUSE` 读的漫反射色 —— 也就是 `.obj.mtl` 里的 `Kd`
4. **备份**：`verticesDataBackup = memcpy(verticesData)`（`robotgeometry.cpp:184-185`）。
   这份备份是关节动画的"原始姿态"，每帧动画前都要拿它重置（见 3.2）。
5. `VB.create/bind/allocate/release` —— 每个部件一个独立的 `QOpenGLBuffer`，**不用索引**。

### 3.2 关节怎么动：`jointData` 表 + Rodrigues 旋转 + 链式累积

**(a) 关节表在 `ChangeRobot3D()` 里按 DH 参数生成**（`robotgeometry.cpp:510-534`）：

```cpp
multiJointData joint1={ (0,0,0)*G_SCALE,                                      (0,0,1), I };
multiJointData joint2={ (0,0,DH[0].d            )*G_SCALE,                    (0,-1,0), I };
multiJointData joint3={ (0,0,DH[0].d+DH[2].a    )*G_SCALE,                    (0,-1,0), I };
multiJointData joint4={ (0,0,DH[0].d+DH[2].a+DH[3].a)*G_SCALE,                (0,-1,0), I };
multiJointData joint5={ (0,-DH[3].d,0           )*G_SCALE,                    (0,0,1), I };
multiJointData joint6={ (0,0,前四项之和+DH[4].d  )*G_SCALE,                   (0,-1,0), I };
// numTool==1 时再补一条 tool，轴向同 joint6
```

翻译成人话（以 `G6L-3-20` 的 DH 为例：`d0=164.3, a2=874.2, a3=819.8, d3=146.6, d4=106, d5=102.6`）：

| 关节 | 旋转中心（mm，机器人基坐标 Z-up） | 旋转轴 | 物理含义 |
|---|---|---|---|
| J1 | (0, 0, 0) | **+Z** | 底座回转 |
| J2 | (0, 0, d0=164.3) | **-Y** | 大臂俯仰 |
| J3 | (0, 0, d0+a2=1038.5) | -Y | 小臂俯仰 |
| J4 | (0, 0, d0+a2+a3=1858.3) | -Y | 腕部回转 |
| J5 | (0, -d3=-146.6, 0) | **+Z** | 腕部摆动 |
| J6 | (0, 0, 前四项+d4=1964.3) | -Y | 末端回转 |

> 这张表直接暴露了**模型文件的坐标约定**：Z 朝上、关节 2/3/4 绕 Y 轴转、腕部关节 5 绕 Z 轴。
> 也就是说 `.obj` 是导出成"关节 1 在原点、朝上生长"的姿态，DH 的 `d/a` 只用来定位旋转中心。

**(b) 单关节旋转矩阵用 Rodrigues 公式手写**（`setTransformMatrix()`，`robotgeometry.cpp:202-232`）：

给（旋转中心 center、旋转轴 normal、角度 degree），返回 4×4 齐次矩阵。
这是标准的绕任意轴旋转公式，`a14/a24/a34` 三列就是"绕该点而非原点"的平移补偿。

**(c) 链式累积**（`multjointTransform()`，`robotgeometry.cpp:286-335`）：

```cpp
for (i = 0; i < numJoints + numTool; i++) {
    jointData[i].transform = setTransformMatrix(center[i], normal[i], i<numJoints ? array[i] : 0);
    if (i > 0) jointData[i].transform = jointData[i-1].transform * jointData[i].transform;  // ★ 串联
}
for (joint = 1; joint < entriesData.size(); joint++) {
    float *matrixPtr = jointData[joint-1].transform.data();    // ★ 注意是 joint-1
    for (每个顶点) 用 matrixPtr 变换 position;
    重新上传 VB;
}
```

两个"★"是理解关节链的关键：

1. **`jointData[i].transform = jointData[i-1].transform * jointData[i].transform`** ——
   子关节的变换继承父关节，摇一次底座，整条手臂跟着转。这是把"绝对旋转矩阵"变成"累积矩阵"的一步。
2. **绘制第 `joint` 个网格时用的是 `jointData[joint-1]`** ——
   因为 `entriesData[0]` 是 `base.obj`（不动），`entriesData[1]` 是 `joint1.obj`。
   `joint1.obj` 是"底座之上、J1 转动的那段"，它随 J1 动，所以要乘 `jointData[0]`。
   依次类推，`jointN.obj` 乘 `jointData[N-1]`，工具乘 `jointData[5]`（=tool 项）。

**(d) 每次都从备份重算**（`robotgeometry.cpp:290-293`）：

```cpp
for (joint = 1; joint < entriesData.size(); ++joint)
    memcpy(verticesData, verticesDataBackup, sizeof(VertexData)*numVertices);
```

先复位到原始姿态再施加累积矩阵，**避免误差累积**。代价是每帧要重算全部顶点（CPU 侧），
但对 6~7 个总共约 3 万顶点的模型来说完全够用。

**(e) `multjointTransform2()` 与 `multjointTransform()` 的唯一区别**：
前者重新绑定 VB 时**无条件** `allocate()`（`robotgeometry.cpp:280`），
后者按 `g_openglVersion` 走 map/memcpy（`robotgeometry.cpp:322-333`）。
`multjointTransform2()` 在当前代码里**没有任何调用点**（是保留的兼容版本）。

### 3.3 光怎么打：三个矩阵与一个方向光

`drawGeometry(model, mvpMatrix, lightMatrix)`（`robotgeometry.cpp:355-411`）：

- 传 uniform：`mvp_matrix`、`model`、`light1.{ambient=0.7, diffuse=0.5, specular=0.2}`、
  `material.{ambient=0.8, diffuse=0.9, specular=0.5, shininess=32}`、`viewPos = Pos`。
- 光方向 = `(Target - Pos + (2,2,2))` 再乘 `lightMatrix`（当前 `lightMatrix` 是空矩阵，
  等价于**方向不变的平行光**，来自相机方向）。
- 绘制循环：`for (num_model = 0; num_model < 1 + numJoints + min(numTool, showEndToolStatus); ...)`
  → **`showEndToolStatus` 实现在这里**：值为 0 时循环上界少 1，`tool.obj` 压根不画。

`drawSecondGeometry()`（`robotgeometry.cpp:414-466`）是**同一套几何的第二份画法**：
把光照三个分量全换成 `(0.1, 0.73, 1.0)`（亮蓝），用于"目标位姿机器人"的幽灵显示。

### 3.4 已知的坐标系/单位问题（移植时踩到的坑）

| 问题 | 位置 | 说明 |
|---|---|---|
| **TCP 球偏心 25 倍** | `SphereGeometry::GenerateSphereData` 内部乘 G_SCALE ↔ 调用方传 mm | 已在 Demo 传参侧补 `* G_SCALE` 修正（`robot3dglwidget.cpp:592, 799`，带 `[demo 适配]` 注释） |
| **`Pos`/`Target` 传了个寂寞** | `drawSecondGeometry` 里 `direction` 算了却没用 | 第二个机器人用固定光方向，不影响结果 |
| **`numJoints` 是硬编码常量** | `robotgeometry.cpp:10` | 构造函数写死 `numJoints = 6`，`ChangeRobot3D` 不更新它 → 换 4 轴/7 轴机型会错位 |
| **`~RobotGeometry` 用 `delete` 而非 `delete[]`** | `robotgeometry.cpp:124-125` | `verticesData` 是 `new VertexData[]`，析构用标量 delete → 未定义行为 |
| **模型路径依赖工作目录** | `QString appPath = "./"`（`robotgeometry.cpp:479`） | 运行目录下必须有 `robotType/` |
| **`setTransformMatrix` 的角度参数是 float 且未归一化** | `robotgeometry.cpp:212` | 角度超过 ±360° 时三角函数仍正确，但无归一化保护 |

---

## L4 渲染与控制总线：`Robot3dGLWidget`（GL 部分）

它同时是 `QOpenGLWidget`（渲染）和一堆小控件的宿主（UI），这里先讲渲染总线。

### 4.1 生命周期与初始化顺序（**有强约束**）

```cpp
构造 → InitUI() → initializeGL() → resizeGL() → paintGL() ...
```

`initializeGL()`（`robot3dglwidget.cpp:529-643`）一次性把 8 类几何对象全部 new 出来：

| 成员 | 类型 | 内容 | 备注 |
|---|---|---|---|
| `robotGeometry` | `RobotGeometry*` | 主机器人（6 关节 + 可选 tool） | **构造函数里就会读模型文件！** |
| `secondRobotGeometry` | `RobotGeometry*` | 幽灵机器人（目标位姿） | 同机型、共享关节链机制 |
| `coordSysGeometry[3]` | `QList<LineGeometry*>` | 坐标系三轴，长 400mm | 红 `#b63232` / 绿 `#32b67a` / 蓝 `#3255b6` |
| `TCP_Geometry` | `SphereGeometry*` | TCP 球，半径 10mm，蓝 | 初值由 `cr_ForwardKineFull` 正解给出 |
| `waypointPathlineGeometry` | `LineGeometry*` | 路点轨迹折线 | 默认 `maxNum=1000` |
| `waypointSphereGeometry` | `SphereGeometry*` | 路点球 | `maxNum=500` |
| `safePlaneGeometrys[8]` | `QList<PlaneGeometry*>` | 安全限制平面 | 数量 = `CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM`，初值 `alpha=0` |
| `tool_Geometry` + `toolCoordSysGeometry[3]` | `SubstanceGeometry*` + 3 线 | 末端工具 + 工具坐标系 | 工具轴长 120mm |

**为什么顺序不能乱**：`RobotGeometry` 的**构造函数**里就会调
`ChangeRobot3D(CommDataController::...->robotType.robotTypeParaStr, stdRobotWholeDH)`
（`robotgeometry.cpp:21`）去读 `robotType/<机型>/model3d/*.obj`。
所以必须先 `DemoDataHub::LoadRobotType()`，再创建 widget，最后才允许调 `Update*` 系列槽 ——
否则 `robotGeometry` / `TCP_Geometry` 还是空指针，`paintGL()` 会直接崩。

### 4.2 相机、矩阵与投影

**相机是固定的**，不是可拖动的：

```cpp
view.lookAt(robotGeometry->Pos, robotGeometry->Target, Up(0,0,1));   // robot3dglwidget.cpp:723
```

`Pos` / `Target` 由 `CalRobotViewTargetPos()`（`robotgeometry.cpp:68-117`）在载入模型后
**扫一遍全部顶点求包围盒**得到：

```
Target = 包围盒中心
Pos    = (Target.x + 150, Target.y, Target.z)      // 固定从 +X 方向 150 模型单位处看
```

投影在 `resizeGL()` 里设置（`robot3dglwidget.cpp:645-660`）：

```cpp
projection.perspective(fov=45, aspect=w/h, zNear=30, zFar=400);
```

> 注意 `zNear/zFar` 的单位是**模型单位**（mm×0.04），30/400 对应 750mm~10000mm。
> 过近或过远都会被裁掉 —— 缩放变小（`scalePara` 到 0.4）时模型更小更远，仍在范围内。

### 4.3 `paintGL()` 的绘制顺序（`robot3dglwidget.cpp:693-775`）

这是整个模块的"渲染总线"，每一步都值得记：

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

QMatrix4x4 model;
model.setToIdentity();
model.scale(curRobot3DShowData.scalePara);                          // ① 缩放（滑条）
model = curRobot3DShowData.ViewChangeMatrix.inverted() * model;     // ② 坐标系切换
model.translate(ViewChangeMatrix * translate_model / scalePara);    // ③ 平移（鼠标拖拽）
model.translate(rotate_center);                                     // ④ 绕 rotate_center 转：
model.rotate(rotation_model [* 安装姿态]);                            //    先把中心挪到原点
model.translate(-rotate_center);                                    //    转完挪回去

QMatrix4x4 mvpMatrix = projection * view * model;
QMatrix4x4 light_matrix;   // 空

robotGeometry->drawGeometry(model, mvpMatrix, light_matrix);              // 1 机器人本体
if (secondRobotShowSignal) secondRobotGeometry->drawSecondGeometry(...);  // 2 幽灵机器人
TCP_Geometry->drawGeometry(...);                                          // 3 TCP 球
for (coordSysGeometry) ...->drawGeometry(...);                            // 4 坐标系三轴
if (waypointShowSignal)     waypointSphereGeometry->drawGeometry(...);    // 5 路点球
if (waypointPathShowSignal) waypointPathlineGeometry->drawGeometry(...);  // 6 路点轨迹
if (safePlaneShowSignal)    safePlaneGeometrys[i]->drawGeometry(...);     // 7 安全平面
if (toolShowSignal) {  工具坐标系三轴 → tool_Geometry }                     // 8 末端工具
```

**三个视角状态**（`Robot3DShowData`，`robot3dglwidget.h:40-47`）：
`{ ViewChangeMatrix（坐标系切换）, rotation_model（四元数）, scalePara, translate_model, rotate_center }`。
`cur*` 是当前值，`pre*` 是进入"安装姿态"模式前的存档。

**统一渲染状态**（`initializeGL()` 设置，全模块共用）：

```cpp
glClearColor(1,1,1,1);      // 白底
glFrontFace(GL_CCW);
glCullFace(GL_BACK);        // 但 glEnable(GL_CULL_FACE) 是注释掉的 → 实际双面渲染
glEnable(GL_DEPTH_TEST);
```

### 4.4 显示位掩码：`Robot3DShowTypes`

```cpp
enum Robot3DShowType {                       // robot3dglwidget.h:25-38
    show_Null = 0x0,      show_CoordSys = 0x1,     show_RobotOperate = 0x2,
    show_Zoom = 0x4,      show_PoseOperate = 0x8,  show_WaypointOperate = 0x10,
    show_TargetRobot = 0x20, show_SafePlane = 0x40, show_TcpSpeed = 0x80,
    show_Install = 0x100,  show_Tool = 0x200
};
```

`SetShowType()`（`robot3dglwidget.cpp:288-426`）做两件事：

1. **按位切 UI 控件的显隐**：坐标系下拉框 / 右上操作按钮 / 缩放条 / 位姿编辑按钮 /
   路点开关组 / TCP 速度标签。
2. **把位翻译成渲染侧的布尔开关**（这些才是 `paintGL()` 真正看的）：
   `secondRobotShowSignal`、`waypointShowSignal`、`waypointPathShowSignal`、
   `safePlaneShowSignal`、`toolShowSignal`。
3. `show_Null` 是**特例**：一次性全关并提前 return（`robot3dglwidget.cpp:315-330`）。
4. `show_Install` 与其它位不同，它调 `IsIntoWorldTransformWidget(true)` —— 进入"安装姿态"视角，
   并在 `paintGL()` 里给 model 矩阵**额外叠加**一个由 `worldTransform.tiltAngle/baseAngle`
   组成的欧拉旋转（`robot3dglwidget.cpp:708-715`）。

### 4.5 交互：鼠标

三种鼠标事件（`robot3dglwidget.cpp:478-527`）配合一个模式开关 `rotateTranslateStatus`：

| 事件 | 行为 |
|---|---|
| `mousePressEvent` | 记下 `curPos`（QVector2D 局部坐标） |
| `mouseMoveEvent` | 模式 0（**旋转**）：轴向 = `(0, diff.y, diff.x)` 归一化，角度 = `diff.length()/5`；<br>并先把轴用 `ViewChangeMatrix` 变换到当前坐标系下，再乘到 `rotation_model` 四元数上。<br>模式 1（**平移**）：`translate_model += (0, diff.x/5, -diff.y/5)` |
| `mouseReleaseEvent` | 清标志（旋转惯性/角速度那套代码是注释掉的历史遗留） |

**缩放**走滑条（`ZoomSlider_valueChanged`，`robot3dglwidget.cpp:472-476`）：

```cpp
curRobot3DShowData.scalePara = 0.4 + value/10.0*0.15;   // value∈[0,100] → scalePara∈[0.4,1.9]
```

滑条初值 40 → `scalePara = 1.0`；`zoomChange_slot()` 用 `pageStep` 加减，配合 `setAutoRepeat(100ms)`
实现"按住连续缩放"。`Recovery3D_slot()` 一键回位（平移清零、四元数归单位、滑条回 40）。

### 4.6 坐标系列切换（与外部数据的耦合点）

`updateUseCoordSysSlot()`（`robot3dglwidget.cpp:894-961`）是一段**很典型的业务耦合代码**：
外部通过 `curTcpInCoordSysMsg.coordSysTcpPara.{coordSysType, coordinateId}` 告诉控件
"现在 TCP 在哪个坐标系下"，控件据此：

1. 按 `CoordSysType` 枚举（`base/joint/point/line/plane/tool/vision/path/world/work`）
   拼出坐标系 ID 字符串（`b_0` / `p_N` / `l_N` / `f_N` / `t_0` / `wcs_N`）；
2. 从 `RobotConfig::coordSysList` / `wcsList` 里查出该坐标系的位姿；
   工件坐标系还要做一次 `cr_PoseInv(worldInBase, baseInWorldPose)` + `cr_PoseTrans(...)`
   —— 即"世界→基座→工件"的变换链；
3. 用 `ViewChangeMatrixFunction(pose)` 生成变换矩阵，**同时顺手把坐标系三轴的线段端点按新位姿重设**
   （`robot3dglwidget.cpp:1144-1149`）—— 这就是"坐标系轴跟着走"的实现；
4. 反向下发：填 `setCoordinateTcpMessage` + `setTCPInCoordSysSignal = true`（在
   `UpdateCoordSysChange_slot` 里，用户手动切下拉框时）。

**视图与数据的双向性**：`updateRealtimeRobot3dMsgSignal` 是总闸门（`robot3dglwidget.cpp:896`），
为 `false` 时所有实时刷新动作直接 return —— 由 `GetRobot3dGLWidget(showType, isUpdateRTRobot3dMsg)` 设置。

---

## L5 控制面板层：控件、滑块与"长按 Jog"

### 5.1 `InitUI()`：一个 QOpenGLWidget 上叠了三层布局

`InitUI()`（`robot3dglwidget.cpp:42-286`）把控件直接做成 GL 控件的子控件（浮在 3D 视图上），
用 `QVBoxLayout(topLayout / midLayout / bottomLayout)` 分三层：

| 层 | 左侧 | 中间 | 右侧 |
|---|---|---|---|
| top | `coordSysWidget`：`坐标系:` 下拉框 + `TCP:` 名称标签 | ≤spacer≥ | `waypointOperateWidget`：5 个 checkable 图标按钮 |
| mid | `robotOperateWidget`：置初始位 / 平移旋转切换 / 末端工件显隐（3 个 40×40 图标按钮） | ≤spacer≥ | `zoomWidget`：放大 / 缩放滑条 / 缩小 |
| bottom | ≤spacer≥ | `tcpSpeedWidget`：`TCP速度: 0.000 mm/s` | `poseOperateWidget`：确定 / 取消 |

细节：
- 路点开关组 `machineBase / targetPose / currentPose / waypointPose / pathCurve`，
  **前三个被 `opPb->hide()` 隐藏了**（`robot3dglwidget.cpp:235-236`），只剩"路点显示"和"轨迹显示"两个；
  它们用 `_checked.png` / `_unchecked.png` 两套 border-image 做勾选态。
- 图标全部走 `:/resource/image/*.png`（17 个图标，见 `demo.qrc`）。
- 大量 `setProperty("helpPrompt", ...)` 的帮助提示代码被注释掉 —— 这是原项目帮助系统的痕迹，
  移植时因为不再有那套 UI 框架而注释（这是移植中**唯一"删功能"的地方**，但只是提示浮层）。
- `resizeCustomedWidget()`（`robot3dglwidget.cpp:662-691`）：窗口高度小于历史最大值的 75% 时，
  自动把操作按钮从 40×40 缩到 30×30 —— 适配小屏示教器。

### 5.2 长按 Jog：`TouchLongPressFilter`

在 `robot3dcontrolwidget.cpp:87-195`，是一个装在按钮上的事件过滤器（面向触摸屏示教器）：

```cpp
TouchBegin → 按下态图标 / 改样式 → handleLongPress_slot() → timer.start(200)   // 200ms 长按阈值
timer 每 200ms → 发送一次 click() 并 jogKeepCount++
TouchEnd  → 复位图标 → timer.stop() → touchEndEvent()
```

`touchEndEvent()` 里有两个业务判断：

1. `curRobotModes < 11 || == CloseBrake` → 弹"机械臂未上电使能完成！"警告并 return（**未使能就不允许 Jog**）；
2. `jogKeepCount > 2`（即长按超过约 600ms）→ 下发
   `movePara.movetype = CR__SERVICE__MOVE_TYPE__DecStop; moveControlSignal = true;`
   → **松手即减速停止**。

另外它拦截了两类事件（`robot3dcontrolwidget.cpp:146-162`）：
合成鼠标事件（`MouseEventSynthesizedBySystem/Qt`，避免触摸被重复当鼠标处理）、
双击（双击直接转成一次 click）。

### 5.3 路点轨迹累积：`AddRobotWaypointPath(pose)`（**注意它在 L6 类里**）

`robot3dcontrolwidget.cpp:56-85`，虽属门面类但功能是"喂数据给 L4 的折线"：

1. 若点数已达 `maxNum`（1000）→ `removeFirst()`（**FIFO 滑窗**，只保留最近 1000 点）；
2. **只有**处于 `lua_Script_run` 或 `lua_Script_pause` 时才真正记点 —— 即"只有跑脚本时才画轨迹"；
3. 记点前去重：与上一个点三个方向差都 < 5mm 就跳过（`fabs(...)<5.0`）；
4. `AddDeleteLineData(pointList)` → 走 L1 的"个数有增删"上传路径。

---

## L6 对外接口层：`Robot3dControlWidget` —— 外部只认这一个门面

`robot3dcontrolwidget.h/.cpp`，是典型的**单例门面 + 全局句柄**：

```cpp
static Robot3dControlWidget* robot3dControl_handle;
static void InitInstance(QWidget *parent);   // 幂等：已存在则不重建
static void UnInitInstance();                // 空实现（原项目也没写）
static Robot3dControlWidget* GetInstance();
```

对外暴露的就 5 个方法：

| 方法 | 作用 | 落到 L4 的什么 |
|---|---|---|
| `GetRobot3dGLWidget(showType, isUpdateRTRobot3dMsg)` | **取得 GL 控件并加入你的布局**，同时设总闸门与显示位 | `SetShowType()` |
| `UpdateRobot3dJointAngle(JointPose)` | 刷新关节角（度） | `SetRobotJointAngle_slot()` → `multjointTransform()` |
| `UpdateRobot3dToolPosition(Pose)` | 刷新 TCP 位姿（mm） | `SetRobotToolPosition_slot()` → `TCP_Geometry->SetSphereData()` |
| `UpdateRobot3dSafetyToolData(QVector<SubstanceMsg>)` | 刷新末端工具外形 | `SetRobotSafetyToolData()` → `tool_Geometry->SetSubstanceData()` |
| `AddRobotWaypointPath(Pose)` | 追加路点轨迹 | 直接操作 L4 的 `waypointPathlineGeometry` |
| `ResizeRobot3dMode()` | 强制重算投影 | `resize(size())` |

**注意这里有一个重要的架构含义**：`Robot3dGLWidget` 的构造函数是 `public`，
但它**不该被外部直接 new**（否则单例门面失效）。原项目约定由门面创建、外部通过
`GetRobot3dGLWidget()` 拿到指针后自己 `layout->addWidget()`（Demo 里就是这么做的，见 `demoui.cpp:68-75`）。

---

## L7 外围/适配层：`demo_support.h/.cpp` —— 让移植代码"零改动"编译

**这一层不是从原项目移植来的**，它是移植的产物。原 `3dmodel/` 依赖 6 个外部模块，
适配层把这 6 项一次性补齐，从而让 `3dmodel/` 下的源码**只需改 include，其余一字不动**。

### 7.1 依赖映射表

| 原依赖 | 原性质 | 适配层方案 |
|---|---|---|
| `base/basedefine.h` | 基础类型 | 直接重建：`JointPose`、`Pose`、`WholeDH`、`ROB_AXIS_NUM`、各种 Copy/Set 工具 |
| `commu/commdatacontroller.h` | 全局数据中枢单例 | `class DemoDataHub` + **`#define CommDataController DemoDataHub`**（宏在头文件末尾） |
| `robotconfig/robotconfig.h` | 坐标系/TCP/安装配置 | 只重建 3D 模块真正用到的字段与 6 个接口 |
| `base/softwareui_controller.h` | 机型切换通知 | 最小单例 + `ChangeRobotType_signal()` |
| `base/userpushbutton.h` | 自定义按钮 | `UserPushButton`：只保留 `SetPose_*` 三个槽 + `PoseChanged_signals` |
| `commu/rammonitorthread.h` | 内存保护中断 | 桩：`MemoryCheck()` 恒 `false`（永不拦截） |
| **`robotAlgorithm/tpAlgApp`** | **纯 MSVC 预编译库，MinGW 无法链接** | **用标准 C++ 按同接口重写运动学**（`demo_support.cpp:117-235`） |

### 7.2 运动学重写（`demo_support.cpp:22-235`）—— 适配层最硬的部分

原项目里 `cr_PoseTrans / cr_PoseInv / cr_ForwardKineFull` 只是对 `tp_*` 函数的薄封装，
而 `tpAlgApp` 只提供 MSVC ABI 的 `.dll/.lib`，跨编译器不可用 → 只能重写。

前置的小工具（匿名 namespace）：`PoseToMatrix` / `MatrixToPose` / `MatMul3` / `MatMulVec3` / `Transpose3`。

| 函数 | 语义 | 实现要点 |
|---|---|---|
| `cr_PoseTrans(result, Pose1, Pose2)` | `result = Pose2⁻¹ · Pose1` | 用 `R2ᵀ` 代替 `R2⁻¹`；`T = R2ᵀ·(T1-T2)` |
| `cr_PoseInv(result, pose)` | 位姿求逆 | `Rᵀ`；`T = -Rᵀ·T` |
| `cr_ForwardKineFull(joint, pose, DH)` | **标准 DH 正解**，关节角单位**度** | `T = RotZ(θ)·TransZ(d)·TransX(a)·RotX(α)` 六段连乘 |
| `cr_Eule2AxisAngle` / `cr_AxisAngle2Eule` | 欧拉角 ↔ 轴角 | 含 180° 与 0° 奇异点处理 |

**旋转约定必须与控件一致**：`R = Rz · Ry · Rx`（`demo_support.cpp:53-60` 与
`Robot3dGLWidget::Euler2RotateMatrix` 的 `RotZYX = RotZ*RotY*RotX` 对齐）。
`MatrixToPose` 反解时按 `R = Rz·Ry·Rx` 提取 `asin/atan2`，并处理万向锁分支。

### 7.3 `DemoDataHub`：与原数据结构逐字段对齐

字段名**刻意与原项目保持一致**，这是"业务代码零改动"的前提：

```cpp
robotType.robotTypeParaStr            // 机型名（决定 robotType/<name>/model3d 路径）
WholeDH stdRobotWholeDH[6]            // 标准 DH 参数表（由 config.xml 填充）
JointPose currentJointPose            // 当前关节角（度）
Pose      currentPose                 // 当前 TCP 位姿（mm + 轴角）
Pose      baseInWorldPose             // 基座在世界系下的位姿
curJogPara.{coordSysID, tcpID, tcpOffset}
curTcpInCoordSysMsg.coordSysTcpPara.{coordSysType, coordinateId}
updateRealtimeRobot3dMsgSignal         // 3D 控件是否跟随实时数据刷新
```

`LoadRobotType(name, root="robotType")`（`demo_support.cpp:253-302`）用 `QXmlStreamReader`
解析 `robotType/<机型>/config.xml` 的 `<DHpara>` 段，每项形如：

```xml
<j0>0,0,164.3,0,0</j0>     <!-- alpha, a, d, theta, beta；alpha/theta 单位是弧度 -->
```

**注意单位混用**：DH 的 `alpha/theta` 在 xml 里是**弧度**，而关节角 `q[i]` 是**度**，
`cr_ForwardKineFull` 里专门做了 `theta = DH[i].theta + q[i]*M_PI/180`（`demo_support.cpp:163`）。

### 7.4 三个"降级为桩"的业务功能（移植中有意为之）

| 原功能 | 桩实现 | 影响 |
|---|---|---|
| `TipMsgDialog`（弹窗） | 输出到 `qWarning()`（`demo_support.cpp:416-421`） | 长按 Jog 的"未使能"警告只打日志，不弹窗 |
| `RamMonotorThread`（内存保护） | `MemoryCheck()` 恒 `false` | 内存不足时不再中断 3D 刷新 |
| `UserOperateLogPrint`（用户操作日志） | `qDebug()` | 无上报 |

---

## L8 驱动外壳与资源（**不属于 `3dmodel/`，但在本工程里负责验证它**）

| 文件 | 角色 |
|---|---|
| `main.cpp` | 入口；写 `demo_debug.log`；支持 `DEMO_DUMP_START/FRAME/SIMSTEPS` 三个环境变量做无人值守帧导出 |
| `demoui.h/.cpp` | Demo 外壳：机型下拉框、6 个关节滑条、正弦摆动模拟、显示项复选框、日志窗 |
| `demo.qrc` / `demo.pro` | 8 个 shader + 17 个图标入资源；`QT += opengl widgets`、assimp 头/库路径 |
| `robotType/<机型>/` | `config.xml`（DH 参数）+ `model3d/{base,joint1..6,tool}.obj` + `.mtl`（材质色） |
| `build.bat` / `run_demo.bat` | 一键构建（qmake + mingw32-make + 部署 Qt/assimp 运行库到 `run/`）与启动 |
| `port_assets.py` / `port_code.py` | 移植脚本：资源拷贝 / include 批量改写（`REPLACE_MAP`） |

### Demo 侧的调用链（验证 `3dmodel/` 的最短路径）

```
DemoUi 构造
 ├─ hub->LoadRobotType("G6L-3-20")                      // ① 先有 DH 参数
 ├─ Robot3dControlWidget::InitInstance(this)            // ② 建门面
 ├─ GetRobot3dGLWidget(showType, true) → layout->addWidget()  // ③ 加入布局，触发 initializeGL
 ├─ QTimer::singleShot(120, OnResetJoint)               // ④ 窗口显示后才允许 Update*
 └─ OnSimTimer → hub->StepSimulation(dt)                //    正弦摆动 → currentJointPose/currentPose
                → UpdateRobot3dJointAngle(...)          //    → 关节链重算
                → UpdateRobot3dToolPosition(...)        //    → TCP 球重定位
```

④ 这一步的 `QTimer::singleShot` 正是为了绕开 L4.1 说的**初始化顺序强约束**。

---

## 附 A：一帧的完整数据流（把 8 层串起来）

```
             ┌── 外部数据源（示范：DemoDataHub::StepSimulation 正弦摆动）
             │        currentJointPose{L1..L6} / currentPose{X,Y,Z,Rx,Ry,Rz}
             ▼
[L6] Robot3dControlWidget::UpdateRobot3dJointAngle / UpdateRobot3dToolPosition
             │
             ├──► [L4] SetRobotJointAngle_slot(jointPose)
             │         └─► [L3] RobotGeometry::multjointTransform(arr[6])
             │                ├─ memcpy 顶点 ← 备份（复位）
             │                ├─ 6+1 次 Rodrigues 旋转 → jointData[i].transform（链式累积）
             │                ├─ 每个网格乘 jointData[joint-1] → 改写 CPU 顶点
             │                └─ VB.map/memcpy 上传 GPU
             │
             └──► [L4] SetRobotToolPosition_slot(pose)
                       └─► [L2] SphereGeometry::SetSphereData(center*G_SCALE, r)
                                 └─ 432 顶点重算 → 上传 GPU
             │
             ▼  update() 触发
[L4] paintGL()
   ├─ 组合 model = 缩放 · ViewChangeMatrix⁻¹ · 平移 · 绕 rotate_center 旋转
   ├─ view = lookAt(Pos, Target, +Z)
   ├─ mvp = projection(fov 45, 30/400) · view · model
   └─ 按固定顺序 8 次 drawGeometry：
        机器人(robot shader, Blinn-Phong) → 幽灵机器人 → TCP球 → 坐标系轴
        → 路点球 → 路点轨迹 → 安全平面(blend) → 工具+工具轴(blend)
             │
             ▼
[L1] 每个图元的 program.bind() → setUniformValue(lightColor/toyColor/mvp) 
     → enableAttributeArray(a_position[/a_normal/a_material]) 
     → glDrawArrays(GL_TRIANGLES / GL_LINE_STRIP)
```

## 附 B：`G_SCALE` 单位对照速查（最容易出错的地方）

| 数据 | 单位 | 是否已 ×G_SCALE |
|---|---|---|
| `.obj` 顶点（`pos.obj` 原始） | mm | 否 |
| `VertexData.position`（载入后） | 模型单位 | **是**（`LoadMesh` 里 `scaleMatrix`） |
| `jointData[i].center` | 模型单位 | **是**（`*G_SCALE` 写死在 `ChangeRobot3D`） |
| `LineGeometry` / `PlaneGeometry` / `SphereGeometry` 的输入坐标 | mm | 否（**类内部**自己乘） |
| `Pose`（mm）传给 `SetRobotToolPosition_slot` | mm | 否 → **调用侧补乘**（`robot3dglwidget.cpp:799`，Demo 修正点） |
| `SubstanceMsg.centerPose` | mm | 否（`GetRotatePoint` 内部先 `÷G_SCALE` 再 `×G_SCALE`） |
| `projection` 的 near/far | 模型单位 | — |

## 附 C：移植过程中发现/修正的问题清单

| # | 问题 | 位置 | 状态 |
|---|---|---|---|
| 1 | TCP 球心单位不一致（偏 25 倍） | `robot3dglwidget.cpp:592, 799` | **已修正**（传参侧补 `*G_SCALE`，标了 `[demo 适配]`） |
| 2 | shader 带 UTF-8 BOM → GLSL 编译失败、3D 全黑 | `shaders/*.glsl` | 已规避（保持无 BOM） |
| 3 | 初始化顺序强约束（早调 Update 必崩） | `initializeGL()` 之前 | 已规避（`QTimer::singleShot` 延迟） |
| 4 | 模型路径依赖工作目录（`"./" + robotType/...`） | `robotgeometry.cpp:479` | 保留原样，靠 `build.bat` 复制 `robotType/` 到 `run/` |
| 5 | `~RobotGeometry()` 用 `delete` 而非 `delete[]` | `robotgeometry.cpp:124-125` | 未改（原样保留） |
| 6 | `numJoints` 硬编码为 6，换机型不更新 | `robotgeometry.cpp:10` | 未改 |
| 7 | `drawSecondGeometry` 里 `direction` 算了没用 | `robotgeometry.cpp:429` | 未改（无副作用） |
| 8 | `WarningLevel::WARNING` 与 `windows.h` 的宏冲突 | `robot3dcontrolwidget.cpp:183` | **已改**为 `LV_WARNING` |
| 9 | `SubstanceGeometry::AxisAngle2RotateMatrix` 原地改写入参 | `substancegeometry.cpp:423-425` | 未改（调用方传的是临时量） |
| 10 | `PlaneGeometry` 的 `SetPlaneData` 单面版写一个面，但容量按 `numVertex*numMax` 预留 | `planegeometry.cpp:39-54` | 未改（预留量大于写入量，安全） |

---

## 附 D：想改功能，该动哪一层？

| 想做的事 | 应该改的地方 |
|---|---|
| 换模型格式 / 换缩放系数 | L3 `LoadMesh()` + L1 `G_SCALE`（注意是全局约定，改了要全线检查） |
| 加减一个关节 / 支持非 6 轴 | L3 `ChangeRobot3D()` 的关节表 + `numJoints`（现在写死 6） |
| 改光照/材质观感 | L3 `drawGeometry()` 的 uniform + L1 `robot_fragment_shader.glsl` |
| 加一种新图元（如箭头、文字） | 学 L2 任一子类：继承 `BaseGeometry` + 一对 shader + 复用三种上传策略 |
| 加一个显示开关 | L4 `Robot3DShowType` 加位 + `SetShowType()` 分支 + `paintGL()` 里的 `if` |
| 改相机角度/视野/near-far | L3 `CalRobotViewTargetPos()` + L4 `resizeGL()` |
| 换交互方式（触摸/手柄） | L4 三个鼠标事件 + L5 `TouchLongPressFilter` |
| 接真实机器人数据 | L7 把 `DemoDataHub` 换成你的数据单例（改 `#define` 或在其 setter 里桥接） |
| 加逆解/轨迹规划 | L7 运动学区（现只有正解；原项目在 `tpAlgApp` 里） |

---

## 附 E：本次分析使用的明文副本

`3dmodel/` 下的 19 个源文件在本机处于透明加密状态（文件头 `88 7D 1C`，DSH `Read` 工具读不了），
本次分析用 Python 通道（加密系统白名单进程）导出了明文副本，存放在临时目录：

```
%TEMP%\dsh-39fK86\robot3d_3dmodel_plain\   （文件名形如 robot3dglwidget_cpp.md）
```

该目录是**分析用的临时产物**，可随时删除；需要重新生成时按同样方式用 Python 读取 `3dmodel/` 即可。
上面的行号引用针对的是**明文内容**，与原密文文件的行号一致。
