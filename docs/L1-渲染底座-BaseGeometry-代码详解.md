# L1 渲染底座：`BaseGeometry` 代码级实现详解

> 对象：`3dmodel/basegeometry.h`（108 行）+ `3dmodel/basegeometry.cpp`（25 行）+ `shaders/`（8 个 GLSL）
> 定位：整个 3D 模块的**唯一地基** —— 它把"CPU 顶点数据 → GPU 顶点缓冲 → 着色器 → glDrawArrays"
> 这条链路固化成一基类，L2 的四个图元类和 L3 的 `RobotGeometry` 都只在它上面填"顶点怎么生成"和"怎么画"。

---

## 1. 文件全景

`basegeometry.cpp` 只有 25 行，但信息密度极高 —— 逐行看：

```cpp
1  #include"basegeometry.h"
2  int g_openglVersion=-1;                                    // ① 全局变量的唯一定义处
3  BaseGeometry::BaseGeometry()
4      : vertex_index_Buf(QOpenGLBuffer::IndexBuffer)         // ② 唯一一个初始化列表项
5  {
6      initializeOpenGLFunctions();                           // ③ 取当前上下文的 GL 函数表
7
8      vertex_data_Buf.create();                              // ④ 真正创建两个 GL buffer 对象
9      vertex_index_Buf.create();
10 }
11 BaseGeometry::~BaseGeometry()
12 {
13     vertex_data_Buf.destroy();
14     vertex_index_Buf.destroy();
15 }
16
17 bool BaseGeometry::initShaders()
18 {
19     return false;                                          // ⑤ 基类"空实现"：明确返回失败
20 }
21
22 void BaseGeometry::drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
23 {
24
25 }                                                          // ⑥ 基类"空实现"：什么都不做
```

下面把这 6 个标记点逐个展开。

---

## 2. ① `g_openglVersion`：全模块共享的版本号

### 定义与声明

```cpp
// basegeometry.h:17
extern int g_openglVersion;

// basegeometry.cpp:2
int g_openglVersion = -1;      // ← 唯一定义，初值 -1
```

**注意它定义在 `.cpp` 而不是头文件里**，头文件只 `extern` 声明 —— 这是 C++ 里避免"多重定义"的经典做法。
初值 `-1` 很重要：任何"还没测到版本"的情况下，所有 `if (g_openglVersion > 2)` 判断都走 **else 分支（安全的 `allocate` 路径）**。

### 谁写它：`Robot3dGLWidget::GetOpenGLVersion()`

```cpp
// robot3dglwidget.cpp:1218-1246
const GLubyte* OpenGLVersion = glGetString(GL_VERSION);      // 形如 "4.6.0 NVIDIA 536.23"
QString version; version.sprintf("%s", OpenGLVersion);
for (int j = 0; j < version.length(); j++) {
    if (version[j] > '0' && version[j] < '9') {              // 找第一个 '1'~'8'
        g_openglVersion = QString(version[j]).toInt();
        break;
    }
}
```

解析方式很粗糙 —— **取版本串里第一个 `'1'`~`'8'` 的字符当主版本号**：
`"4.6.0 ..."` → 4；`"OpenGL ES 3.2 ..."` → 3（`'O'` 不是数字，跳过）；`"2.1 ..."` → 2。
只要版本串里出现过 `0` 或 `9` 就被忽略（所以理论上 `"OpenGL 2.0"` 里的 `0` 不影响，但 `"9.x"` 会被跳过）。

### 它唯一的用途：挑 VBO 上传策略

全模块只有一处消费它 —— 见第 6 节的三种上传策略。
**它不参与任何渲染分支**（不影响画什么、怎么画），只影响"数据怎么送上去"。

---

## 3. ② `vertex_index_Buf`：一个从创建到销毁都没被用过的缓冲

```cpp
BaseGeometry::BaseGeometry()
    : vertex_index_Buf(QOpenGLBuffer::IndexBuffer)     // ← 关键：必须显式指定类型
{
```

### 为什么这行是必需的（一个容易踩的 Qt 细节）

`QOpenGLBuffer` 的构造函数是 `QOpenGLBuffer(QOpenGLBuffer::Type type = VertexBuffer)`。
**默认是顶点缓冲**。如果漏掉这个初始化列表项，`vertex_index_Buf` 会被当成 `VertexBuffer` 创建 ——
虽然本模块从不用它，但这属于"语义错误"。

所以这一行是**整份代码里唯一一处显式区分 buffer 类型的地方**，值得记住。

### 然而它从未被使用

```cpp
BaseGeometry 的成员：QOpenGLBuffer vertex_index_Buf;
```

`grep vertex_index_Buf` 的全部命中只有 3 处：
`basegeometry.h:99`（声明）、`basegeometry.cpp:4`（构造）、`basegeometry.cpp:14`（析构）。

配套的还有两个同样空转的成员：

```cpp
QVector<GLuint> vertex_index;   // basegeometry.h:97  从未 push_back 过
int num_vertex_index;           // basegeometry.h:100 从未赋值过
```

**原因**：所有绘制路径用的都是 `glDrawArrays(...)`，没有一处 `glDrawElements(...)` ——
这一点在每个子类的 `drawGeometry` 里都能看到，而且原代码里 `glDrawElements` 都被注释掉了，例如：

```cpp
// linegeometry.cpp:133-135
//需要改成绘制直线GL_Lines GL_TRIANGLE_STRIP GL_LINES
//glDrawElements(GL_LINE_STRIP, num_vertex_index, GL_UNSIGNED_INT, 0);
glDrawArrays(GL_LINE_STRIP, 0, num_vertex_data);
```

