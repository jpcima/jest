#include "jest_app.h"
#include "jest_dsp.h"
#include "jest_worker.h"
#include "jest_client.h"
#include "utility/logs.h"
#include "ui_jest_main_window.h"
#include "faust/MyQTUI.h"
#include <unistd.h>
#include <QFileDialog>
#include <QSocketNotifier>
#include <QCommandLineParser>
#include <QResource>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QFileSystemWatcher>
#include <QDebug>

namespace jest {

struct App::Impl {
    QSocketNotifier *_termPipeNotifier = nullptr;
    DSPWrapperPtr _dspWrapper;
    Worker *_worker = nullptr;
    Client _client;
    QFileSystemWatcher *_watcher = nullptr;
    QMainWindow *_window = nullptr;
    Ui::MainWindow _windowUi;
    GUI *_faustUi = nullptr;

    void startedCompiling(const CompileRequest &request);
    void finishedCompiling(const CompileRequest &request, const CompileResult &result);
};

App::App(int &argc, char **argv)
    : QApplication(argc, argv),
      _impl(new Impl)
{
    setApplicationName("Jest");
}

App::~App()
{
    Impl &impl = *_impl;

    delete impl._worker;
}

void App::init(int termPipe)
{
    Impl &impl = *_impl;

    if (termPipe != -1) {
        impl._termPipeNotifier = new QSocketNotifier(termPipe, QSocketNotifier::Read, this);
        connect(
            impl._termPipeNotifier, &QSocketNotifier::activated,
            this, [this](QSocketDescriptor socket, QSocketNotifier::Type type) {
                Log::i("Interrupt");
                char byte;
                read(socket, &byte, 1);
                quit();
            });
    }

    ///
    QCommandLineParser clp;
    clp.addPositionalArgument("file", tr("The file to open."));
    clp.addHelpOption();
    clp.process(*this);

    const QStringList positional = clp.positionalArguments();
    const QString fileToLoad = positional.value(0);

    ///
    const QString &cacheDir = DSPWrapper::getCacheDirectory();
    Log::i("Creating the cache directory");
    QDir(cacheDir).mkpath(".");

    const QString &wrapperFile = DSPWrapper::getWrapperFile();
    Log::i("Creating the wrapper file");
    {
        QFile out(wrapperFile);
        out.open(QFile::WriteOnly);
        out.write(QResource("architecture/wrapper.cpp").uncompressedData());
    }

    ///
    QMainWindow *window = new QMainWindow;
    impl._window = window;
    impl._windowUi.setupUi(window);

    window->setWindowTitle(applicationDisplayName());
    window->show();

    connect(
        impl._windowUi.actionOpen, &QAction::triggered,
        this, [this]() { chooseFile(); });

    ///
    impl._worker = new Worker(this);

    connect(
        impl._worker, &Worker::startedCompiling,
        this, [&impl](const CompileRequest &request) { impl.startedCompiling(request); });
    connect(
        impl._worker, &Worker::finishedCompiling,
        this, [&impl](const CompileRequest &request, const CompileResult &result) { impl.finishedCompiling(request, result); });

    if (!fileToLoad.isEmpty())
        loadFile(fileToLoad);
}

void App::shutdown()
{
    const QString &cacheDir = DSPWrapper::getCacheDirectory();
    Log::i("Deleting the cache directory");
    QDir(cacheDir).removeRecursively();
}

void App::loadFile(const QString &fileName)
{
    Impl &impl = *_impl;

    CompileRequest req;
    req.fileName = fileName;
    impl._worker->request(req);
}

void App::chooseFile()
{
    Impl &impl = *_impl;

    QString fileName = QFileDialog::getOpenFileName(
        impl._window, tr("Open file"), QString(), tr("Faust DSP (*.dsp)"));

    if (fileName.isEmpty())
        return;

    loadFile(fileName);
}

///
void App::Impl::startedCompiling(const CompileRequest &request)
{
    if (_watcher) {
        _watcher->deleteLater();
    }

    App *self = static_cast<App *>(qApp);
    const QString &fileName = request.fileName;

    _watcher = new QFileSystemWatcher(self);
    _watcher->addPath(fileName);
    connect(
        _watcher, &QFileSystemWatcher::fileChanged,
        self, [self, fileName]() { Log::i("DSP file changed"); self->loadFile(fileName); });
}

void App::Impl::finishedCompiling(const CompileRequest &request, const CompileResult &result)
{
    DSPWrapperPtr wrapper = result.dspWrapper;

    _dspWrapper = wrapper;

    ///
    if (_faustUi) {
        _faustUi->stop();
        _faustUi = nullptr;
    }

    QFrame *mainFrame = _windowUi.mainFrame;

    QVBoxLayout *layout = static_cast<QVBoxLayout *>(mainFrame->layout());
    if (!layout) {
      layout = new QVBoxLayout;
      layout->setContentsMargins(0, 0, 0, 0);
      mainFrame->setLayout(layout);
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    if (!wrapper)
      return;

    _client.setDsp(wrapper);

    ///
    dsp *dsp = wrapper->getDsp();

    ///
    GUI *faustUI = QTUI_create();
    _faustUi = faustUI;
    dsp->buildUserInterface(faustUI);
    {
        layout->addWidget(QTUI_widget(faustUI));
        _window->setWindowTitle(
            QString("%1: %2").arg(applicationDisplayName())
            .arg(QFileInfo(request.fileName).baseName()));
        _window->adjustSize();
    }
    faustUI->run();
}

} // namespace jest
