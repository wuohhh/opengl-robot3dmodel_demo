#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif
#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include "robot3d/application/WorkpieceService.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace robot3d;
std::set<QString> protectedInputs;
QString inputIdentity(const QString&path) {
    const QFileInfo info(path);QString identity=info.canonicalFilePath();
    if(identity.isEmpty())identity=QDir::cleanPath(info.absoluteFilePath());
#if defined(_WIN32)
    identity=identity.toCaseFolded();
#endif
    return identity;
}
struct Memory {
    bool available=false;
    quint64 privateBytes=0,workingSetBytes=0,peakWorkingSetBytes=0;
};
Memory processMemory() {
    Memory value;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters={};counters.cb=sizeof(counters);
    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))) {
        value.available=true;value.privateBytes=quint64(counters.PrivateUsage);
        value.workingSetBytes=quint64(counters.WorkingSetSize);value.peakWorkingSetBytes=quint64(counters.PeakWorkingSetSize);
    }
#endif
    return value;
}
QJsonObject memoryJson(const Memory&value) {
    QJsonObject out;out["available"]=value.available;
    if(value.available) {
        out["privateBytes"]=double(value.privateBytes);out["workingSetBytes"]=double(value.workingSetBytes);
        out["processLifetimePeakWorkingSetBytes"]=double(value.peakWorkingSetBytes);
    }
    return out;
}
void maximum(std::atomic<quint64>&target,quint64 value) {
    auto previous=target.load();
    while(previous<value&&!target.compare_exchange_weak(previous,value)) {}
}
class MemorySampler {
public:
    MemorySampler() {
        sample();worker_=std::thread([this] {
            while(!stop_.load()) {sample();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            sample();
        });
    }
    ~MemorySampler(){finish();}
    void finish() {stop_.store(true);if(worker_.joinable())worker_.join();}
    QJsonObject result()const {
        QJsonObject out;out["available"]=available_.load();out["sampleIntervalMs"]=10;
        out["sampledPeakPrivateBytes"]=double(privatePeak_.load());
        out["sampledPeakWorkingSetBytes"]=double(workingPeak_.load());
        out["processLifetimePeakWorkingSetBytes"]=double(osWorkingPeak_.load());
        out["privatePeakIsSampled"]=true;return out;
    }
private:
    void sample() {
        const auto m=processMemory();if(!m.available)return;available_.store(true);
        maximum(privatePeak_,m.privateBytes);maximum(workingPeak_,m.workingSetBytes);maximum(osWorkingPeak_,m.peakWorkingSetBytes);
    }
    std::atomic_bool stop_{false},available_{false};
    std::atomic<quint64> privatePeak_{0},workingPeak_{0},osWorkingPeak_{0};
    std::thread worker_;
};
domain::RobotStateSnapshot benchmarkRobot() {
    domain::RobotStateSnapshot state;std::shared_ptr<domain::RobotConfiguration> config(new domain::RobotConfiguration);
    config->id="cpu-benchmark-static-world";state.configuration=config;return state;
}
struct Trial {
    bool ok=false;
    QString error;
    QJsonObject record;
    double loadMs=0,normalizationMs=0,totalMs=0;
    quint64 triangles=0,vertices=0;
};
Trial runTrial(const QString&path,double fallbackUnit,bool forceUnit,application::WorkpieceService&service) {
    Trial trial;QElapsedTimer all,phase;all.start();phase.start();
    infrastructure::WorkpieceLoadOptions loadOptions;
    std::shared_ptr<domain::WorkpieceAsset> raw;QString error;
    if(!infrastructure::loadWorkpieceFile(path,loadOptions,raw,error)) {
        trial.error=error;trial.record["failedPhase"]="load";trial.loadMs=phase.nsecsElapsed()/1000000.0;
    } else {
        trial.loadMs=phase.nsecsElapsed()/1000000.0;
        protectedInputs.insert(inputIdentity(path));
        for(const auto&dependency:raw->dependencies) {
            const QString reference=QString::fromUtf8(dependency.relativePath.c_str());
            protectedInputs.insert(inputIdentity(QDir(QFileInfo(path).absolutePath()).absoluteFilePath(reference)));
        }
        trial.record["format"]=QString::fromUtf8(raw->format.c_str());trial.record["adapterVersion"]=QString::fromUtf8(raw->adapterVersion.c_str());
        trial.record["sourceUnitKnown"]=raw->unitKnown;trial.record["sourceUnit"]=QString::fromUtf8(raw->sourceUnit.c_str());
        trial.record["sourceSnapshotBytes"]=double(raw->sourceBytes.size());trial.record["dependencyCount"]=int(raw->dependencies.size());
        trial.record["unitPolicy"]=forceUnit?"explicit_cli_override":raw->unitKnown?"trusted_source_unit":"benchmark_explicit_fallback_mm";
        domain::WorkpieceNormalizeOptions normalization;normalization.unitToMm=forceUnit||!raw->unitKnown?fallbackUnit:0;
        phase.restart();const auto normalized=domain::normalizeWorkpieceAsset(*raw,normalization);
        trial.normalizationMs=phase.nsecsElapsed()/1000000.0;
        if(!normalized.ok) {
            trial.error=QString::fromUtf8(normalized.error.c_str());trial.record["failedPhase"]="normalize";
        } else {
            trial.triangles=domain::workpieceTriangleCount(*normalized.value);
            for(const auto&c:normalized.value->components)for(const auto&m:c.meshes)trial.vertices+=m.vertices.size();
            trial.record["unitToMmUsed"]=normalized.value->sourceUnitToMm;
            trial.record["triangleCount"]=double(trial.triangles);trial.record["vertexCount"]=double(trial.vertices);
            trial.record["assetId"]=QString::fromUtf8(normalized.value->id.c_str());
            phase.restart();const auto added=service.add(normalized.value,"Benchmark","world",{},benchmarkRobot());
            trial.record["addMs"]=phase.nsecsElapsed()/1000000.0;
            if(!added.ok) {
                trial.error=QString::fromUtf8(added.error.c_str());trial.record["failedPhase"]="add";
            } else {
                phase.restart();const auto removed=service.remove(added.value);trial.record["removeMs"]=phase.nsecsElapsed()/1000000.0;
                if(!removed.ok||!service.snapshot().instances.empty()) {
                    trial.error=removed.ok?QString::fromUtf8("删除后场景仍有实例"):QString::fromUtf8(removed.error.c_str());
                    trial.record["failedPhase"]="remove";
                } else trial.ok=true;
            }
        }
    }
    raw.reset();trial.totalMs=all.nsecsElapsed()/1000000.0;trial.record["ok"]=trial.ok;trial.record["loadMs"]=trial.loadMs;
    trial.record["normalizationMs"]=trial.normalizationMs;trial.record["totalMs"]=trial.totalMs;
    if(!trial.error.isEmpty())trial.record["error"]=trial.error;
    return trial;
}
QJsonObject distribution(std::vector<double> values) {
    QJsonObject result;result["count"]=int(values.size());if(values.empty())return result;
    std::sort(values.begin(),values.end());const auto middle=values.size()/2;
    result["medianMs"]=values.size()%2?values[middle]:(values[middle-1]+values[middle])/2;
    result["maximumMs"]=values.back();result["minimumMs"]=values.front();return result;
}
bool generateGrid(const QString&path,int columns,int rows,QString&error) {
    QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::Text)){error=file.errorString();return false;}
    QTextStream out(&file);out.setCodec("UTF-8");out.setLocale(QLocale::c());
    out.setRealNumberNotation(QTextStream::FixedNotation);out.setRealNumberPrecision(6);
    out<<"ply\nformat ascii 1.0\ncomment robot3d generated CPU benchmark only\nelement vertex "<<(columns+1)*(rows+1)
       <<"\nproperty float x\nproperty float y\nproperty float z\nelement face "<<columns*rows*2
       <<"\nproperty list uchar int vertex_indices\nend_header\n";
    for(int y=0;y<=rows;++y)for(int x=0;x<=columns;++x)
        out<<x*10<<' '<<y*10<<' '<<std::sin(double(x)*.05)*std::cos(double(y)*.03)*5.0<<'\n';
    for(int y=0;y<rows;++y)for(int x=0;x<columns;++x) {
        const int a=y*(columns+1)+x,b=a+1,c=a+columns+1,d=c+1;
        out<<"3 "<<a<<' '<<b<<' '<<d<<"\n3 "<<a<<' '<<d<<' '<<c<<'\n';
    }
    out.flush();if(out.status()!=QTextStream::Ok||!file.flush()){error=file.errorString();return false;}return true;
}
QJsonObject benchmark(const QString&path,double fallbackUnit,bool forceUnit,bool&allOk) {
    QJsonObject result;result["path"]=QFileInfo(path).absoluteFilePath();result["fileBytes"]=double(QFileInfo(path).size());
    result["beforeModel"]=memoryJson(processMemory());MemorySampler sampler;
    {
        application::WorkpieceService warmup;auto trial=runTrial(path,fallbackUnit,forceUnit,warmup);
        result["firstParseAndWarmup"]=trial.record;
        if(!trial.ok) {
            allOk=false;result["ok"]=false;result["error"]=trial.error;sampler.finish();result["memoryPeak"]=sampler.result();return result;
        }
        result["triangleCount"]=double(trial.triangles);result["vertexCount"]=double(trial.vertices);
    }
    QJsonArray samples;std::vector<double> loading,normalization,total;bool modelOk=true;
    for(int i=0;i<5;++i) {
        application::WorkpieceService independent;auto trial=runTrial(path,fallbackUnit,forceUnit,independent);
        trial.record["sample"]=i+1;samples.append(trial.record);
        if(!trial.ok){modelOk=false;break;}
        loading.push_back(trial.loadMs);normalization.push_back(trial.normalizationMs);total.push_back(trial.totalMs);
    }
    result["timingSamples"]=samples;QJsonObject timing;
    timing["load"]=distribution(loading);timing["normalization"]=distribution(normalization);timing["total"]=distribution(total);
    result["timingSummary"]=timing;
    const auto beforeCycles=processMemory();result["beforeLifecycle"]=memoryJson(beforeCycles);
    application::WorkpieceService lifecycle;QJsonArray cycles;int completed=0;
    if(modelOk)for(int i=0;i<50;++i) {
        auto trial=runTrial(path,fallbackUnit,forceUnit,lifecycle);trial.record["cycle"]=i+1;
        // runTrial has released its transient raw and normalized candidates.
        // The service intentionally still owns its bounded undo history.
        trial.record["afterTransientRelease"]=memoryJson(processMemory());cycles.append(trial.record);
        if(!trial.ok){modelOk=false;break;}++completed;
        std::cout<<"  cycle "<<completed<<"/50: "<<trial.totalMs<<" ms"<<std::endl;
        QCoreApplication::processEvents();
    }
    result["lifecycleCycles"]=cycles;result["completedCycles"]=completed;result["undoHistoryLimit"]=20;
    const auto withHistory=processMemory();result["afterLifecycleWithUndoHistory"]=memoryJson(withHistory);
    lifecycle=application::WorkpieceService();QCoreApplication::processEvents();const auto afterClear=processMemory();
    result["afterUndoHistoryRelease"]=memoryJson(afterClear);
    if(beforeCycles.available&&withHistory.available&&afterClear.available) {
        result["privateDeltaWithUndoHistoryBytes"]=double(qint64(withHistory.privateBytes)-qint64(beforeCycles.privateBytes));
        result["privateDeltaAfterUndoHistoryReleaseBytes"]=double(qint64(afterClear.privateBytes)-qint64(beforeCycles.privateBytes));
    }
    sampler.finish();result["memoryPeak"]=sampler.result();result["ok"]=modelOk&&completed==50;
    if(!modelOk||completed!=50)allOk=false;return result;
}
}

