#pragma once
#include "robot3d/domain/PointCloud.h"
#include <QColor>
#include <QImage>
#include <QWidget>
#include <memory>
class VtkPointCloudViewport : public QWidget {
    Q_OBJECT
public:
    explicit VtkPointCloudViewport(QWidget* parent=nullptr);
    ~VtkPointCloudViewport();
    void setCloud(std::shared_ptr<const robot3d::domain::PointCloud> cloud);
    void setPointSize(int size);
    void setColor(const QColor& color,bool useFileColors);
    void setCloudVisible(bool visible);
    void resetCamera();
    bool isRendererReady() const;
    std::size_t pointCount() const;
    QImage capture();
private:
    void render();
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};
