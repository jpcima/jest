#pragma once
#include <faust/dsp/dsp.h>
#include <QString>
#include <memory>

class DSPWrapper;
using DSPWrapperPtr = std::shared_ptr<DSPWrapper>;

class DSPWrapper {
protected:
    DSPWrapper() = default;

public:
    ~DSPWrapper();

    static DSPWrapperPtr compile(const QString &fileName);
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
