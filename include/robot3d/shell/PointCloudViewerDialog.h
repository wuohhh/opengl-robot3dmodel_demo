#pragma once
#include <QDialog>
#include <memory>
class PointCloudViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PointCloudViewerDialog(QWidget* parent=nullptr);
    ~PointCloudViewerDialog();
    bool isLoading() const;
    QString loadedFile() const;
private:
    void loadSelectedFile();
    void clearCloud();
    void finishLoading();
    void updateColor();
    void updateControls();
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};
