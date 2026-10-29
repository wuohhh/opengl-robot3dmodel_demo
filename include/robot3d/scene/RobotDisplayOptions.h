#pragma once
namespace robot3d
{
namespace scene
{
/** @brief 控制三维视图面板、场景装饰和安装姿态模式的显示选项。 */
struct RobotDisplayOptions
{
    bool coordinatePanel = true; ///< 是否显示坐标系选择面板。
    bool operatePanel = true;    ///< 是否显示视图操作面板，含复位、旋转/平移切换和末端工件显隐。
    bool zoomPanel = true;       ///< 是否显示视图缩放面板。
    bool posePanel = false;      ///< 是否显示位姿操作面板。
    bool waypointPanel = true;   ///< 是否显示路点和轨迹操作面板。
    bool speedPanel = true;      ///< 是否显示 TCP 速度面板。
    bool coordinateAxes = true;  ///< 是否显示所选坐标系的三轴坐标线。
    bool waypoints = false;      ///< 是否显示已记录的路点标记。
    bool trajectory = false;     ///< 是否显示 TCP 运动轨迹线。
    bool tool = false;           ///< 是否显示工具几何体及工具坐标轴。
    bool endWorkpiece = false;   ///< 是否显示机器人末端工件模型。
    bool targetRobot = false;    ///< 是否显示使用 targetJoints 的目标机器人模型。
    bool safePlanes = false;     ///< 是否显示安全边界平面。
    bool install = false;        ///< 是否启用安装姿态显示模式。
};
}
}