**意味着**：这套渲染管线是"**顶点数组膨胀式**"的 —— 不复用顶点，每个三角形独立列 3 个顶点。
代价是显存和上传量偏大（例如一个球 432 个顶点其实只需要约 144 个独立顶点），
好处是**顶点可以逐帧自由重写**（L3 的关节变形正是靠这个：同一个顶点数组每帧被重新变换、重新上传）。

> 结论：`vertex_index_Buf` / `vertex_index` / `num_vertex_index` 是"预留了但没用上的索引管线"，
> 属于死代码，删除它们不影响功能（每个几何体还能省一个 GL buffer 对象）。

---

## 4. ③ `initializeOpenGLFunctions()`：整个 L1 的上下文契约

```cpp
BaseGeometry::BaseGeometry()
{
    initializeOpenGLFunctions();     // basegeometry.cpp:6
```

`initializeOpenGLFunctions()` 是 `QOpenGLFunctions` 提供的，它做两件事：
**取 `QOpenGLContext::currentContext()`，并把该上下文的 GL 函数表指针缓存到本对象里**。
之后所有 `glDrawArrays` / `glLineWidth` / `glEnable` 调用都走这个缓存指针。

### 由此推导出一条硬性架构约束

`QOpenGLFunctions` 只能解析**当前已 makeCurrent 的上下文**。而 `BaseGeometry` 的构造函数就直接用了它
（还有 `create()` 也要求有当前上下文）。

所以**所有几何对象必须在 GL 上下文已为 current 时构造**。实际代码确实是这么做的 ——
全部对象都在 `Robot3dGLWidget::initializeGL()` 里 new 出来（此时 Qt 已经 makeCurrent）：

```cpp
// robot3dglwidget.cpp:529-643（节选，全部 8 类对象都在这个函数里创建）
robotGeometry            = new RobotGeometry();            // 541
secondRobotGeometry      = new RobotGeometry();            // 548
LineGeometry* lineGeometry = new LineGeometry(coordsysLineMsg);   // 575（×3）
TCP_Geometry             = new SphereGeometry(TCP_sphereMsg);     // 593
waypointPathlineGeometry = new LineGeometry(tmpWaypoints, ...);   // 597
waypointSphereGeometry   = new SphereGeometry(sphereMsgs, 500);   // 602
PlaneGeometry* tmpPlaneGeometry = new PlaneGeometry(planeMsg);    // 611（×8）
tool_Geometry            = new SubstanceGeometry(substanceMsg);   // 621
```

**这条约束就是 README 第六节第 3 条"初始化顺序有强约束"的底层原因之一**：
如果外部在 `initializeGL()` 之前 new 图元（或调 `Update*` 导致图元被 new），
要么静默失败（`QOpenGLBuffer::create()` 返回 false、`isCreated()` 为 false），
要么拿不到 GL 函数表而崩溃。

### `initShaders()` 走的是另一条路

四个图元类的 `initShaders()` 里用的是 `program.addShaderFromSourceFile(...)` ——
这是 `QOpenGLShaderProgram` 的成员，**它自己会 makeCurrent 或要求上下文已 current**，
但同样必须在上下文可用时调用。所以每个几何对象的创建流程在 L4 里都是成对出现的：

```cpp
LineGeometry* lineGeometry = new LineGeometry(coordsysLineMsg);
lineGeometry->initShaders();          // ← 紧接着立刻初始化着色器，不延后
```

---

## 5. 数据结构：三套顶点格式与字节布局

`basegeometry.h` 里定义的**全部**结构体（这是 L2+L3 共用的数据字典）：

| 行号 | 名字 | 成员 | 字节数 | 用在哪 |
|---|---|---|---|---|
| 19 | `FeatureVertexData` | `QVector3D position` | **12** | L2 全部图元（线/面/球/体） |
| 65 | `VertexData` | `position`, `normal`, `material`（各 `QVector3D`） | **36** | L3 `RobotGeometry` 网格 |
| 24 | `SubstanceMsg` | `int substanceType` + `double centerPose[6]` + `float radius/length/width/height` + `QColor color` | ~72 | L2 `SubstanceGeometry` 的输入 |
| 35 | `SphereMsg` | `center`, `radius`, `color` | 24 | L2 `SphereGeometry` 的输入 |
| 42 | `LineMsg` | `startPt`, `endPt`, `color` | 28 | L2 `LineGeometry` 的输入 |
| 49 | `PointMsg` | `Pt`, `color` | 20 | **已定义但全模块未使用**（点渲染的预留） |
| 55 | `PlaneMsg` | `firstPt`, `secondPt`, `thirdPt`, `fourPt`, `color`, `alpha` | 56 | L2 `PlaneGeometry` 的输入 |
| 72 | `multiJointData` | `center`, `normal`, `QMatrix4x4 transform` | 88 | L3 关节表 |
| 79 | `MeshEntry` | `VertexData* verticesData` + `VertexData* verticesDataBackup` + `ushort numVertices` + `QOpenGLBuffer VB` | — | L3 每个部件一份 |

### `VertexData` = 正好对应 robot 着色器的 3 个 attribute

`QVector3D` 内部是 **3 个 `float`，无 padding → 严格 12 字节**。所以：

```
VertexData（36 字节）的 GPU 字节布局：
偏移 0  ┌──────────────┬──────────────┬──────────────┐
        │ position.x   │ position.y   │ position.z   │   ← attribute a_position (vec3)
偏移 12 ├──────────────┼──────────────┼──────────────┤
        │ normal.x     │ normal.y     │ normal.z     │   ← attribute a_normal   (vec3)
偏移 24 ├──────────────┼──────────────┼──────────────┤
        │ material.r   │ material.g   │ material.b   │   ← attribute a_material (vec3)
偏移 36 └──────────────┴──────────────┴──────────────┘   ← 下一个顶点
```

