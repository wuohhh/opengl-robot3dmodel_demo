#include "robot3d/shell/WorkpieceImportDialog.h"
#include "robot3d/shell/WorkpiecePreviewWidget.h"
#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <stdexcept>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QColorDialog>
#include <mutex>
#include <map>
namespace {
QString utf(const std::string &s){return QString::fromUtf8(s.c_str());}
struct Progress {std::mutex mutex;QString stage;int percent=-1;};
struct Loaded {std::shared_ptr<robot3d::domain::WorkpieceAsset> asset;QString error;};
QDoubleSpinBox *number(QWidget *p,double low,double high,int decimals=3){
    auto *v=new QDoubleSpinBox(p);v->setDecimals(decimals);v->setRange(low,high);v->setKeyboardTracking(false);return v;
}
}
struct WorkpieceImportDialog::Impl {
    robot3d::domain::RobotStateSnapshot robot;
    std::shared_ptr<robot3d::domain::WorkpieceAsset> raw;
    robot3d::domain::WorkpieceAssetPtr candidate;
    CommitHandler handler;
    WorkpiecePreviewWidget *preview;
    QLineEdit *path,*name;
    QTreeWidget *tree;
    QLabel *info,*status,*capabilities;
    QPushButton *load,*cancelLoad,*import;
    QComboBox *unit,*up,*forward,*origin,*quality,*parent,*mode,*views;
    QDoubleSpinBox *custom,*pose[6];
    QCheckBox *leftHanded,*doubleSided,*bounds,*overrideColor;
    QProgressBar *progress;
    QTimer *debounce,*progressTimer;
    QColor color=QColor(165,180,200);
    std::shared_ptr<std::atomic_bool> cancellation,normalCancellation;
    std::shared_ptr<Progress> sharedProgress;
    std::uint64_t generation=0,normalGeneration=0;
    bool loading=false,normalizing=false,committing=false,populating=false,validPath=false;
    QString loadedPath;
    QDateTime loadedTime;
    qint64 loadedSize=0;
};
WorkpieceImportDialog::WorkpieceImportDialog(const robot3d::domain::RobotStateSnapshot &robot,QWidget *parent)
    :QDialog(parent),d_(new Impl){
    d_->robot=robot;setWindowTitle(QString::fromUtf8("导入工件模型"));setObjectName("workpieceImportDialog");setModal(true);
    resize(1150,760);if(auto *s=QApplication::primaryScreen())resize(size().boundedTo(s->availableGeometry().size()-QSize(30,60)));
    auto *layout=new QVBoxLayout(this);auto *pathRow=new QHBoxLayout;
    d_->path=new QLineEdit(this);d_->path->setObjectName("workpieceFilePath");d_->path->setPlaceholderText(QString::fromUtf8("选择 STEP / IGES / STL / OBJ / PLY"));
    auto *browse=new QPushButton(QString::fromUtf8("选择文件…"),this);d_->load=new QPushButton(QString::fromUtf8("加载预览"),this);d_->load->setObjectName("workpieceLoadButton");
    d_->cancelLoad=new QPushButton(QString::fromUtf8("取消加载"),this);
    pathRow->addWidget(d_->path,1);pathRow->addWidget(browse);pathRow->addWidget(d_->load);pathRow->addWidget(d_->cancelLoad);layout->addLayout(pathRow);
    auto *split=new QSplitter(Qt::Horizontal,this);
    d_->tree=new QTreeWidget(split);d_->tree->setObjectName("workpieceComponents");d_->tree->setHeaderLabel(QString::fromUtf8("导入组件"));d_->tree->setMinimumWidth(150);
    auto *center=new QWidget(split);auto *centerLayout=new QVBoxLayout(center);centerLayout->setContentsMargins(0,0,0,0);
    auto *viewRow=new QHBoxLayout;d_->views=new QComboBox(center);d_->views->addItems(QString::fromUtf8("等轴测|前|后|左|右|上|下").split('|'));
    auto *fit=new QPushButton(QString::fromUtf8("适应窗口"),center);d_->mode=new QComboBox(center);d_->mode->addItems(QString::fromUtf8("实体|线框|实体带边线").split('|'));d_->mode->setCurrentIndex(2);
    viewRow->addWidget(d_->views);viewRow->addWidget(fit);viewRow->addWidget(d_->mode);centerLayout->addLayout(viewRow);
    d_->preview=new WorkpiecePreviewWidget(center);centerLayout->addWidget(d_->preview,1);
    auto *scroll=new QScrollArea(split);scroll->setWidgetResizable(true);scroll->setMinimumWidth(340);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *parameters=new QWidget(scroll);auto *form=new QFormLayout(parameters);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);scroll->setWidget(parameters);
    d_->name=new QLineEdit(parameters);d_->name->setObjectName("workpieceInstanceName");form->addRow(QString::fromUtf8("工件名称"),d_->name);
    d_->quality=new QComboBox(parameters);d_->quality->addItems(QString::fromUtf8("快速 0.5 mm / 30°|标准 0.1 mm / 15°|精细 0.02 mm / 10°").split('|'));d_->quality->setCurrentIndex(1);form->addRow(QString::fromUtf8("CAD 离散质量"),d_->quality);
    d_->unit=new QComboBox(parameters);d_->unit->setObjectName("workpieceUnit");d_->unit->addItem(QString::fromUtf8("请选择单位（未知）"),0.0);
    d_->unit->addItem("mm",1.0);d_->unit->addItem("cm",10.0);d_->unit->addItem("m",1000.0);d_->unit->addItem("in",25.4);d_->unit->addItem(QString::fromUtf8("自定义换算"),-1.0);form->addRow(QString::fromUtf8("读入坐标单位"),d_->unit);
    d_->custom=number(parameters,0.000001,1000000,6);d_->custom->setValue(1);d_->custom->setSuffix(" mm / unit");form->addRow(QString::fromUtf8("自定义比例"),d_->custom);
    d_->up=new QComboBox(parameters);d_->forward=new QComboBox(parameters);for(int axis=1;axis<=3;++axis)for(int sign=1;sign>=-1;sign-=2){QString label=(sign>0?"+":"-")+QString("XYZ").mid(axis-1,1);d_->up->addItem(label,axis*sign);d_->forward->addItem(label,axis*sign);}d_->up->setCurrentIndex(4);d_->forward->setCurrentIndex(0);
    form->addRow(QString::fromUtf8("源上方向"),d_->up);form->addRow(QString::fromUtf8("源前方向"),d_->forward);
    d_->leftHanded=new QCheckBox(QString::fromUtf8("源模型为左手系"),parameters);form->addRow(d_->leftHanded);
    d_->origin=new QComboBox(parameters);d_->origin->addItems(QString::fromUtf8("保留源原点|包围盒中心|包围盒底面中心").split('|'));form->addRow(QString::fromUtf8("工件局部原点"),d_->origin);
    d_->parent=new QComboBox(parameters);d_->parent->addItem(QString::fromUtf8("世界坐标系"),"world");
    for(const auto &cs:robot.coordinates){robot3d::domain::Matrix4d ignored;if(robot3d::domain::parentWorldMatrix(cs.id,robot,ignored))d_->parent->addItem(utf(cs.name)+" ("+utf(cs.id)+")",utf(cs.id));}
    if(d_->parent->findData("b_0")<0)d_->parent->addItem(QString::fromUtf8("机器人基坐标系"),"b_0");d_->parent->setCurrentIndex(d_->parent->findData("b_0"));form->addRow(QString::fromUtf8("父坐标系"),d_->parent);
    const char *labels[]={"X (mm)","Y (mm)","Z (mm)","Rx (°)","Ry (°)","Rz (°)"};
    for(int i=0;i<6;++i){d_->pose[i]=number(parameters,i<3?-10000000:-360000,i<3?10000000:360000);d_->pose[i]->setObjectName(QString("workpieceInitialPose%1").arg(i));form->addRow(QString::fromUtf8(labels[i]),d_->pose[i]);}
    auto *hint=new QLabel(QString::fromUtf8("原点放到父坐标系的上述位姿。旋转：Rz × Ry × Rx。预览以工件局部坐标观察。"),parameters);hint->setWordWrap(true);form->addRow(hint);
    d_->doubleSided=new QCheckBox(QString::fromUtf8("双面观察"),parameters);d_->doubleSided->setChecked(true);d_->bounds=new QCheckBox(QString::fromUtf8("包围盒与尺寸"),parameters);d_->bounds->setChecked(true);
    d_->overrideColor=new QCheckBox(QString::fromUtf8("统一预览颜色"),parameters);auto *color=new QPushButton(QString::fromUtf8("选择颜色…"),parameters);form->addRow(d_->doubleSided);form->addRow(d_->bounds);form->addRow(d_->overrideColor,color);
    d_->info=new QLabel(QString::fromUtf8("尚未加载模型"),parameters);d_->info->setWordWrap(true);d_->info->setTextInteractionFlags(Qt::TextSelectableByMouse);form->addRow(d_->info);
    split->setStretchFactor(0,0);split->setStretchFactor(1,1);split->setStretchFactor(2,0);split->setSizes(QList<int>()<<170<<560<<360);layout->addWidget(split,1);
    d_->capabilities=new QLabel(robot3d::infrastructure::workpieceImportCapabilities(),this);d_->capabilities->setWordWrap(true);layout->addWidget(d_->capabilities);
    d_->progress=new QProgressBar(this);d_->progress->setRange(0,0);d_->progress->hide();layout->addWidget(d_->progress);
    d_->status=new QLabel(QString::fromUtf8("请选择文件并加载预览，确认尺寸和方向后导入。"),this);d_->status->setObjectName("workpieceImportStatus");d_->status->setWordWrap(true);d_->status->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(d_->status);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel,this);buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("取消"));
    d_->import=buttons->addButton(QString::fromUtf8("导入场景"),QDialogButtonBox::AcceptRole);d_->import->setObjectName("workpieceCommitButton");layout->addWidget(buttons);
    d_->debounce=new QTimer(this);d_->debounce->setSingleShot(true);d_->debounce->setInterval(120);connect(d_->debounce,&QTimer::timeout,this,&WorkpieceImportDialog::rebuildPreview);
    d_->progressTimer=new QTimer(this);d_->progressTimer->setInterval(100);connect(d_->progressTimer,&QTimer::timeout,this,[this]{if(!d_->sharedProgress)return;std::lock_guard<std::mutex> lock(d_->sharedProgress->mutex);d_->status->setText(d_->sharedProgress->stage);int percent=d_->sharedProgress->percent;if(percent>=0){d_->progress->setRange(0,100);d_->progress->setValue(percent);}else d_->progress->setRange(0,0);});
    connect(browse,&QPushButton::clicked,this,[this]{auto file=QFileDialog::getOpenFileName(this,QString::fromUtf8("选择工件模型"),d_->path->text(),"3D models (*.step *.stp *.iges *.igs *.stl *.obj *.ply);;STEP (*.step *.stp);;IGES (*.iges *.igs);;Meshes (*.stl *.obj *.ply)");if(!file.isEmpty())d_->path->setText(file);});
    connect(d_->path,&QLineEdit::textChanged,this,[this]{invalidate();});connect(d_->path,&QLineEdit::returnPressed,this,&WorkpieceImportDialog::loadPreview);
    connect(d_->load,&QPushButton::clicked,this,&WorkpieceImportDialog::loadPreview);
    connect(d_->cancelLoad,&QPushButton::clicked,this,[this]{invalidate();d_->status->setText(QString::fromUtf8("已取消。旧任务结果不会提交到场景。"));});
    connect(buttons,&QDialogButtonBox::rejected,this,&WorkpieceImportDialog::reject);connect(d_->import,&QPushButton::clicked,this,&WorkpieceImportDialog::commit);
    for(auto *combo:{d_->unit,d_->up,d_->forward,d_->origin})connect(combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{schedulePreview();});
    connect(d_->quality,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{if(d_->raw){invalidate();d_->status->setText(QString::fromUtf8("离散质量已更改，请重新加载预览。"));}});
    connect(d_->custom,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this]{schedulePreview();});connect(d_->leftHanded,&QCheckBox::toggled,this,[this]{schedulePreview();});
    connect(d_->tree,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int column){
        if(d_->populating||column!=0)return;
        QSignalBlocker blocker(d_->tree);
        auto state=item->checkState(0);
        std::function<void(QTreeWidgetItem*)> propagate=[&](QTreeWidgetItem *node){
            for(int i=0;i<node->childCount();++i){node->child(i)->setCheckState(0,state);propagate(node->child(i));}
        };
        if(state!=Qt::PartiallyChecked)propagate(item);
        for(auto *parent=item->parent();parent;parent=parent->parent()){
            bool any=false,all=true;
            for(int i=0;i<parent->childCount();++i){auto s=parent->child(i)->checkState(0);any=any||s!=Qt::Unchecked;all=all&&s==Qt::Checked;}
            parent->setCheckState(0,all?Qt::Checked:(any?Qt::PartiallyChecked:Qt::Unchecked));
        }
        schedulePreview();
    });
    connect(d_->tree,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){if(item&&d_->candidate)d_->preview->focusComponent(item->data(0,Qt::UserRole).toString());});
    connect(d_->name,&QLineEdit::textChanged,this,[this]{updateControls();});
    connect(d_->views,QOverload<int>::of(&QComboBox::currentIndexChanged),d_->preview,&WorkpiecePreviewWidget::standardView);connect(fit,&QPushButton::clicked,d_->preview,&WorkpiecePreviewWidget::resetCamera);
    connect(d_->mode,QOverload<int>::of(&QComboBox::currentIndexChanged),d_->preview,&WorkpiecePreviewWidget::setDisplayMode);
    connect(d_->doubleSided,&QCheckBox::toggled,d_->preview,&WorkpiecePreviewWidget::setDoubleSided);connect(d_->bounds,&QCheckBox::toggled,d_->preview,&WorkpiecePreviewWidget::setBoundsVisible);
    connect(d_->overrideColor,&QCheckBox::toggled,this,[this](bool on){d_->preview->setUniformColor(on,d_->color);});
    connect(color,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(d_->color,this);if(c.isValid()){d_->color=c;d_->overrideColor->setChecked(true);d_->preview->setUniformColor(true,c);}});
    connect(d_->preview,&WorkpiecePreviewWidget::assetReady,this,[this]{updateControls();});
    connect(d_->preview,&WorkpiecePreviewWidget::renderError,this,[this](const QString &e){d_->candidate.reset();d_->status->setText(QString::fromUtf8("预览失败：")+e);updateControls();});
    for(auto *b:findChildren<QPushButton*>())b->setAutoDefault(false);updateControls();
}
WorkpieceImportDialog::~WorkpieceImportDialog(){if(d_->cancellation)d_->cancellation->store(true);if(d_->normalCancellation)d_->normalCancellation->store(true);}
void WorkpieceImportDialog::setCommitHandler(CommitHandler h){d_->handler=std::move(h);}
void WorkpieceImportDialog::setFilePath(const QString &p){d_->path->setText(p);}
bool WorkpieceImportDialog::isLoading()const{return d_->loading||d_->normalizing;}
robot3d::domain::WorkpieceAssetPtr WorkpieceImportDialog::previewAsset()const{return d_->candidate;}
void WorkpieceImportDialog::invalidate(){
    ++d_->generation;++d_->normalGeneration;if(d_->cancellation)d_->cancellation->store(true);if(d_->normalCancellation)d_->normalCancellation->store(true);
    d_->debounce->stop();d_->progressTimer->stop();d_->loading=false;d_->normalizing=false;d_->candidate.reset();d_->raw.reset();d_->validPath=false;d_->progress->hide();
    d_->status->setText(QString::fromUtf8("当前文件尚未就绪；视图区可能仍为上次预览，请重新加载。"));updateControls();
}
void WorkpieceImportDialog::reject(){if(d_->committing)return;invalidate();QDialog::reject();}
void WorkpieceImportDialog::loadPreview(){
    if(d_->path->text().trimmed().isEmpty()||d_->committing)return;
    invalidate();d_->loading=true;d_->status->setStyleSheet("");d_->progress->setRange(0,0);d_->progress->show();
    auto path=QFileInfo(d_->path->text().trimmed()).absoluteFilePath();auto generation=d_->generation;
    d_->cancellation=std::make_shared<std::atomic_bool>(false);d_->sharedProgress=std::make_shared<Progress>();
    robot3d::infrastructure::WorkpieceLoadOptions options;options.cancellation=d_->cancellation;
    double linear[]={.5,.1,.02},angular[]={30,15,10};options.linearDeflectionMm=linear[d_->quality->currentIndex()];options.angularDeflectionDeg=angular[d_->quality->currentIndex()];
    auto progress=d_->sharedProgress;options.progress=[progress](const QString &s,int p){std::lock_guard<std::mutex> lock(progress->mutex);progress->stage=s;progress->percent=p;};
    auto *watcher=new QFutureWatcher<Loaded>(this);d_->progressTimer->start();updateControls();
    connect(watcher,&QFutureWatcher<Loaded>::finished,this,[this,watcher,generation,path]{
        auto result=watcher->result();watcher->deleteLater();if(generation!=d_->generation)return;
        d_->loading=false;d_->progressTimer->stop();d_->progress->hide();
        if(!result.asset){d_->status->setStyleSheet("color:#b42318");d_->status->setText(QString::fromUtf8("加载失败：")+result.error);updateControls();return;}
        d_->raw=result.asset;d_->validPath=true;d_->loadedPath=path;QFileInfo info(path);d_->loadedTime=info.lastModified();d_->loadedSize=info.size();
        d_->populating=true;d_->tree->clear();std::map<std::string,QTreeWidgetItem*> items;
        for(const auto &c:result.asset->components){auto *item=new QTreeWidgetItem;item->setText(0,utf(c.name));item->setToolTip(0,utf(c.name));item->setData(0,Qt::UserRole,utf(c.id));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,Qt::Checked);items[c.id]=item;}
        for(const auto &c:result.asset->components){auto *item=items[c.id];auto parent=items.find(c.parentId);if(parent!=items.end()&&parent->second!=item)parent->second->addChild(item);else d_->tree->addTopLevelItem(item);}
        d_->tree->expandAll();d_->name->setText(utf(result.asset->name));
        int index=result.asset->unitKnown?d_->unit->findData(result.asset->sourceUnitToMm):0;
        if(result.asset->unitKnown&&index<0){index=5;d_->custom->setValue(result.asset->sourceUnitToMm);}d_->unit->setCurrentIndex(index);
        d_->populating=false;schedulePreview();
    });
    watcher->setFuture(QtConcurrent::run([path,options]{Loaded r;try{robot3d::infrastructure::loadWorkpieceFile(path,options,r.asset,r.error);}catch(const std::exception &e){r.error=QString::fromUtf8(e.what());}catch(...){r.error=QString::fromUtf8("解析器发生未知异常");}return r;}));
}
void WorkpieceImportDialog::schedulePreview(){if(d_->populating)return;if(d_->normalCancellation)d_->normalCancellation->store(true);++d_->normalGeneration;d_->normalizing=false;d_->candidate.reset();d_->debounce->start();updateControls();}
void WorkpieceImportDialog::rebuildPreview(){
    if(!d_->raw||!d_->validPath){updateControls();return;}
    if(d_->unit->currentIndex()==0){d_->status->setText(QString::fromUtf8("单位未知：请主动选择源模型单位后预览。"));updateControls();return;}
    robot3d::domain::WorkpieceNormalizeOptions options;
    d_->normalCancellation=std::make_shared<std::atomic_bool>(false);options.cancellation=d_->normalCancellation;
    options.unitToMm=d_->unit->currentData().toDouble();if(options.unitToMm<0)options.unitToMm=d_->custom->value();
    options.upAxis=d_->up->currentData().toInt();options.forwardAxis=d_->forward->currentData().toInt();options.leftHanded=d_->leftHanded->isChecked();
    options.origin=robot3d::domain::WorkpieceOrigin(d_->origin->currentIndex());options.selectAllComponents=false;
    QTreeWidgetItemIterator it(d_->tree);for(;*it;++it)if((*it)->checkState(0)!=Qt::Unchecked)options.selectedComponentIds.push_back((*it)->data(0,Qt::UserRole).toString().toUtf8().toStdString());
    auto raw=d_->raw;auto version=d_->normalGeneration;d_->normalizing=true;d_->progress->setRange(0,0);d_->progress->show();d_->status->setText(QString::fromUtf8("正在检查单位、轴向与组件…"));updateControls();
    typedef robot3d::core::Result<robot3d::domain::WorkpieceAssetPtr> Normalized;
    auto *watcher=new QFutureWatcher<Normalized>(this);
    connect(watcher,&QFutureWatcher<Normalized>::finished,this,[this,watcher,version]{
        auto result=watcher->result();watcher->deleteLater();if(version!=d_->normalGeneration)return;
        d_->normalizing=false;d_->progress->hide();
        if(!result.ok){d_->candidate.reset();d_->status->setText(utf(result.error));updateControls();return;}
        d_->candidate=result.value;d_->preview->setAsset(result.value);
        auto a=result.value->boundsMin,b=result.value->boundsMax;QStringList warnings;for(const auto &w:result.value->warnings)warnings<<utf(w);
        d_->info->setText(QString::fromUtf8("文件：%1\n格式：%2\n源单位：%3（%4）\n转换后尺寸：\n%5 × %6 × %7 mm\n组件：%8\n三角面：%9\n源大小：%10 MiB\n封闭/自交：未检查\n\n%11")
            .arg(QFileInfo(d_->loadedPath).fileName()).arg(utf(result.value->format)).arg(d_->unit->currentText())
            .arg(d_->raw->unitKnown?(QString::fromUtf8("文件单位：")+utf(d_->raw->sourceUnit)+QString::fromUtf8("；可覆盖读入比例")):QString::fromUtf8("用户指定"))
            .arg(b.x-a.x,0,'g',8).arg(b.y-a.y,0,'g',8).arg(b.z-a.z,0,'g',8).arg(qulonglong(result.value->components.size()))
            .arg(qulonglong(robot3d::domain::workpieceTriangleCount(*result.value))).arg(double(d_->raw->sourceBytes.size())/(1024*1024),0,'f',2).arg(warnings.join("\n")));
        d_->status->setStyleSheet(warnings.isEmpty()?"":"color:#9a6700");d_->status->setText(warnings.isEmpty()?QString::fromUtf8("预览就绪。请核对尺寸、方向、原点和初始位姿；同源文件将新增实例。"):QString::fromUtf8("预览包含警告，请查看右侧详情后决定是否导入。"));updateControls();
    });
    watcher->setFuture(QtConcurrent::run([raw,options]{try{return robot3d::domain::normalizeWorkpieceAsset(*raw,options);}catch(const std::exception &e){return Normalized::failure(e.what());}catch(...){return Normalized::failure("Normalization failed");}}));
}
void WorkpieceImportDialog::updateControls(){
    d_->custom->setEnabled(d_->unit->currentData().toDouble()<0);d_->cancelLoad->setEnabled(d_->loading||d_->normalizing);
    d_->load->setEnabled(!d_->path->text().trimmed().isEmpty()&&!d_->committing);
    d_->import->setEnabled(d_->candidate&&d_->preview->ready()&&d_->validPath&&!d_->loading&&!d_->normalizing&&!d_->debounce->isActive()&&!d_->committing&&!d_->name->text().trimmed().isEmpty());
}
void WorkpieceImportDialog::commit(){
    if(!d_->candidate||!d_->preview->ready()||d_->committing||!d_->handler)return;
    QFileInfo current(d_->loadedPath);
    if(!current.exists()||current.lastModified()!=d_->loadedTime||current.size()!=d_->loadedSize){
        QMessageBox box(QMessageBox::Warning,QString::fromUtf8("源文件已变化"),QString::fromUtf8("当前预览是已读取的快照。选择导入该版本，或重新加载源文件。"),QMessageBox::NoButton,this);
        auto *old=box.addButton(QString::fromUtf8("导入已预览版本"),QMessageBox::AcceptRole);auto *reload=box.addButton(QString::fromUtf8("重新加载"),QMessageBox::ActionRole);box.addButton(QMessageBox::Cancel);box.exec();
        if(box.clickedButton()==reload){loadPreview();return;}if(box.clickedButton()!=old)return;
    }
    robot3d::domain::EulerPoseDeg pose={d_->pose[0]->value(),d_->pose[1]->value(),d_->pose[2]->value(),d_->pose[3]->value(),d_->pose[4]->value(),d_->pose[5]->value()};
    d_->committing=true;updateControls();d_->status->setText(QString::fromUtf8("正在准备场景资源…"));QString error;
    bool ok=d_->handler(d_->candidate,d_->name->text().trimmed().toUtf8().toStdString(),d_->parent->currentData().toString().toStdString(),pose,error);
    d_->committing=false;if(ok)QDialog::accept();else{d_->status->setText(QString::fromUtf8("导入失败：")+error);updateControls();}
}
