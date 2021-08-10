#pragma once
#include <faust/dsp/dsp.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

class DSPWrapper;
using DSPWrapperPtr = std::shared_ptr<DSPWrapper>;

struct CompileRequest;
struct CompileResult;

class DSPWrapper {
protected:
    DSPWrapper() = default;

public:
    ~DSPWrapper();

    static CompileResult compile(const CompileRequest &request);
    dsp *getDsp() noexcept { return _dsp; }

    static const QString &getCacheDirectory();
    static const QString &getWrapperFile();
    static const QString &getFaustProgram();
    static const QString &getCxxProgram();
    static const QStringList &getCxxFlags();
    static const QStringList &getLdFlags();

private:
    void *_soHandle = nullptr;
    QString _soFile;
    dsp *_dsp = nullptr;
};

///
struct CompileRequest {
    QString fileName;
    QVector<float> initialControlValues;
};
struct CompileResult {
    DSPWrapperPtr dspWrapper;
};

Q_DECLARE_METATYPE(CompileRequest)
Q_DECLARE_METATYPE(CompileResult)