这个布局不是巧合 —— 它被 `RobotGeometry::drawGeometry` 的属性指针代码完全依赖：

```cpp
// robotgeometry.cpp:392-405
quintptr offset = 0;
int vertexLocation   = program.attributeLocation("a_position");
program.setAttributeBuffer(vertexLocation,   GL_FLOAT, offset, 3, sizeof(VertexData));  // offset=0
offset += sizeof(QVector3D);                                                           // → 12
int normalLocation   = program.attributeLocation("a_normal");
program.setAttributeBuffer(normalLocation,   GL_FLOAT, offset, 3, sizeof(VertexData));  // offset=12
offset += sizeof(QVector3D);                                                           // → 24
int materialLocation = program.attributeLocation("a_material");
program.setAttributeBuffer(materialLocation, GL_FLOAT, offset, 3, sizeof(VertexData));  // offset=24
```

**读这段代码要记住 `QOpenGLShaderProgram::setAttributeBuffer` 的 5 个参数语义**：

```cpp
setAttributeBuffer(location, type, offset, tupleSize, stride)
//                               ↑        ↑          ↑
//                     该属性在本顶点内的 分量个数   相邻顶点间隔
//                     字节偏移
```

**第一个参数 `offset` 是"字节偏移"而不是"顶点序号"** —— 这是最容易看错的地方。
因为 `sizeof(QVector3D) == 12 == sizeof(float)*3 ==` 前一个属性占的字节数，
所以"累加 `sizeof(QVector3D)`"这种写法在数值上恰好等于"累加前一个属性的字节宽度"，
代码看起来像是在数元素、实际是正确的字节偏移。

### `FeatureVertexData` 更简单，但 stride 依然写全

```cpp
// linegeometry.cpp:127-130（plane/sphere/substance 三处完全一样）
quintptr offset = 0;
int vertexLocation = program.attributeLocation("a_position");
program.enableAttributeArray(vertexLocation);
program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(FeatureVertexData));
```

这里 `tupleSize=3`、`stride=12`、`offset=0` —— 因为顶点只有位置一个属性，
所以 stride 恰好等于 `tupleSize*sizeof(float)`，**"紧密排列"**。
即使如此，代码仍老老实实写 `sizeof(FeatureVertexData)` 而不是 `0`，语义更清楚。

---

## 6. 三种 VBO 上传策略（底座的核心机制）

这是 L1 唯一"有技术含量"的部分，也是四个图元类里那段高度重复的模板代码的来源。

### 6.1 策略总表

| 场景 | 条件 | 实现 | 代码位置（每个类各一份） |
|---|---|---|---|
| **A. 首次分配** | 在构造函数里 | `create()` 已在基类做过 → `bind()` → `allocate(data, n*sizeof(V))` → `release()` | `linegeometry.cpp:8-11, 20-23`；`planegeometry.cpp:8-11, 24-27`；`spheregeometry.cpp:8-11, 23-26`；`substancegeometry.cpp:8-11, 23-26` |
| **B. 个数不变，只换数据** | 顶点数恒定 | `bind()` → `map(WriteOnly)` → `memcpy` → `unmap()` → `release()`（GL>2）<br>或 `allocate(data, n*sizeof(V))`（否则） | `linegeometry.cpp:31-49`；`planegeometry.cpp:36-54`；`spheregeometry.cpp:57-74`；`substancegeometry.cpp:56-72` |
| **C. 个数有增删** | 顶点数可变 | GL>2：同 B<br>否则：`destroy()` → `create()` → `bind()` → `allocate(...)` | `linegeometry.cpp:69-92`；`planegeometry.cpp:74-95`；`spheregeometry.cpp:76-99`；`substancegeometry.cpp:74-97` |

**命名约定**（四个类完全对称，看到名字就知道走哪条路）：

```
SetXxxData(...)           → 策略 B（个数不变）
AddDeleteXxxData(...)     → 策略 C（个数可能增删）
构造函数                   → 策略 A（首次分配）
```

### 6.2 策略 B/C 的统一代码骨架

```cpp
// 以 linegeometry.cpp:31-49 的 SetLineData(startPt, endPt) 为例
GenerateLineData(startPt, endPt);                    // ① CPU 侧重算顶点
num_vertex_data = vertex_data.size();                // ② 更新计数

vertex_data_Buf.bind();                              // ③ 绑定
if (g_openglVersion > 2) {
    auto ptrVertex = vertex_data_Buf.map(QOpenGLBuffer::WriteOnly);   // ④ 拿到写指针
    memcpy(ptrVertex, vertex_data.data(), sizeof(FeatureVertexData) * num_vertex_data);  // ⑤ 覆盖
    vertex_data_Buf.unmap();                         // ⑥ 提交
    vertex_data_Buf.release();
} else {
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data * sizeof(FeatureVertexData));  // ⑦ 重新分配
}
```

两条路径的**代价差别**：

- `map/unmap` 路径：**不重新分配显存**，只是拿到一段映射指针后 `memcpy` 覆盖已分配区域。
  省掉了驱动侧的内存管理与可能的同步开销 —— 这是它被优先使用的原因。
- `allocate` 路径：**每次都可能触发一次 `glBufferData`**（驱动可能重新分配显存、丢弃旧内容）。
  在频繁刷新的场景（关节联动每帧调一次）下更重。

### 6.3 关键前提：`map()` 返回的是"整个缓冲区"，所以必须预分配容量

`QOpenGLBuffer::map(WriteOnly)` 返回的是**整个 buffer 的起始指针**，长度 = 上次 `allocate` 的长度。
代码直接按 `sizeof(V)*num_vertex_data` 覆盖写入 —— 这只有在

