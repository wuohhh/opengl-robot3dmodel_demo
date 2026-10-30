#pragma once
#include <QOpenGLWidget>
#include <QColor>
#include <memory>
#include "robot3d/domain/Workpiece.h"
class QPainter;
class WorkpiecePreviewWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit WorkpiecePreviewWidget(QWidget *parent=nullptr);
    ~WorkpiecePreviewWidget();
    void setAsset(robot3d::domain::WorkpieceAssetPtr);
    void setDisplayMode(int);
    void setUniformColor(bool,const QColor &);
    void setDoubleSided(bool);
    void setBoundsVisible(bool);
    bool ready() const;
    void focusComponent(const QString &id);
public slots:
    void resetCamera();
    void standardView(int);
signals:
    void assetReady();
    void renderError(const QString &);
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int,int) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    void release();
    void drawOverlay(QPainter &);
};
