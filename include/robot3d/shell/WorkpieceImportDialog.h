#pragma once
#include <QDialog>
#include <functional>
#include <memory>
#include "robot3d/domain/Workpiece.h"
class WorkpieceImportDialog : public QDialog {
    Q_OBJECT
public:
    typedef std::function<bool(robot3d::domain::WorkpieceAssetPtr,const std::string &,const std::string &,
        const robot3d::domain::EulerPoseDeg &,QString &)> CommitHandler;
    explicit WorkpieceImportDialog(const robot3d::domain::RobotStateSnapshot &,QWidget *parent=nullptr);
    ~WorkpieceImportDialog();
    void setCommitHandler(CommitHandler);
    void setFilePath(const QString &);
    bool isLoading() const;
    robot3d::domain::WorkpieceAssetPtr previewAsset() const;
public slots:
    void loadPreview();
    void reject() override;
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
    void invalidate();
    void schedulePreview();
    void rebuildPreview();
    void updateControls();
    void commit();
};
