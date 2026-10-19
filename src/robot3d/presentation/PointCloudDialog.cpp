#include "robot3d/presentation/PointCloudDialog.h"
#include "robot3d/infrastructure/PointCloudFileLoader.h"
#include "robot3d/rendering/vtk/VtkPointCloudViewport.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <limits>
struct PointCloudDialog::Implementation {
    VtkPointCloudViewport* viewport=nullptr;
    QLineEdit* path=nullptr;
    QPushButton *browse=nullptr,*load=nullptr,*reset=nullptr,*clear=nullptr,*colorButton=nullptr;
    QSpinBox* pointSize=nullptr;
    QComboBox* colorMode=nullptr;
    QCheckBox* visible=nullptr;
    QLabel *summary=nullptr,*status=nullptr;
    QProgressBar* progress=nullptr;
    QFutureWatcher<robot3d::infrastructure::PointCloudLoadResult>* watcher=nullptr;
    std::shared_ptr<const robot3d::domain::PointCloud> cloud;
    QString loadedPath,pendingPath;
    QColor color=QColor(51,204,255);
    bool loading=false;
};
PointCloudDialog::PointCloudDialog(QWidget* parent):QDialog(parent),impl_(new Implementation) {
    setObjectName("pointCloudDialog"); setWindowTitle(QString::fromUtf8("VTK 点云查看器"));
    setWindowFlag(Qt::WindowContextHelpButtonHint,false); resize(1080,720); setMinimumSize(800,520);
    auto* layout=new QVBoxLayout(this);
    auto* fileRow=new QHBoxLayout;
    impl_->path=new QLineEdit(this); impl_->path->setObjectName("pointCloudFilePath");
    impl_->path->setPlaceholderText(QString::fromUtf8("选择点云文件，或输入完整路径")); impl_->path->setClearButtonEnabled(true);
    impl_->browse=new QPushButton(QString::fromUtf8("选择文件…"),this); impl_->browse->setObjectName("pointCloudBrowseButton");
    impl_->load=new QPushButton(QString::fromUtf8("加载显示"),this); impl_->load->setObjectName("pointCloudLoadButton");
    impl_->load->setAutoDefault(false);
    fileRow->addWidget(new QLabel(QString::fromUtf8("点云文件："),this)); fileRow->addWidget(impl_->path,1);
    fileRow->addWidget(impl_->browse); fileRow->addWidget(impl_->load); layout->addLayout(fileRow);
    auto* content=new QHBoxLayout;
    impl_->viewport=new VtkPointCloudViewport(this); content->addWidget(impl_->viewport,1);
    auto* controls=new QWidget(this); controls->setFixedWidth(220); auto* side=new QVBoxLayout(controls); side->setMargin(0);
    auto* display=new QGroupBox(QString::fromUtf8("显示设置"),controls); auto* form=new QFormLayout(display);
    impl_->pointSize=new QSpinBox(display); impl_->pointSize->setObjectName("pointCloudPointSize");
    impl_->pointSize->setRange(1,20); impl_->pointSize->setValue(3); impl_->pointSize->setSuffix(" px");
    form->addRow(QString::fromUtf8("点大小："),impl_->pointSize);
    impl_->colorMode=new QComboBox(display); impl_->colorMode->setObjectName("pointCloudColorMode");
    impl_->colorMode->addItem(QString::fromUtf8("文件原色")); impl_->colorMode->addItem(QString::fromUtf8("统一颜色"));
    form->addRow(QString::fromUtf8("颜色模式："),impl_->colorMode);
    impl_->colorButton=new QPushButton(QString::fromUtf8("选择颜色…"),display); impl_->colorButton->setObjectName("pointCloudColorButton");
    form->addRow(impl_->colorButton);
    impl_->visible=new QCheckBox(QString::fromUtf8("显示点云"),display); impl_->visible->setObjectName("pointCloudVisibleCheck"); impl_->visible->setChecked(true); form->addRow(impl_->visible);
    impl_->reset=new QPushButton(QString::fromUtf8("视角复位 / 适应窗口"),display); impl_->reset->setObjectName("pointCloudResetButton"); form->addRow(impl_->reset);
    impl_->clear=new QPushButton(QString::fromUtf8("清空点云"),display); impl_->clear->setObjectName("pointCloudClearButton"); form->addRow(impl_->clear); side->addWidget(display);
    auto* info=new QGroupBox(QString::fromUtf8("点云信息"),controls); auto* infoLayout=new QVBoxLayout(info);
    impl_->summary=new QLabel(info); impl_->summary->setObjectName("pointCloudSummary"); impl_->summary->setWordWrap(true);
    impl_->summary->setMinimumHeight(impl_->summary->fontMetrics().lineSpacing()*7);
    impl_->summary->setTextInteractionFlags(Qt::TextSelectableByMouse); infoLayout->addWidget(impl_->summary); side->addWidget(info);
    auto* help=new QLabel(QString::fromUtf8("左键拖动：旋转\n中键 / Shift + 左键：平移\n滚轮 / 右键拖动：缩放\n\n支持 PCD、PLY、XYZ、TXT\nXYZ/TXT：XYZ 或 XYZ RGB\nRGB 范围：0～255"),controls); help->setWordWrap(true); side->addWidget(help); side->addStretch(); content->addWidget(controls); layout->addLayout(content,1);
    impl_->progress=new QProgressBar(this); impl_->progress->setRange(0,0); impl_->progress->setTextVisible(false); impl_->progress->setMaximumHeight(5); impl_->progress->hide(); layout->addWidget(impl_->progress);
    impl_->status=new QLabel(QString::fromUtf8("请选择点云文件，然后点击“加载显示”。"),this); impl_->status->setObjectName("pointCloudStatus");
    impl_->status->setMinimumHeight(impl_->status->fontMetrics().lineSpacing()*2);
    impl_->status->setWordWrap(true); impl_->status->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(impl_->status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this); buttons->button(QDialogButtonBox::Close)->setText(QString::fromUtf8("关闭")); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close); layout->addWidget(buttons);
    connect(impl_->browse,&QPushButton::clicked,this,[this] {
        const auto path=QFileDialog::getOpenFileName(this,QString::fromUtf8("选择点云文件"),impl_->path->text(),
            QString::fromUtf8("点云文件 (*.pcd *.ply *.xyz *.txt);;PCD 点云 (*.pcd);;PLY 点云 (*.ply);;XYZ / TXT 点云 (*.xyz *.txt)"));
        if(!path.isEmpty()) impl_->path->setText(QDir::toNativeSeparators(path));
    });
    connect(impl_->path,&QLineEdit::textChanged,this,[this] { updateControls(); });
    connect(impl_->path,&QLineEdit::returnPressed,this,&PointCloudDialog::loadSelectedFile);
    connect(impl_->load,&QPushButton::clicked,this,&PointCloudDialog::loadSelectedFile);
    connect(impl_->pointSize,QOverload<int>::of(&QSpinBox::valueChanged),impl_->viewport,&VtkPointCloudViewport::setPointSize);
    connect(impl_->visible,&QCheckBox::toggled,impl_->viewport,&VtkPointCloudViewport::setCloudVisible);
    connect(impl_->reset,&QPushButton::clicked,impl_->viewport,&VtkPointCloudViewport::resetCamera);
    connect(impl_->clear,&QPushButton::clicked,this,&PointCloudDialog::clearCloud);
    connect(impl_->colorMode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this] { updateColor(); });
    connect(impl_->colorButton,&QPushButton::clicked,this,[this] {
        const auto color=QColorDialog::getColor(impl_->color,this,QString::fromUtf8("选择点云颜色"));
        if(color.isValid()) { impl_->color=color; impl_->colorMode->setCurrentIndex(1); updateColor(); }
    });
    for(auto* button:findChildren<QPushButton*>()) button->setAutoDefault(false);
    impl_->summary->setText(QString::fromUtf8("尚未加载点云")); updateColor(); updateControls();
}
PointCloudDialog::~PointCloudDialog() = default;
bool PointCloudDialog::isLoading() const { return impl_->loading; }
QString PointCloudDialog::loadedFile() const { return impl_->loadedPath; }
void PointCloudDialog::loadSelectedFile() {
    const auto path=impl_->path->text().trimmed(); if(impl_->loading||path.isEmpty()) return;
    impl_->pendingPath=QFileInfo(path).absoluteFilePath(); impl_->loading=true; updateControls();
    impl_->status->setStyleSheet(""); impl_->status->setText(QString::fromUtf8("正在加载：%1").arg(QDir::toNativeSeparators(impl_->pendingPath))); impl_->progress->show();
    const auto filename=impl_->pendingPath;
    impl_->watcher=new QFutureWatcher<robot3d::infrastructure::PointCloudLoadResult>(this);
    connect(impl_->watcher,&QFutureWatcher<robot3d::infrastructure::PointCloudLoadResult>::finished,this,&PointCloudDialog::finishLoading);
    impl_->watcher->setFuture(QtConcurrent::run([filename] { return robot3d::infrastructure::loadPointCloudFile(filename); }));
}
void PointCloudDialog::finishLoading() {
    auto* watcher=impl_->watcher;
    const auto result=watcher->result();
    impl_->watcher=nullptr; watcher->deleteLater();
    impl_->loading=false; impl_->progress->hide();
    if(result.ok()) {
        impl_->cloud=result.cloud; impl_->loadedPath=impl_->pendingPath; impl_->viewport->setCloud(result.cloud);
        impl_->visible->setChecked(true); impl_->viewport->setCloudVisible(true);
        if(!result.cloud->hasColors) impl_->colorMode->setCurrentIndex(1);
        updateColor();
        double low[3]={std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),std::numeric_limits<double>::max()};
        double high[3]={-low[0],-low[1],-low[2]};
        for(const auto& p:result.cloud->points) {
            const double xyz[]={p.x,p.y,p.z};
            for(int i=0;i<3;++i) { low[i]=std::min(low[i],xyz[i]); high[i]=std::max(high[i],xyz[i]); }
        }
        impl_->summary->setText(QString::fromUtf8("点数：%1\n颜色：%2\n\n坐标范围\nX：%3 ～ %4\nY：%5 ～ %6\nZ：%7 ～ %8")
            .arg(qulonglong(result.cloud->points.size())).arg(result.cloud->hasColors?QString::fromUtf8("RGB"):QString::fromUtf8("无"))
            .arg(low[0],0,'g',6).arg(high[0],0,'g',6).arg(low[1],0,'g',6).arg(high[1],0,'g',6).arg(low[2],0,'g',6).arg(high[2],0,'g',6));
        impl_->status->setStyleSheet("");
        impl_->status->setText(QString::fromUtf8("已显示 %1 个点：%2%3")
            .arg(qulonglong(result.cloud->points.size())).arg(QDir::toNativeSeparators(impl_->loadedPath))
            .arg(result.cloud->skippedPoints?QString::fromUtf8("（已跳过 %1 个无效坐标）").arg(qulonglong(result.cloud->skippedPoints)):QString()));
    } else {
        impl_->status->setStyleSheet("color:#b42318;");
        impl_->status->setText(QString::fromUtf8("加载失败：%1%2").arg(result.error).arg(impl_->cloud?QString::fromUtf8(" 当前仍显示上次成功加载的点云。"):QString()));
    }
    updateControls();
}
void PointCloudDialog::clearCloud() {
    if(impl_->loading) return;
    impl_->cloud.reset(); impl_->loadedPath.clear(); impl_->viewport->setCloud(nullptr);
    impl_->summary->setText(QString::fromUtf8("尚未加载点云")); impl_->status->setStyleSheet(""); impl_->status->setText(QString::fromUtf8("点云已清空。")); updateControls();
}
void PointCloudDialog::updateColor() {
    impl_->viewport->setColor(impl_->color,impl_->colorMode->currentIndex()==0);
    impl_->colorButton->setStyleSheet(QString("QPushButton { border-left: 14px solid %1; padding: 4px; }").arg(impl_->color.name()));
}
void PointCloudDialog::updateControls() {
    const bool loaded=bool(impl_->cloud),idle=!impl_->loading;
    impl_->path->setEnabled(idle); impl_->browse->setEnabled(idle); impl_->load->setEnabled(idle&&!impl_->path->text().trimmed().isEmpty());
    impl_->clear->setEnabled(idle&&loaded); impl_->reset->setEnabled(loaded); impl_->visible->setEnabled(loaded);
}
