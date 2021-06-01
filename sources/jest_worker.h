#pragma once
#include "jest_dsp.h"
#include <QObject>
#include <memory>

namespace jest {

class Worker : public QObject {
    Q_OBJECT

public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

    void request(const CompileRequest &request);

signals:
    void startedCompiling(const CompileRequest &request);
    void finishedCompiling(const CompileRequest &request, const CompileResult &result);

    // private use
    void startedCompilingPrivate(const CompileRequest &request);
    void finishedCompilingPrivate(const CompileRequest &request, const CompileResult &result);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace jest
