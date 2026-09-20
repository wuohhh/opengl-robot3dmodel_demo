#ifndef ROBOT3DCONTROLWIDGET_H
#define ROBOT3DCONTROLWIDGET_H

#include <QWidget>
#include <QTouchEvent>
#include <QTimer>

#include"robot3dglwidget.h"

class Robot3dControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit Robot3dControlWidget(QWidget *parent = nullptr);

    static void InitInstance(QWidget *parent);
    static void UnInitInstance();
    static Robot3dControlWidget* GetInstance();

    static Robot3dControlWidget* robot3dControl_handle;

    Robot3dGLWidget* robot3dGLWidget;

    //bool addWaypointPathLineSignal = false;
    Pose preTcpPose = {0,0,0,0,0,0};
   //  QList<LineMsg> waypointPathlinelist;
    void AddRobotWaypointPath(Pose pose);

    //
    Robot3dGLWidget* GetRobot3dGLWidget(Robot3DShowTypes showType, bool isUpdateRTRobot3dMsg);
    void UpdateRobot3dJointAngle(JointPose jointPose);
    void UpdateRobot3dToolPosition(Pose pose);
    void UpdateRobot3dSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn);
    void ResizeRobot3dMode();

};

class TouchLongPressFilter : public QObject
{
    Q_OBJECT
public:
    TouchLongPressFilter(QObject *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *event) override;

    void touchEndEvent();

    QTimer timer;
    QPushButton *senderPushButton;
    int jogKeepCount = 0;

public slots:
    void handleLongPress_slot();

private:
    QList<QString> pixmapName;
    QList<QString> pixmapNamePress;
    QString moveBtnString;
    QString moveBtnPressString;
};

#endif // ROBOT3DCONTROLWIDGET_H