```
num_vertex_data * sizeof(V)  ≤  buffer 已分配字节数
```

时才安全。所以"顶点数会变化"的几何对象在构造时**就按最大容量预分配**：

```cpp
// spheregeometry.cpp:15-27（路点球用这个构造，maxNum=500）
SphereGeometry::SphereGeometry(QVector<SphereMsg> sphereMsgsIn, int maxNum) : BaseGeometry()
{
    if (sphereMsgsIn.count() > 0) this->sphereMsg = sphereMsgsIn[0];
    this->maxNum = maxNum;
    GenerateSpheresData(sphereMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(nullptr, sizeof(FeatureVertexData) * numVertex * maxNum);   // ★ 只分配，不填数据
    vertex_data_Buf.release();
}
```

`allocate(nullptr, size)` 只分配空间、不传数据。三类图元的容量公式：

| 类 | 容量公式 | 大小常数 | 在 L4 里的实际取值 |
|---|---|---|---|
| `SphereGeometry(points, maxNum)` | `sizeof(FeatureVertexData) * 432 * maxNum` | `numVertex=432`（`spheregeometry.h:26`） | 路点球 `maxNum=500` → **2,592,000 B ≈ 2.47 MB** |
| `PlaneGeometry(points, numMax)` | `sizeof(FeatureVertexData) * 4 * numMax` | `numVertex=4`（`planegeometry.h:28`） | 安全平面用单面构造函数，不走这里 |
| `SubstanceGeometry(points, maxNum)` | `sizeof(FeatureVertexData) * 432 * maxNum` | `numVertex=432`（`substancegeometry.h:35`） | 工具用单条构造函数，不走这里 |

> 那 432 是哪来的：球面按 `angleSpan = 30°` 切分 → 纬向 6 带 × 经向 12 带 × 每格 6 顶点 = **432**。
> 这个数字在 `spheregeometry.h:26` 和 `substancegeometry.h:35` 里各硬编码了一份，
> 一旦改了 `angleSpan`，**两处容量常数都必须同步改**，否则 `map()` 会越界写。

### 6.4 一个容易看反的地方：容量 ≠ 计数

有个细节很容易误判成 bug，实际是设计：

```cpp
// linegeometry.cpp:14-24 —— 路点轨迹线的构造函数（L4 用的就是这条，maxNum 默认 1000）
LineGeometry::LineGeometry(QVector<QVector3D> points, QColor color, int maxNum) : BaseGeometry()
{
    this->maxNum = maxNum;                                   // 1000
    this->pointList.append(points);                          // 初始为空
    this->lineMsg.color = color;
    GenerateLinesData(points);                               // 0 个点 → vertex_data 为空
    num_vertex_data = vertex_data.size();                    // = 0
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), maxNum * sizeof(FeatureVertexData));  // ★ 1000 点容量
    vertex_data_Buf.release();
}
```

`allocate` 的**长度参数是容量（1000 点），而不是 `num_vertex_data`（0）**，
但传进去的数据指针是空的 —— 也就是"**按 1000 点分配，但只填了 0 点的数据**"。

- 后续 `AddDeleteLineData(pointList)` 用 `map()` 覆盖前 N 个点 → **安全**（N ≤ 1000）。
- 绘制时用的是 `num_vertex_data`（`glDrawArrays(..., num_vertex_data)`），
  **不是容量** → 缓冲区尾部的垃圾数据不会被画出来。

如果这里写成 `allocate(data, num_vertex_data * sizeof(...))`，那么缓冲区只有 0 字节，
第一次 `AddDeleteLineData` 的 `map()` 就会越界 —— **所以这里的 `maxNum` 是必须的**。

### 6.5 策略 C 在 `AddDeleteLineData` 里的"删点残留"

```cpp
// robot3dcontrolwidget.cpp:58-61（调用侧，路点轨迹 FIFO 滑窗）
if (waypointPathlineGeometry->pointList.count() >= waypointPathlineGeometry->maxNum)
    waypointPathlineGeometry->pointList.removeFirst();
```

点数到达 1000 后删掉最早的点，然后 `AddDeleteLineData(pointList)` 重新生成并覆盖前 999 个点。
**缓冲区里第 1000 个位置还留着上一次的数据**，但 `num_vertex_data` 已经变成 999，
`glDrawArrays` 只画前 999 个 → **残留数据不会被渲染**。这是"容量/计数分离"带来的额外好处。

---

## 7. ⑤⑥ 两个空实现的接口约定

```cpp
bool BaseGeometry::initShaders() { return false; }                        // basegeometry.cpp:17-20
void BaseGeometry::drawGeometry(QMatrix4x4, QMatrix4x4, QMatrix4x4) { }   // basegeometry.cpp:22-25
```

**它们没有写成 `= 0` 纯虚函数**，而是给了空实现 —— 这个选择有两个实际后果：

1. **`BaseGeometry` 可以被实例化**（虽然没人这么干），不是抽象类。
2. **子类忘记 override 不会有编译错误**，只会静默地"不画 / 返回 false"。

`initShaders()` 的签名是 `virtual bool`，**语义约定是"成功返回 true、失败返回 false"**：

```cpp
// 例如 linegeometry.cpp:94-113
bool LineGeometry::initShaders()
{
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex,   ":/shaders/line_vertex_shader.glsl"))   return false;
    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/line_fragment_shader.glsl")) return false;
    if (!program.link()) return false;
    if (!program.bind()) return false;
    return true;
}
```

值得注意的三点：

- **返回值在 L4 里被完全忽略**：`lineGeometry->initShaders();` 不看结果（`robot3dglwidget.cpp:576` 等 18 处）。
  所以**着色器编译失败时 3D 会全黑而没有任何提示** —— 这正是 README 里"shader 带 BOM 导致 3D 全黑"那个坑难以定位的原因。
