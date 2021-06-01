#include "jest_app.h"
#include "jest_dsp.h"
#include "jest_worker.h"
#include "jest_client.h"
#include "utility/logs.h"
#include "ui_jest_main_window.h"
#include "faust/MyQTUI.h"
#include <unistd.h>
#include <QProgressIndicator>
#include <QLabel>
#include <QFileDialog>
#include <QSocketNotifier>
#include <QCommandLineParser>
#include <QResource>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

namespace jest {

struct App::Impl {
    QSocketNotifier *_termPipeNotifier = nullptr;
    DSPWrapperPtr _dspWrapper;
    Worker *_worker = nullptr;
    Client _client;
    QMainWindow *_window = nullptr;
    Ui::MainWindow _windowUi;
    QProgressIndicator *_spinner = nullptr;
    QLabel *_statusLabel = nullptr;
    GUI *_faustUi = nullptr;
    QString _fileToLoad;
    QDateTime _fileToLoadMtime;
    QTimer *_fileCheckTimer = nullptr;

    void requestCurrentFile();
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

    QToolBar *toolBar = impl._windowUi.toolBar;

    QWidget *toolBarSpacer = new QWidget;
    toolBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(toolBarSpacer);

    QLabel *statusLabel = new QLabel(tr("Init"));
    impl._statusLabel = statusLabel;
    toolBar->addWidget(statusLabel);
    {
        QFont font = statusLabel->font();
        font.setWeight(QFont::Bold);
        statusLabel->setFont(font);
    }

    statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    statusLabel->setFrameShape(QFrame::Panel);
    statusLabel->setFrameShadow(QFrame::Sunken);
    statusLabel->setAutoFillBackground(true);

    QPalette statusPalette = statusLabel->palette();
    statusPalette.setColor(statusLabel->foregroundRole(), Qt::lightGray);
    statusPalette.setColor(statusLabel->backgroundRole(), Qt::black);
    statusLabel->setPalette(statusPalette);

    QProgressIndicator *spinner = new QProgressIndicator;
    impl._spinner = spinner;
    toolBar->addWidget(spinner);

    ///
    QTimer *fileCheckTimer = new QTimer(this);
    impl._fileCheckTimer = fileCheckTimer;
    fileCheckTimer->setInterval(100);
    connect(
        fileCheckTimer, &QTimer::timeout,
        this, [&impl]() {
            QDateTime mtime = QFileInfo(impl._fileToLoad).fileTime(QFile::FileModificationTime);
            if (mtime.isValid() && mtime != impl._fileToLoadMtime) {
                Log::i("DSP file changed");
                impl._fileToLoadMtime = mtime;
                impl.requestCurrentFile();
            }
        });

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

    impl._fileToLoad = fileName;
    impl._fileToLoadMtime = QFileInfo(fileName).fileTime(QFile::FileModificationTime);

    impl.requestCurrentFile();

    impl._fileCheckTimer->start();
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
void App::Impl::requestCurrentFile()
{
    CompileRequest req;
    req.fileName = _fileToLoad;
    _worker->request(req);
}

void App::Impl::startedCompiling(const CompileRequest &request)
{
    _spinner->startAnimation();
}

void App::Impl::finishedCompiling(const CompileRequest &request, const CompileResult &result)
{
    _spinner->stopAnimation();

    DSPWrapperPtr wrapper = result.dspWrapper;

    _dspWrapper = wrapper;

    ///
    if (_faustUi) {
        _faustUi->stop();
        _faustUi = nullptr;
    }

    QLabel *statusLabel = _statusLabel;
    statusLabel->setText(wrapper ? tr("Success") : tr("Error"));

    QPalette statusPalette = statusLabel->palette();
    statusPalette.setColor(statusLabel->foregroundRole(), wrapper ? Qt::green : Qt::red);
    statusPalette.setColor(statusLabel->backgroundRole(), Qt::black);
    statusLabel->setPalette(statusPalette);

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
