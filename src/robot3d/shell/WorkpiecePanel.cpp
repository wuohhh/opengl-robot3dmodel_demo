#include "robot3d/shell/WorkpiecePanel.h"
#include "robot3d/shell/RobotViewport.h"
#include "robot3d/shell/WorkpieceImportDialog.h"
#include "robot3d/application/WorkpieceService.h"
#include "robot3d/infrastructure/WorkpieceSceneStore.h"
#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QFile>
#include <QCoreApplication>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
namespace {
QString utf(const std::string &s){return QString::fromUtf8(s.c_str());}
struct Stored {bool ok=false;QString error;robot3d::domain::WorkpieceSceneState state;};
}
struct WorkpiecePanel::Impl {
    RobotViewport *viewport;
    robot3d::application::WorkpieceService service;
    robot3d::scene::RobotSceneSnapshot robotScene;
    QTreeWidget *list;
    QLineEdit *name;
    QComboBox *parent,*mode;
    QDoubleSpinBox *pose[6],*opacity;
    QCheckBox *visible,*locked,*overrideColor,*axes,*keepWorld;
    QColor color=QColor(165,180,200);
    QLabel *info,*status;
    QPushButton *add,*save,*open,*apply,*remove,*undo,*redo,*focus,*fit,*colorButton;
    bool refreshing=false,busy=false,closeAfterSave=false;
    double draftPose[6]={0,0,0,0,0,0};
    QString path,pendingOpen;