- **最后那个 `program.bind()` 是多余的**：`QOpenGLShaderProgram` 在 `link()` 成功时已经 bind 过，
  而这里再 bind 一次只是为了对齐 Qt 官方示例的写法；真正的 bind 发生在每帧 `drawGeometry()` 里。
- 四个图元类各持一个 `QOpenGLShaderProgram` 成员（在 `BaseGeometry` 里），
  所以**同一份 GLSL 会被编译多份**：line 着色器编译 6 次（坐标系 ×3 + 工具坐标系 ×3 + 路点轨迹 ×1 = 7 次，
  见第 11 节统计）。这是零共享的简单做法，换取的是"每个几何对象完全自洽"。

---

## 8. 着色器契约：8 个 GLSL 与 uniform 命名约定

### 8.1 四对 program 的分工

| 用途 | 顶点着色器 | 片段着色器 | attribute | uniform | 谁在用 |
|---|---|---|---|---|---|
| line | `line_vertex_shader.glsl` | `line_fragment_shader.glsl` | `a_position` | `mvp_matrix`, `lightColor`, `toyColor` | `LineGeometry` **+ `SphereGeometry`**（复用） |
| plane | `plane_vertex_shader.glsl` | `plane_fragment_shader.glsl` | `a_position` | 同上 + `alpha` | `PlaneGeometry` |
| substance | `substance_vertex_shader.glsl` | `substance_fragment_shader.glsl` | `a_position` | `mvp_matrix`, `lightColor`, `toyColor` | `SubstanceGeometry` |
| robot | `robot_vertex_shader.glsl` | `robot_fragment_shader.glsl` | `a_position`, `a_normal`, `a_material` | `mvp_matrix`, `model`, `light1.*`, `material.*`, `viewPos` | `RobotGeometry` |

> **注意 line 与 substance 两对的实际差别只在 alpha**：line 的片元输出 alpha 恒为 `1.0`，
> substance 恒为 `0.5`（`substance_fragment_shader.glsl:12`），
> 而 `SphereGeometry` 选择复用 line 那对（`spheregeometry.cpp:103, 106`）→ **球是不透明的**。

### 8.2 三个"简单"着色器：极简到只有一句

三个图元的顶点着色器**内容完全相同**（只有 `line_vertex_shader.glsl` / `plane_vertex_shader.glsl` /
`substance_vertex_shader.glsl` 三个文件的差别仅在于文件名）：

```glsl
#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

uniform mat4 mvp_matrix;
attribute vec4 a_position;

void main()
{
    gl_Position = mvp_matrix * a_position;
}
```

**关键点**：只用 `mvp_matrix`（投影×视图×模型**已经 CPU 侧合成好**），
顶点着色器里不做任何矩阵分解 —— 这决定了 `drawGeometry(model, mvp, light)` 三个参数里
**`model` 和 `lightMatrix` 在图元层是收下但不用的**（只有 `RobotGeometry` 用 `model`）。

片段着色器同样只有一句有效代码：

```glsl
// line_fragment_shader.glsl
uniform vec3 lightColor;      // 恒为 (1,1,1)，见下
uniform vec3 toyColor;
void main() { gl_FragColor = vec4(lightColor * toyColor, 1.0); }
```

`lightColor` 在 C++ 侧**每次绘制都固定传 (1,1,1)**：

```cpp
// linegeometry.cpp:119-121
program.bind();
program.setUniformValue("lightColor", QVector3D(1.0, 1.0, 1.0));                       // ← 永远是白
program.setUniformValue("toyColor", QVector3D(color.red()/255.0, color.green()/255.0, color.blue()/255.0));
program.setUniformValue("mvp_matrix", mvpMatrix);
```

也就是说 `lightColor * toyColor` 实际就是 `toyColor` —— **`lightColor` 是个预留的"亮度调节"旋钮**，
目前被钉死在 1.0。整条链路的补色来源是**图元对象的 `color` 成员**，不是顶点数据。

三个变体的差异：

| 着色器 | 片元输出 | C++ 侧的配合 |
|---|---|---|
| line | `vec4(lightColor*toyColor, 1.0)` | 不透明 |
| plane | `vec4(lightColor*toyColor, alpha)` | `setUniformValue("alpha", planeMsg.alpha)` + `glEnable(GL_BLEND)`（`planegeometry.cpp:122, 132-133`） |
| substance | `vec4(lightColor*toyColor, 0.5)` | **alpha 硬编码 0.5**，C++ 侧只开 blend，不传 alpha（`substancegeometry.cpp:132-133`） |

### 8.3 robot 着色器：唯一带光照的

顶点着色器（`robot_vertex_shader.glsl`）—— 注意它**同时用了 `mvp_matrix` 和 `model`**：

```glsl
uniform mat4 mvp_matrix;
uniform mat4 model;
attribute vec3 a_position;
attribute vec3 a_material;
attribute vec3 a_normal;
varying vec3 mater;
varying vec3 Normal;
varying vec3 fragPos;

void main()
{
    gl_Position = mvp_matrix * vec4(a_position, 1.0);     // 屏幕空间位置
    fragPos     = vec3(model * vec4(a_position, 1.0));    // 世界空间位置（给视方向用）
    Normal      = vec3(model * vec4(a_normal, 0.0));      // 世界空间法线（w=0 只旋转不平移）
    mater       = a_material;                             // 材质色直接插值
}
```

两个细节值得注意：

- `Normal = vec3(model * vec4(a_normal, 0.0))` 用的是 **w=0**，所以只受旋转影响、不受平移影响 —— 正确做法。
  但它**没有用逆转置矩阵**，因此当 `model` 含非均匀缩放时法线会偏。
  本模块的 `model` 只有**均匀缩放**（`model.scale(scalePara)`）+ 旋转 + 平移，所以恰好没问题。