int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);QCoreApplication::setApplicationName("robot3d-workpiece-cpu-benchmark");
    const auto args=app.arguments();QString output;QStringList paths;bool generated=false,forceUnit=false;double unit=1;
    try {
        if(args.size()==1)generated=true;
        else if(args[1]=="--generated") {
            generated=true;if(args.size()>2)output=args[2];
            if(args.size()>3)throw std::runtime_error("Usage: --generated [output-json]");
        } else if(args.size()>=3) {
            paths<<QFileInfo(args[1]).absoluteFilePath();output=args[2];
            if(args.size()>3) {
                bool valid=false;if(args.size()!=5||args[3]!="--unit-to-mm")throw std::runtime_error("Usage: model-file output-json [--unit-to-mm positive-number]");
                unit=args[4].toDouble(&valid);if(!valid||!std::isfinite(unit)||unit<=0)throw std::runtime_error("Invalid --unit-to-mm");forceUnit=true;
            }
        } else throw std::runtime_error("Usage: [model-file output-json [--unit-to-mm positive-number]] or --generated [output-json]");
        const QString controlled=QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../workpiece-benchmark-validation"));
        if(output.isEmpty())output=QDir(controlled).filePath("cpu-benchmark.json");output=QFileInfo(output).absoluteFilePath();
        if(!QDir().mkpath(QFileInfo(output).absolutePath()))throw std::runtime_error("Cannot create benchmark output directory");
        if(generated) {
            const QString assets=QDir(controlled).filePath("generated");if(!QDir().mkpath(assets))throw std::runtime_error("Cannot create generated fixture directory");
            const auto small=QDir(assets).filePath("small-40-triangles.ply"),medium=QDir(assets).filePath("medium-150000-triangles.ply");QString error;
            if(!generateGrid(small,5,4,error)||!generateGrid(medium,300,250,error))throw std::runtime_error(error.toUtf8().constData());
            paths<<small<<medium;
        }
        QJsonObject report;report["schemaVersion"]=1;report["capturedAtUtc"]=QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss.zzz'Z'");
        report["scope"]="CPU fresh parser invocation, normalization and service add/remove; no GPU upload, viewport FPS or robot motion benchmark";
        report["cachePolicy"]="Each trial creates a new importer and re-parses source. Application geometry cache is bypassed; OS filesystem cache is not evicted or controlled.";
        report["memoryPolicy"]="PrivateUsage and working set sampled every 10 ms; OS peak working set is a process-lifetime high-water mark. Undo retention and post-history-release memory are reported separately; allocator retention is not classified automatically as a leak.";
        report["samplePolicy"]="One first parse/warmup, five independent timing samples, then fifty fresh-parse add/remove lifecycle cycles per model. Timing summary reports median/max only.";
        report["generatedFixtures"]=generated;report["generatedFixtureDirectory"]=generated?QDir(controlled).filePath("generated"):QString();
        report["fallbackUnitToMm"]=unit;report["forceUnitOverride"]=forceUnit;
        report["unknownUnitPolicy"]="This noninteractive benchmark explicitly assumes mm (or supplied --unit-to-mm). The application UI still requires the user to confirm unknown units.";
        report["importCapabilities"]=infrastructure::workpieceImportCapabilities();
        report["pointerBits"]=int(sizeof(void*)*8);
#ifdef _MSC_VER
        report["msvcVersion"]=_MSC_VER;
#endif
#ifdef NDEBUG
        report["configuration"]="Release";
#else
        report["configuration"]="Debug";
#endif
        report["initialProcessMemory"]=memoryJson(processMemory());bool ok=true;QJsonArray models;
        for(const auto&path:paths)protectedInputs.insert(inputIdentity(path));
        for(const auto&path:paths) {
            std::cout<<"Benchmark: "<<path.toUtf8().constData()<<std::endl;
            try {models.append(benchmark(path,unit,forceUnit,ok));}
            catch(const std::exception&e) {
                QJsonObject failed;failed["path"]=path;failed["ok"]=false;failed["error"]=QString::fromUtf8(e.what());models.append(failed);ok=false;
            }
        }
        report["models"]=models;report["ok"]=ok;report["finalProcessMemory"]=memoryJson(processMemory());
        if(protectedInputs.count(inputIdentity(output)))throw std::runtime_error("Report path aliases a model or dependency; source files are read-only");
        QSaveFile destination(output);destination.setDirectWriteFallback(false);
        const auto json=QJsonDocument(report).toJson(QJsonDocument::Indented);
        if(!destination.open(QIODevice::WriteOnly)||destination.write(json)!=json.size()||!destination.commit())
            throw std::runtime_error("Cannot atomically save benchmark JSON");
        std::cout<<"Report: "<<output.toUtf8().constData()<<std::endl;return ok?0:1;
    }catch(const std::exception&e) {std::cerr<<"Benchmark error: "<<e.what()<<std::endl;return 2;}
}
