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
        args << request.fileName;
        proc.setArguments(args);
        proc.start();
        proc.waitForFinished(-1);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            Log::e("DSP compilation failed (faust)");
            return result;
        }
    }

    {
        QProcess proc;
        proc.setProgram(getCxxProgram());
        QStringList args;
        args << getCxxFlags();
        args << "-shared";
        args << "-fPIC";
        args << "-o" << soFile;
        args << cppFile;
        args << getLdFlags();
        proc.setArguments(args);
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

const QString &DSPWrapper::getCxxProgram()
{
    static QString file = []() -> QString {
        const QByteArray data = qgetenv("CXX");
        if (data.isEmpty())
            return "c++";
        return QString::fromUtf8(data);
    }();
    return file;
}

const QStringList &DSPWrapper::getCxxFlags()
{
    static QStringList list = []() -> QStringList {
        const QByteArray data = qgetenv("CXXFLAGS");
        if (data.isEmpty()) {
            //return {"-O0", "-g"};
            return {"-O3", "-g", "-ffast-math"};
        }
        return QString::fromUtf8(data).split(' ', Qt::SkipEmptyParts);
    }();
    return list;
}

const QStringList &DSPWrapper::getLdFlags()
{
    static QStringList list = []() -> QStringList {
        const QByteArray data = qgetenv("LDFLAGS");
        if (data.isEmpty())
            return {};
        return QString::fromUtf8(data).split(' ', Qt::SkipEmptyParts);
    }();
    return list;
}
