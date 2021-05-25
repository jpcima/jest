#pragma once
#include <QApplication>
#include <memory>

namespace jest {

class App : public QApplication {
public:
    App(int &argc, char **argv);
    ~App();
    void init(int termPipe);
    void shutdown();
    void loadFile(const QString &fileName);
    void chooseFile();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace jest