    bool prepare(robot3d::application::WorkpieceService &candidate,QString &error){
        auto scene=robotScene;scene.workpieces=candidate.snapshot().instances;scene.selectedWorkpieceId=candidate.snapshot().selectedId;
        std::string reason;
        if(!viewport->prepareWorkpieces(scene,reason)){error=utf(reason);return false;}
        service=std::move(candidate);return true;
    }
};
WorkpiecePanel::WorkpiecePanel(RobotViewport *viewport,QWidget *parent):QWidget(parent),d_(new Impl){
    d_->viewport=viewport;setObjectName("workpiecePanel");auto *layout=new QVBoxLayout(this);
    auto *actions=new QHBoxLayout;d_->add=new QPushButton(QString::fromUtf8("导入工件…"),this);d_->add->setObjectName("importWorkpieceButton");
    d_->open=new QPushButton(QString::fromUtf8("打开…"),this);d_->save=new QPushButton(QString::fromUtf8("保存…"),this);
    actions->addWidget(d_->add);actions->addWidget(d_->open);actions->addWidget(d_->save);layout->addLayout(actions);
    d_->list=new QTreeWidget(this);d_->list->setObjectName("workpieceObjectList");d_->list->setHeaderLabels(QString::fromUtf8("显示 / 工件|锁定").split('|'));d_->list->header()->setSectionResizeMode(0,QHeaderView::Stretch);d_->list->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);d_->list->setMinimumHeight(120);d_->list->setMaximumHeight(190);layout->addWidget(d_->list);
    auto *history=new QHBoxLayout;d_->undo=new QPushButton(QString::fromUtf8("撤销"),this);d_->redo=new QPushButton(QString::fromUtf8("重做"),this);d_->remove=new QPushButton(QString::fromUtf8("删除"),this);d_->undo->setObjectName("workpieceUndo");d_->redo->setObjectName("workpieceRedo");d_->remove->setObjectName("workpieceDelete");
    history->addWidget(d_->undo);history->addWidget(d_->redo);history->addWidget(d_->remove);layout->addLayout(history);
    auto *camera=new QHBoxLayout;d_->focus=new QPushButton(QString::fromUtf8("定位选中"),this);d_->fit=new QPushButton(QString::fromUtf8("适应全部"),this);camera->addWidget(d_->focus);camera->addWidget(d_->fit);layout->addLayout(camera);
    auto *scroll=new QScrollArea(this);scroll->setWidgetResizable(true);auto *body=new QWidget(scroll);auto *form=new QFormLayout(body);scroll->setWidget(body);layout->addWidget(scroll,1);
    d_->name=new QLineEdit(body);d_->name->setObjectName("workpieceName");form->addRow(QString::fromUtf8("名称"),d_->name);
    d_->parent=new QComboBox(body);d_->parent->setObjectName("workpieceParent");form->addRow(QString::fromUtf8("父坐标系"),d_->parent);
    d_->keepWorld=new QCheckBox(QString::fromUtf8("更换父系时保持世界位置"),body);d_->keepWorld->setChecked(true);form->addRow(d_->keepWorld);
    const char *labels[]={"X (mm)","Y (mm)","Z (mm)","Rx (°)","Ry (°)","Rz (°)"};
    for(int i=0;i<6;++i){d_->pose[i]=new QDoubleSpinBox(body);d_->pose[i]->setDecimals(3);d_->pose[i]->setRange(i<3?-10000000:-360000,i<3?10000000:360000);d_->pose[i]->setKeyboardTracking(false);d_->pose[i]->setObjectName(QString("workpiecePose%1").arg(i));form->addRow(QString::fromUtf8(labels[i]),d_->pose[i]);}
    d_->visible=new QCheckBox(QString::fromUtf8("显示"),body);d_->locked=new QCheckBox(QString::fromUtf8("锁定位姿和删除"),body);d_->locked->setObjectName("workpieceLocked");d_->axes=new QCheckBox(QString::fromUtf8("显示局部坐标轴"),body);form->addRow(d_->visible);form->addRow(d_->locked);form->addRow(d_->axes);
    d_->mode=new QComboBox(body);d_->mode->addItems(QString::fromUtf8("实体|线框|实体带边线").split('|'));form->addRow(QString::fromUtf8("显示方式"),d_->mode);
    d_->opacity=new QDoubleSpinBox(body);d_->opacity->setRange(0.05,1);d_->opacity->setDecimals(2);d_->opacity->setSingleStep(.05);form->addRow(QString::fromUtf8("不透明度"),d_->opacity);
    d_->overrideColor=new QCheckBox(QString::fromUtf8("统一颜色"),body);d_->colorButton=new QPushButton(QString::fromUtf8("选择…"),body);form->addRow(d_->overrideColor,d_->colorButton);
    d_->apply=new QPushButton(QString::fromUtf8("应用属性 / 位姿"),body);d_->apply->setObjectName("workpieceApply");form->addRow(d_->apply);
    for(int i=0;i<6;++i)connect(d_->pose[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this,i](double value){if(!d_->refreshing)d_->draftPose[i]=value;});
    auto *tip=new QLabel(QString::fromUtf8("位置使用 mm，旋转采用 Rz × Ry × Rx。单位与轴向只在导入预览中修改。"),body);tip->setWordWrap(true);form->addRow(tip);
    d_->info=new QLabel(body);d_->info->setWordWrap(true);d_->info->setTextInteractionFlags(Qt::TextSelectableByMouse);form->addRow(d_->info);
    d_->status=new QLabel(QString::fromUtf8("尚无工件。导入后可独立定位、管理和保存。"),this);d_->status->setObjectName("workpieceStatus");d_->status->setWordWrap(true);layout->addWidget(d_->status);
    auto *licenses=new QPushButton(QString::fromUtf8("组件与许可…"),this);licenses->setObjectName("workpieceLicenses");layout->addWidget(licenses);
    connect(licenses,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle(QString::fromUtf8("工件导入组件与许可"));dialog.resize(760,600);auto *box=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget(&dialog);box->addWidget(tabs);
        auto *about=new QTextBrowser(tabs);about->setOpenExternalLinks(true);about->setHtml(QString::fromUtf8("<p>%1</p><p>CAD 文件通过 Open CASCADE Technology 7.9.1 解析；网格通过 Assimp 3.2 解析。模型统一交由当前场景渲染。</p><p>OCCT 采用 LGPL 2.1 与附加例外，使用动态链接，未修改内核。源代码、构建方式和二进制来源见随附说明。</p><p><a href=\"https://github.com/Open-Cascade-SAS/OCCT/tree/V7_9_1\">OCCT 7.9.1 官方源代码</a></p>").arg(robot3d::infrastructure::workpieceImportCapabilities().toHtmlEscaped()));tabs->addTab(about,QString::fromUtf8("组件"));
        const char *files[]={"OCCT-LGPL-2.1.md","OCCT-exception.md","README.md"};const char *titles[]={"LGPL 2.1","OCCT exception","Deployment"};
        for(int i=0;i<3;++i){auto *text=new QTextBrowser(tabs);QFile file(QCoreApplication::applicationDirPath()+"/licenses/"+files[i]);text->setPlainText(file.open(QIODevice::ReadOnly)?QString::fromUtf8(file.readAll()):QString::fromUtf8("许可文件缺失，请重新部署完整应用：")+files[i]);tabs->addTab(text,QString::fromLatin1(titles[i]));}
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);box->addWidget(buttons);dialog.exec();
    });
    connect(d_->add,&QPushButton::clicked,this,&WorkpiecePanel::importWorkpiece);connect(d_->save,&QPushButton::clicked,this,&WorkpiecePanel::saveScene);connect(d_->open,&QPushButton::clicked,this,&WorkpiecePanel::openScene);
    connect(d_->apply,&QPushButton::clicked,this,&WorkpiecePanel::apply);connect(d_->remove,&QPushButton::clicked,this,&WorkpiecePanel::remove);
    connect(d_->undo,&QPushButton::clicked,this,[this]{undo(false);});connect(d_->redo,&QPushButton::clicked,this,[this]{undo(true);});
    connect(d_->focus,&QPushButton::clicked,this,[this]{d_->viewport->focusWorkpiece(utf(d_->service.snapshot().selectedId));});
    connect(d_->fit,&QPushButton::clicked,viewport,&RobotViewport::fitAllWorkpieces);
    connect(d_->list,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){if(d_->refreshing)return;selectWorkpiece(item?item->data(0,Qt::UserRole).toString():QString());});
    connect(d_->list,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem *item,int column){
        if(d_->refreshing||column!=0||d_->busy)return;auto id=item->data(0,Qt::UserRole).toString().toStdString();auto *old=d_->service.find(id);if(!old)return;
        auto next=*old;next.visible=item->checkState(0)==Qt::Checked;auto candidate=d_->service;auto result=candidate.update(next,d_->robotScene.state);QString error;
        if(!result.ok||!d_->prepare(candidate,error))d_->status->setText(result.ok?error:utf(result.error));refresh();emit sceneChanged();
    });
    connect(d_->parent,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{
        if(d_->refreshing||!d_->keepWorld->isChecked())return;auto *old=d_->service.find(d_->service.snapshot().selectedId);if(!old)return;
        robot3d::domain::Matrix4d parent;if(!robot3d::domain::parentWorldMatrix(d_->parent->currentData().toString().toStdString(),d_->robotScene.state,parent))return;
        auto pose=robot3d::domain::matrixPose(robot3d::domain::multiply(robot3d::domain::rigidInverse(parent),robot3d::domain::workpieceWorldMatrix(*old,d_->robotScene.state)));
        double values[]={pose.X,pose.Y,pose.Z,pose.Rx,pose.Ry,pose.Rz};for(int i=0;i<6;++i){d_->draftPose[i]=values[i];QSignalBlocker blocker(d_->pose[i]);d_->pose[i]->setValue(values[i]);}
    });
    connect(d_->colorButton,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(d_->color,this);if(c.isValid()){d_->color=c;d_->overrideColor->setChecked(true);}});
    connect(viewport,&RobotViewport::workpieceSelected,this,&WorkpiecePanel::selectWorkpiece);
    auto *undoKey=new QShortcut(QKeySequence::Undo,this);undoKey->setContext(Qt::WidgetWithChildrenShortcut);connect(undoKey,&QShortcut::activated,this,[this]{undo(false);});
    auto *redoKey=new QShortcut(QKeySequence::Redo,this);redoKey->setContext(Qt::WidgetWithChildrenShortcut);connect(redoKey,&QShortcut::activated,this,[this]{undo(true);});
    refresh();
}
WorkpiecePanel::~WorkpiecePanel()=default;
bool WorkpiecePanel::dirty()const{return d_->service.dirty();}
const robot3d::domain::WorkpieceSceneState &WorkpiecePanel::state()const{return d_->service.snapshot();}
void WorkpiecePanel::decorateScene(robot3d::scene::RobotSceneSnapshot &scene){
    d_->robotScene=scene;d_->robotScene.workpieces.clear();d_->robotScene.selectedWorkpieceId.clear();
    bool changed=d_->service.syncRobotContext(scene.state);
    d_->add->setEnabled(!d_->busy&&bool(scene.state.configuration));
    scene.workpieces=d_->service.snapshot().instances;scene.selectedWorkpieceId=d_->service.snapshot().selectedId;
    if(changed)refresh();
}
robot3d::scene::RobotSceneSnapshot WorkpiecePanel::currentScene()const{
    auto scene=d_->robotScene;scene.workpieces=d_->service.snapshot().instances;scene.selectedWorkpieceId=d_->service.snapshot().selectedId;return scene;
}
void WorkpiecePanel::refresh(){
    d_->refreshing=true;d_->list->clear();auto selected=d_->service.snapshot().selectedId;
    for(const auto &w:d_->service.snapshot().instances){auto *item=new QTreeWidgetItem(d_->list);item->setText(0,utf(w.name));item->setText(1,w.locked?QString::fromUtf8("已锁定"):QString());item->setData(0,Qt::UserRole,utf(w.id));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(0,w.visible?Qt::Checked:Qt::Unchecked);item->setToolTip(0,utf(w.warning));if(w.id==selected)d_->list->setCurrentItem(item);}
    auto *w=d_->service.find(selected);bool editable=w&&!d_->busy;
    for(auto *widget:std::initializer_list<QWidget*>{d_->name,d_->parent,d_->mode,d_->opacity,d_->visible,d_->locked,d_->axes,d_->overrideColor,d_->colorButton,d_->apply})widget->setEnabled(editable);
    d_->parent->clear();d_->parent->addItem(QString::fromUtf8("世界"),"world");
    for(const auto &cs:d_->robotScene.state.coordinates){robot3d::domain::Matrix4d m;if(robot3d::domain::parentWorldMatrix(cs.id,d_->robotScene.state,m))d_->parent->addItem(utf(cs.name)+" ("+utf(cs.id)+")",utf(cs.id));}
    if(d_->parent->findData("b_0")<0)d_->parent->addItem(QString::fromUtf8("基座"),"b_0");
    for(auto *p:d_->pose)p->setEnabled(editable&&(!w||!w->locked));d_->parent->setEnabled(editable&&(!w||!w->locked));
    if(w){
        d_->name->setText(utf(w->name));d_->parent->setCurrentIndex(d_->parent->findData(utf(w->parentCoordinateId)));
        double v[]={w->pose.X,w->pose.Y,w->pose.Z,w->pose.Rx,w->pose.Ry,w->pose.Rz};for(int i=0;i<6;++i){d_->draftPose[i]=v[i];d_->pose[i]->setValue(v[i]);}
        d_->visible->setChecked(w->visible);d_->locked->setChecked(w->locked);d_->axes->setChecked(w->showAxes);d_->overrideColor->setChecked(w->colorOverride);d_->mode->setCurrentIndex(int(w->displayMode));d_->opacity->setValue(w->opacity);
        d_->color=QColor::fromRgbF(w->color[0],w->color[1],w->color[2]);auto a=w->asset->boundsMin,b=w->asset->boundsMax;
        QStringList warnings;for(const auto &s:w->asset->warnings)warnings<<utf(s);if(!w->warning.empty())warnings<<utf(w->warning);
        d_->info->setText(QString::fromUtf8("来源：%1\n格式：%2\n尺寸：%3 × %4 × %5 mm\n三角面：%6\n资产：%7\n实例：%8\n%9")
            .arg(utf(w->asset->sourcePath)).arg(utf(w->asset->format)).arg(b.x-a.x,0,'g',8).arg(b.y-a.y,0,'g',8).arg(b.z-a.z,0,'g',8)
            .arg(qulonglong(robot3d::domain::workpieceTriangleCount(*w->asset))).arg(utf(w->asset->id)).arg(utf(w->id)).arg(warnings.join("\n")));
    }else{d_->name->clear();d_->info->setText(QString::fromUtf8("在列表或三维场景中选择工件以编辑属性。"));}
    d_->remove->setEnabled(editable&&!w->locked);d_->focus->setEnabled(editable);d_->fit->setEnabled(!d_->service.snapshot().instances.empty()&&!d_->busy);
    d_->add->setEnabled(!d_->busy&&bool(d_->robotScene.state.configuration));d_->open->setEnabled(!d_->busy);d_->save->setEnabled(!d_->busy);
    d_->undo->setEnabled(!d_->busy&&d_->service.canUndo());d_->redo->setEnabled(!d_->busy&&d_->service.canRedo());
    d_->save->setText(d_->service.dirty()?QString::fromUtf8("保存… *"):QString::fromUtf8("保存…"));d_->refreshing=false;emit dirtyChanged(d_->service.dirty());
}
void WorkpiecePanel::importWorkpiece(){
    if(d_->busy||!d_->robotScene.state.configuration)return;
    WorkpieceImportDialog dialog(d_->robotScene.state,this);
    dialog.setCommitHandler([this](robot3d::domain::WorkpieceAssetPtr asset,const std::string &name,const std::string &parent,const robot3d::domain::EulerPoseDeg &pose,QString &error){
        auto candidate=d_->service;auto result=candidate.add(asset,name,parent,pose,d_->robotScene.state);
        if(!result.ok){error=utf(result.error);return false;}if(!d_->prepare(candidate,error))return false;
        refresh();d_->status->setText(QString::fromUtf8("工件已导入。可点击“定位选中”观察；请保存场景。"));emit sceneChanged();return true;
    });
    dialog.exec();
}
void WorkpiecePanel::selectWorkpiece(const QString &id){if(d_->busy)return;if(d_->service.select(id.toStdString())){refresh();emit sceneChanged();}}
void WorkpiecePanel::apply(){
    auto *w=d_->service.find(d_->service.snapshot().selectedId);if(!w||d_->busy)return;auto next=*w;
    next.name=d_->name->text().trimmed().toUtf8().toStdString();next.parentCoordinateId=d_->parent->currentData().toString().toStdString();
    next.pose={d_->draftPose[0],d_->draftPose[1],d_->draftPose[2],d_->draftPose[3],d_->draftPose[4],d_->draftPose[5]};
    next.visible=d_->visible->isChecked();next.locked=d_->locked->isChecked();next.showAxes=d_->axes->isChecked();next.opacity=d_->opacity->value();next.displayMode=robot3d::domain::WorkpieceDisplayMode(d_->mode->currentIndex());next.colorOverride=d_->overrideColor->isChecked();
    next.color={{float(d_->color.redF()),float(d_->color.greenF()),float(d_->color.blueF())}};
    auto candidate=d_->service;auto result=candidate.update(next,d_->robotScene.state);QString error;
    if(!result.ok||!d_->prepare(candidate,error)){d_->status->setText(result.ok?error:utf(result.error));return;}
    refresh();d_->status->setText(QString::fromUtf8("属性已应用，可撤销。"));emit sceneChanged();
}
void WorkpiecePanel::remove(){
    if(d_->busy)return;auto candidate=d_->service;auto result=candidate.remove(candidate.snapshot().selectedId);QString error;
    if(!result.ok||!d_->prepare(candidate,error)){d_->status->setText(result.ok?error:utf(result.error));return;}refresh();emit sceneChanged();
}
void WorkpiecePanel::undo(bool redo){
    if(d_->busy)return;auto candidate=d_->service;if(!(redo?candidate.redo():candidate.undo()))return;QString error;
    if(!d_->prepare(candidate,error)){d_->status->setText(error);return;}refresh();emit sceneChanged();
}
void WorkpiecePanel::saveScene(){
    if(d_->busy)return;QString path=d_->path;
    if(path.isEmpty())path=QFileDialog::getSaveFileName(this,QString::fromUtf8("保存工件场景"),QString(),"Workpiece scene (*.r3wp)");
    if(path.isEmpty()){d_->closeAfterSave=false;d_->pendingOpen.clear();return;}
    if(!path.endsWith(".r3wp",Qt::CaseInsensitive))path+=".r3wp";
    d_->busy=true;auto state=d_->service.snapshot();auto revision=state.revision;
    d_->status->setText(QString::fromUtf8("正在保存自包含场景包…"));refresh();
    auto *watcher=new QFutureWatcher<Stored>(this);connect(watcher,&QFutureWatcher<Stored>::finished,this,[this,watcher,path,revision]{
        auto r=watcher->result();watcher->deleteLater();d_->busy=false;
        if(r.ok){d_->path=path;if(d_->service.snapshot().revision==revision)d_->service.markSaved();d_->status->setText(QString::fromUtf8("已保存：")+path);}else d_->status->setText(QString::fromUtf8("保存失败：")+r.error);
        refresh();
        if(r.ok&&!d_->service.dirty()&&!d_->pendingOpen.isEmpty()){auto next=d_->pendingOpen;d_->pendingOpen.clear();startOpen(next);}
        else if(r.ok&&!d_->service.dirty()&&d_->closeAfterSave){d_->closeAfterSave=false;emit closeReady();}
        else{d_->pendingOpen.clear();d_->closeAfterSave=false;}
    });
    watcher->setFuture(QtConcurrent::run([path,state]{Stored r;try{r.ok=robot3d::infrastructure::saveWorkpieceScene(path,state,r.error);}catch(const std::exception &e){r.error=QString::fromUtf8(e.what());}return r;}));
}
void WorkpiecePanel::openScene(){
    if(d_->busy)return;auto path=QFileDialog::getOpenFileName(this,QString::fromUtf8("打开工件场景"),QString(),"Workpiece scene (*.r3wp)");if(path.isEmpty())return;
    if(d_->service.dirty()){auto answer=QMessageBox::question(this,QString::fromUtf8("工件尚未保存"),QString::fromUtf8("打开将替换当前工件集合，是否先保存？"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
        if(answer==QMessageBox::Cancel)return;if(answer==QMessageBox::Save){d_->pendingOpen=path;saveScene();return;}}
    startOpen(path);
}
void WorkpiecePanel::startOpen(const QString &path){
    d_->busy=true;d_->status->setText(QString::fromUtf8("正在读取并校验工件场景…"));refresh();auto *watcher=new QFutureWatcher<Stored>(this);
    connect(watcher,&QFutureWatcher<Stored>::finished,this,[this,watcher,path]{
        auto r=watcher->result();watcher->deleteLater();d_->busy=false;if(!r.ok){d_->status->setText(QString::fromUtf8("打开失败：")+r.error);refresh();return;}
        bool keepWorld=false;auto &robot=d_->robotScene.state;auto saved=robot3d::domain::poseMatrix(r.state.baseInWorld),current=robot3d::domain::poseMatrix(robot.baseInWorld);
        bool differs=r.state.robotId!=(robot.configuration?robot.configuration->id:std::string());
        for(int i=0;i<16;++i)differs=differs||std::abs(saved.values[i]-current.values[i])>1e-8;
        if(differs){QMessageBox box(QMessageBox::Warning,QString::fromUtf8("机械臂场景上下文不同"),QString::fromUtf8("场景包的机器人型号或基座位置与当前场景不同。选择恢复位置方式；机械臂本身不会改变。"),QMessageBox::NoButton,this);
            auto *world=box.addButton(QString::fromUtf8("保持保存的世界位置"),QMessageBox::AcceptRole);auto *relative=box.addButton(QString::fromUtf8("绑定当前父坐标系"),QMessageBox::ActionRole);box.addButton(QMessageBox::Cancel);box.exec();
            if(box.clickedButton()!=world&&box.clickedButton()!=relative){refresh();return;}keepWorld=box.clickedButton()==world;}
        auto candidate=d_->service;auto result=candidate.replace(r.state,robot,keepWorld);QString error;
        if(!result.ok||!d_->prepare(candidate,error)){d_->status->setText(result.ok?error:utf(result.error));refresh();return;}
        d_->path=path;d_->service.markSaved();refresh();d_->status->setText(QString::fromUtf8("已恢复工件场景：")+path);emit sceneChanged();
    });
    watcher->setFuture(QtConcurrent::run([path]{Stored r;try{r.ok=robot3d::infrastructure::loadWorkpieceScene(path,r.state,r.error);}catch(const std::exception &e){r.error=QString::fromUtf8(e.what());}return r;}));
}
bool WorkpiecePanel::mayClose(){
    if(d_->busy){d_->status->setText(QString::fromUtf8("文件操作进行中，请等待完成后关闭。"));return false;}
    if(!d_->service.dirty())return true;
    auto answer=QMessageBox::question(this,QString::fromUtf8("工件尚未保存"),QString::fromUtf8("是否保存工件场景后关闭？"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if(answer==QMessageBox::Discard)return true;if(answer==QMessageBox::Cancel)return false;d_->closeAfterSave=true;saveScene();return false;
}