- **`fragPos` 与 `Normal` 都在"模型×世界"空间**，而 `viewPos` uniform 传的是
  `robotGeometry->Pos`（相机位置，也是模型空间）—— 三者同空间，光照计算才自洽。
  这就是为什么 `RobotGeometry::drawGeometry` 必须把 `model` 也传进着色器：**它是光照一致性的前提**。

片段着色器实现了完整的**单方向光 Blinn-Phong**（`robot_fragment_shader.glsl:33-56`）：

```glsl
vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir   = normalize(-light.direction);
    float diff      = max(dot(normal, lightDir), 0.0);                                   // 漫反射
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);        // 镜面
    vec3 ambient    = light.ambient  * material.ambient;
    vec3 diffuse    = light.diffuse  * diff * material.diffuse;
    vec3 specular   = light.specular * spec * material.specular;
    return (ambient + diffuse + specular);
}
void main()
{
    vec3 norm    = normalize(Normal);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 result  = CalcDirLight(light1, norm, viewDir);
    gl_FragColor = vec4(mater * result, 1.0);      // ★ 材质色乘光照
}
```

- 结构体 `Material` / `Light` 都在片段着色器里，C++ 侧用**点号命名的 uniform** 填充：
  `"material.shininess"`、`"light1.ambient"` 等（`robotgeometry.cpp:364-380`）。
- `Light` 结构体里定义了 `light2` 但**从未使用**（`robot_fragment_shader.glsl:29`），是预留的第二光源。
- `mater * result` 里的 `mater` 来自 `a_material`，而 `a_material` 在 L3 里是从
  **`.obj.mtl` 的 `Kd`（漫反射色）**读出来的（`robotgeometry.cpp:155-157, 171-173`）——
  所以**改模型外观有两种途径**：改 `.mtl` 文件，或改 C++ 里的 `material.*` uniform。

### 8.4 GLSL 的两个隐性约定（踩过的坑）

1. **`#ifdef GL_ES` + `precision mediump`**：这套着色器是为**桌面 GL 与 GLES 双份兼容**写的
   （示教器是嵌入式 ARM 板，Demo 是桌面 MinGW）。桌面 GL 下 `GL_ES` 未定义，
   `attribute`/`varying`/`gl_FragColor` 仍可用（GLSL 120 语法）。
2. **文件不能带 UTF-8 BOM** —— GLSL 编译器会在第一个字符处报
   `illegal non-ASCII character (0xef/0xbb/0xbf)`，**全部 8 个着色器编译失败 → 3D 全黑**。
   而不巧的是 `initShaders()` 的返回值在 L4 里被忽略（见第 7 节），所以这个错误没有任何提示。

---

## 9. `drawGeometry` 的完整调用契约（每帧发生的事）

### 9.1 参数语义

```cpp
virtual void drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
```

| 参数 | 内容 | 图元层（L2）怎么用 | 网格层（L3）怎么用 |
|---|---|---|---|
| `model` | 模型矩阵（缩放·坐标系·平移·旋转，见 `paintGL`） | **不用** | 传 uniform `model`（算世界空间法线） |
| `mvpMatrix` | `projection * view * model` | 传 uniform `mvp_matrix` | 传 uniform `mvp_matrix` |
| `lightMatrix` | 光照矩阵 | **不用** | 乘到光方向上（当前传的是空矩阵 = 单位阵） |

**三个参数是取所有绘制者并集的结果** —— 图元层"多收两个不用的参数"，
这是为了让 `paintGL()` 里能用一个统一签名循环调用所有几何体。

### 9.2 图元层的标准 6 步（L2 四个类逐字相同）

```cpp
void XxxGeometry::drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();                                                    // ① 切到自己的着色器
    program.setUniformValue("lightColor", QVector3D(1.0,1.0,1.0));      // ② 三个 uniform
    program.setUniformValue("toyColor",  QVector3D(...color 归一化...));
    program.setUniformValue("mvp_matrix", mvpMatrix);

    vertex_data_Buf.bind();                                            // ③ 绑 VBO（决定下面 attribute 从哪读）

    quintptr offset = 0;                                               // ④ 描述顶点属性
    int vertexLocation = program.attributeLocation("a_position");
    program.enableAttributeArray(vertexLocation);
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(FeatureVertexData));

    [glEnable(GL_BLEND) + glBlendFunc(...)]                            // ⑤ 仅 plane / substance

    glDrawArrays(GL_TRIANGLES / GL_LINE_STRIP, 0, num_vertex_data);    // ⑥ 真正的绘制
}
```

### 9.3 第 ③ 步与第 ④ 步的**次序依赖**（很多人会看漏）

`setAttributeBuffer(location, ...)` **不是**"把地址告诉着色器"，而是
"**把当前已绑定的 GL_ARRAY_BUFFER 的偏移记到这个 attribute 上**"。
所以 `vertex_data_Buf.bind()` 必须**先**执行 —— 这正是 `qopenglwidget` 示例里那个容易漏的坑。
本模块 8 处绘制全部正确遵守了这个次序。

由此也能推出一个隐式约束：**每个几何对象必须绑自己的 VBO 再描述属性**。
现在每个对象都有独立 VBO（`BaseGeometry::vertex_data_Buf`），所以没问题；
但如果将来想"多个图元共享一个 VBO"，这套代码必须改成单次 bind + 一次属性描述 + 多次 `glDrawArrays(offset)`。

### 9.4 第 ⑥ 步的绘制模式对照

