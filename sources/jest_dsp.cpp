#include "jest_dsp.h"
#include "utility/logs.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QTemporaryFile>
#include <QDebug>
#include <dlfcn.h>

DSPWrapper::~DSPWrapper()
{
    if (_dsp)
        delete _dsp;
    if (_soHandle)
        dlclose(_soHandle);
    if (!_soFile.isEmpty())
        QFile::remove(_soFile);
}

CompileResult DSPWrapper::compile(const CompileRequest &request)
{
    CompileResult result;
    const CompileSettings &settings = request.settings;

    Log::i("Compiling DSP");

    const QString cppFile = QString("%1/%2").arg(getCacheDirectory()).arg("file.cpp");
    QString soFile = QString("%1/%2").arg(getCacheDirectory()).arg("file.XXXXXX.so");

    {
        QTemporaryFile temp(soFile);
        if (!temp.open()) {
            Log::e("DSP compilation failed (temporary file)");
            return result;
        }
        soFile = temp.fileName();
    }

    {
        QProcess proc;
        proc.setProgram(getFaustProgram());
        QStringList args;
        args << "-o" << cppFile;
        args << "-a" << getWrapperFile();
        switch (settings.faustFloat) {
        default:
        case kCompilerSingleFloat:
            args << "-single";
            break;
        case kCompilerDoubleFloat:
            args << "-double";
            break;
        case kCompilerQuadFloat:
            args << "-quad";
            break;
        }
        if (settings.faustVec)
            args << "-vec" << "-vs" << QString::number(settings.faustVecSize);
        if (settings.faustMathApp)
            args << "-mapp";
        args << request.fileName;
        proc.setArguments(args);
        Log::i("$ %s %s", proc.program().toUtf8().constData() , proc.arguments().join(' ').toUtf8().constData());
        proc.start();
        proc.waitForFinished(-1);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            Log::e("DSP compilation failed (faust)");
            return result;
        }
    }

    {
        QProcess proc;
        proc.setProgram(getCxxProgram(settings));
        QStringList args;
        args << getCxxFlags(settings);
        args << "-shared";
        args << "-fPIC";
        args << "-o" << soFile;
        args << cppFile;
        args << getLdFlags(settings);
        proc.setArguments(args);
        Log::i("$ %s %s", proc.program().toUtf8().constData() , proc.arguments().join(' ').toUtf8().constData());
        proc.start();
        proc.waitForFinished(-1);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            Log::e("DSP compilation failed (c++)");
            return result;
        }
    }

    Log::s("DSP compilation success");

    ///
    DSPWrapperPtr wrapper(new DSPWrapper);

    void *soHandle = dlopen(soFile.toUtf8().data(), RTLD_LAZY);
    if (!soHandle) {
        Log::e("DSP loading failed: %s", dlerror());
        return result;
    }
    wrapper->_soHandle = soHandle;
    wrapper->_soFile = soFile;

    dsp *(*entry)() = (dsp *(*)())dlsym(soHandle, "createDSPInstance");
    if (!entry) {
        Log::e("DSP loading failed");
        return result;
    }

    dsp *dsp = entry();
    if (!dsp) {
        Log::e("DSP instantiation failed");
        return result;
    }
    wrapper->_dsp = dsp;

    ///
    result.dspWrapper = wrapper;
    return result;
}

const QString &DSPWrapper::getCacheDirectory()
{
    static QString dir = []() -> QString {
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        return QString("%1/%2").arg(cacheDir).arg(QCoreApplication::applicationPid());
    }();
    return dir;
}

const QString &DSPWrapper::getWrapperFile()
{
    static QString file = []() -> QString {
        const QString &cacheDir = getCacheDirectory();
        return QString("%1/wrapper.cpp").arg(cacheDir);
    }();
    return file;
}

const QString &DSPWrapper::getFaustProgram()
{
    static QString file = []() -> QString {
        const QByteArray data = qgetenv("FAUST");
        if (data.isEmpty())
            return "faust";
        return QString::fromUtf8(data);
    }();
    return file;
}

QString DSPWrapper::getCxxProgram(const CompileSettings &settings)
{
    switch (settings.cxxCompiler) {
    default:
    case kCompilerDefault:
    {
        const QByteArray data = qgetenv("CXX");
        if (data.isEmpty())
            return "c++";
        return QString::fromUtf8(data);
    }
    case kCompilerGCC:
        return "g++";
    case kCompilerClang:
        return "clang++";
    }
}

QStringList DSPWrapper::getCxxFlags(const CompileSettings &settings)
{
    QStringList args;
    args << QString("-O%1").arg(settings.cxxOpt);
    if (settings.cxxFastMath)
        args << "-ffast-math";
    return args;
}

QStringList DSPWrapper::getLdFlags(const CompileSettings &settings)
{
    QStringList args;
    (void)settings;
    return args;
}
