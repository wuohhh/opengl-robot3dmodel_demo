#pragma once
#include <QWidget>
#include <memory>
#include "robot3d/scene/RobotSceneSnapshot.h"
class RobotViewport;
class WorkpiecePanel : public QWidget {
    Q_OBJECT
public:
    explicit WorkpiecePanel(RobotViewport *,QWidget *parent=nullptr);
    ~WorkpiecePanel();
    void decorateScene(robot3d::scene::RobotSceneSnapshot &);
    robot3d::scene::RobotSceneSnapshot currentScene() const;
    bool mayClose();
    bool dirty() const;
    const robot3d::domain::WorkpieceSceneState &state() const;
public slots:
    void importWorkpiece();
    void selectWorkpiece(const QString &);
    void saveScene();
    void openScene();
signals:
    void sceneChanged();
    void closeReady();
    void dirtyChanged(bool);
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    void refresh();
    void apply();
    void remove();
    void undo(bool redo);
    void startOpen(const QString &);
};