| 类 | 调用 | 说明 |
|---|---|---|
| `LineGeometry` | `glDrawArrays(GL_LINE_STRIP, 0, num_vertex_data)` + `glLineWidth(3)` | 折线；坐标系轴是 2 点所以等价于一段直线 |
| `PlaneGeometry` | `glDrawArrays(GL_TRIANGLES, 0, num_vertex_data)` | 每面 6 顶点 = 2 三角 |
| `SphereGeometry` | `glDrawArrays(GL_TRIANGLES, 0, num_vertex_data)` | 432 顶点/球 |
| `SubstanceGeometry` | `glDrawArrays(GL_TRIANGLES, 0, num_vertex_data)` | 按类型生成的三角面 |
| `RobotGeometry` | `glDrawArrays(GL_TRIANGLES, 0, numVertices)` **在循环里调用 N 次** | 每个部件一次 |

**注意 `RobotGeometry` 是"多次 draw"**：一个 `drawGeometry` 里循环 N 个部件，
每次重新绑不同的 `MeshEntry::VB`、重新描述一次属性、再 draw：

```cpp
// robotgeometry.cpp:387-410（简化）
for (int num_model = 0; num_model < 1 + numJoints + mmin(numTool, showEndToolStatus); num_model++) {
    entriesData[num_model].VB.bind();                 // ← 每次换 VBO
    ... 重新 enableAttributeArray + setAttributeBuffer ×3 ...   // ← 每次重描述属性
    glDrawArrays(GL_TRIANGLES, 0, entriesData[num_model].numVertices);
}
```

属性描述被重复执行了 7~8 次（因为 7~8 个 VBO 各自需要一次绑定+描述），属于"能用但啰嗦"的写法。

### 9.5 属性数组**开了不关**

所有绘制路径都在结束时**没有 `disableAttributeArray`**，也没解绑 VBO/program。
因为下一帧第一个动作是 `glClear` + 另一个几何体的 `bind()`，
而 OpenGL 的顶点属性/绑定状态是"覆盖式"的，所以**当前单线程、全屏独占的渲染循环下不会出错**。

代价是：**这套绘制代码不是自包含的** —— 任何"离屏渲染到 FBO / 多 pass / 多视图"的扩展
都必须自己补齐状态管理（解绑、禁用属性、重置 lineWidth）。

---

## 10. 全局渲染状态：谁设置、谁消费

`BaseGeometry` 自己**不设置任何全局 GL 状态**，全部由 L4 的 `initializeGL()` 一次性设定：

```cpp
// robot3dglwidget.cpp:535-539
glClearColor(1, 1, 1, 1);      // 白底
glFrontFace(GL_CCW);           // 逆时针为正面
glCullFace(GL_BACK);           // 设置背面剔除的面 —— 但下面这行是注释掉的！
//glEnable(GL_CULL_FACE);      // ← 从未启用 → 实际是双面渲染
glEnable(GL_DEPTH_TEST);       // 深度测试开启
```

| 状态 | 在哪里设 | 在哪里被依赖 / 泄漏 |
|---|---|---|
| `GL_DEPTH_TEST` | `initializeGL()` | 全局依赖（8 个对象都靠它排遮挡） |
| `glClearColor` 白 | `initializeGL()` | 每帧 `glClear` |
| `GL_CULL_FACE` | **未启用**（注释掉） | 注释里写着原因："旋转面模型时，某些位姿看不到" —— 即顶点绕序不保证一致 |
| `GL_BLEND` + `glBlendFunc` | `PlaneGeometry::drawGeometry:132-133`、`SubstanceGeometry::drawGeometry:132-133` | **开了不关** → 影响其后所有绘制（本模块内后续对象的 alpha 恰好都是 1.0 或本来就是半透明，所以看不出问题） |
| `glLineWidth(3)` | `LineGeometry::drawGeometry:132` | **设了不还原** → 全局线宽 |

> 两句结论：**这套渲染代码依赖"调用顺序固定"**（`paintGL()` 里的固定顺序就是隐藏契约）；
> 半透明对象（安全平面 `alpha`、工具 `alpha=0.5`）**没有做深度排序**，
> 靠"最后画"来近似正确 —— 这也是 `paintGL()` 顺序不能随便调的原因。

---

## 11. L1 的实际规模（`paintGL` 一次会做多少事）

按 `initializeGL()` 创建的对象统计：

| 几何对象 | 类（→ 用哪对着色器） | 个数 | 说明 |
|---|---|---|---|
| `robotGeometry` / `secondRobotGeometry` | `RobotGeometry`（robot） | 2 | 各 7~8 个 VBO |
| `coordSysGeometry` | `LineGeometry`（line） | 3 | 坐标系三轴，各 2 顶点 |
| `toolCoordSysGeometry` | `LineGeometry`（line） | 3 | 工具坐标系三轴，各 2 顶点 |
| `waypointPathlineGeometry` | `LineGeometry`（line） | 1 | 容量 1000 点 |
| `TCP_Geometry` | `SphereGeometry`（line） | 1 | 432 顶点 |
| `waypointSphereGeometry` | `SphereGeometry`（line） | 1 | 容量 500 球 |
| `safePlaneGeometrys` | `PlaneGeometry`（plane） | 8 | 各 6 顶点 |
| `tool_Geometry` | `SubstanceGeometry`（substance） | 1 | ~432 顶点起 |

推导出的两个"数量级"事实：

1. **`QOpenGLShaderProgram` 实例 = 20 个**（L4 创建 18 个几何体 + 2 个 `RobotGeometry`），
   而实际只有 **4 份不同的着色器程序**（line / plane / substance / robot）。
   每个 program 在 `initShaders()` 里完成一次 `compile + link` → **启动时多编译了 16 次**。
   这是"每个几何对象完全自洽"这一设计选择的代价 —— 它**不是每帧瓶颈**
   （一次性开销），但如果是启动时间敏感的嵌入式设备，值得改成 4 个共享 program。
