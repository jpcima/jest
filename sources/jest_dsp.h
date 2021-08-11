#pragma once
#include <faust/dsp/dsp.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

class DSPWrapper;
using DSPWrapperPtr = std::shared_ptr<DSPWrapper>;

struct CompileSettings;
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
    static QString getCxxProgram(const CompileSettings &settings);
    static QStringList getCxxFlags(const CompileSettings &settings);
    static QStringList getLdFlags(const CompileSettings &settings);

private:
    void *_soHandle = nullptr;
    QString _soFile;
    dsp *_dsp = nullptr;
};

///
enum CompilerId {
    kCompilerDefault,
    kCompilerGCC,
    kCompilerClang,
};

enum CompilerFloatPrecision {
    kCompilerSingleFloat,
    kCompilerDoubleFloat,
    kCompilerQuadFloat,
};

enum {
    kCompilerVectorSizeMin = 4,
    kCompilerVectorSizeMax = 64,
};

struct CompileSettings {
    int cxxCompiler = kCompilerDefault;
    int cxxOpt = 3;
    bool cxxFastMath = true;
    int faustFloat = kCompilerSingleFloat;
    bool faustVec = false;
    int faustVecSize = 32;
    bool faustMathApp = true;
};

struct CompileRequest {
    QString fileName;
    CompileSettings settings;
    QVector<float> initialControlValues;
};
struct CompileResult {
    DSPWrapperPtr dspWrapper;
};

Q_DECLARE_METATYPE(CompileRequest)
Q_DECLARE_METATYPE(CompileResult)