2. **每帧 `program.bind()` ≥ 20 次、`glDrawArrays` 约 27 次**（机器人本体额外 7~8 次 draw）。
   在白底 + 深度测试的固定相机下，这个量级对示教器嵌入式 GPU 也是够用的。
3. **`substance_vertex/fragment_shader.glsl` 只被 `SubstanceGeometry` 使用**，
   而 `SphereGeometry` 复用的是 **line** 那对（`spheregeometry.cpp:103, 106`）——
   尽管 `demo.qrc` 里确实打包了 substance 那对。
   也就是说"球"和"线"共用着色器、靠 uniform 区分颜色与 alpha —— L1 的着色器复用是按**几何类型**分的，
   不是按"谁需要透明"分的。

显存侧由 L1 直接决定的两处大头：
**路点球 2.47 MB**（`12 × 432 × 500`）与**机器人网格**（7~8 个部件 × 各约 1~3 万顶点 × **36 字节**）。
对比图元层：一个球 / 一个立方体只有几千字节 —— **36 字节/顶点 vs 12 字节/顶点**，
这就是 L3 用 `VertexData`（带法线+材质）的显存代价。

---

## 12. L1 的细节问题清单（逐条给判据与影响）

| # | 问题 | 位置 | 判据 / 影响 | 严重度 |
|---|---|---|---|---|
| 1 | **索引管线是死代码** | `basegeometry.h:97-100`、`basegeometry.cpp:4,9,14` | `vertex_index` 从未 push、`num_vertex_index` 从未赋值、`vertex_index_Buf` 只 create/destroy；全模块无 `glDrawElements` | 低（浪费 1 个 buffer 对象/几何体） |
| 2 | **析构时没有 makeCurrent** | `basegeometry.cpp:11-15`、`robotgeometry.cpp:119-127` | `QOpenGLBuffer::destroy()` 需要当前上下文；`paintGL` 之外（如窗口销毁）调用会静默失败/未定义。且 L4 从未 `delete` 任何几何体 → 这套析构路径**实际从未被执行** | 中（潜在泄漏，当前不触发） |
| 3 | **`map()` 返回值不检查** | `linegeometry.cpp:38`、`planegeometry.cpp:45`、`spheregeometry.cpp:45`、`substancegeometry.cpp:63` 等 8 处 | `map()` 失败返回 `nullptr`，随后 `memcpy(nullptr, ...)` 直接崩。理论上只在上下文丢失/显存不足时发生 | 中 |
| 4 | **`initShaders()` 返回值被忽略** | `robot3dglwidget.cpp` 18 处 | 着色器编译/链接失败时 3D 全黑，无任何日志 —— 与 BOM 坑叠加后极难定位 | 中（已通过"shader 不带 BOM"规避） |
| 5 | **同一 GLSL 编译 18 份** | 每个几何对象一个 `program` 成员 | 启动多编译 ~14 次；显存/驱动对象偏多 | 低 |
| 6 | **`GL_BLEND` / `glLineWidth` 开了不还原** | `planegeometry.cpp:132`、`substancegeometry.cpp:132`、`linegeometry.cpp:132` | 状态泄漏到后续绘制；当前绘制顺序下无可见影响，但破坏绘制代码的自包含性 | 低 |
| 7 | **半透明对象无深度排序** | `paintGL()` 固定顺序 | 安全平面/工具为半透明，仅靠"最后画"近似正确；调整绘制顺序可能立刻暴露 | 低（当前顺序下正确） |
| 8 | **`numVertex=432` 容量常数硬编码两处** | `spheregeometry.h:26`、`substancegeometry.h:35` | 改球面细分度 `angleSpan` 时必须同步改，否则策略 B/C 的 `map()` 越界写 | 中（改代码时的陷阱） |
| 9 | **`PointMsg` 定义了但无实现** | `basegeometry.h:49-53` | 只有数据结构，没有 `PointGeometry` 类 —— 原项目预留的"绘制点"能力未完成 | 低 |
| 10 | **`GL_CULL_FACE` 注释掉** | `robot3dglwidget.cpp:538-539` | 背面也渲染 → 顶点绕序不一致的模型（如 obj 导出不规范）不会露洞，代价是填充率翻倍 | 低（有意为之，注释里有说明） |
| 11 | **顶点的 `material` 走 attribute 而非 uniform** | `VertexData` 的第 3 个 `QVector3D` | 每个顶点存一份材质色（36 字节中的一个 vec3），而实际上同一个部件内材质色**完全相同** → 可用 uniform 省掉 1/3 顶点内存 | 低（设计冗余） |

---

## 13. 一句话总结 L1 的设计取舍

`BaseGeometry` 用**极小的抽象**换来**极强的可复制性**：

- **抽象面**只有 4 个：`vertex_data`（CPU 顶点）、`vertex_data_Buf`（GPU 顶点）、
  `num_vertex_data`（计数）、`program`（着色器）—— 加上 `initShaders()` / `drawGeometry()` 两个虚函数。
- **没有**：索引、UV、纹理、实例化、多 pass、材质统一管理、状态栈、资源池。
- **两个贯穿全模块的约定**由它定义：
  1. **`G_SCALE` 单位约定**（外部 mm → 模型单位，各图元在自己的 `Generate*Data` 里乘）；
  2. **"顶点数组膨胀 + 逐帧重写"** 的渲染路线 —— 这正是 L3 关节动画能直接用 CPU 改顶点、
   `map/memcpy` 上传的原因，也是 L1 必须是"每对象独立 VBO"的原因。
