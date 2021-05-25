#include "jest_app.h"
#include "jest_dsp.h"
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
    QSocketNotifier *termPipeNotifier = nullptr;
    DSPWrapperPtr dspWrapper;
    QString dspFile;
    Client client;
    QFileSystemWatcher *watcher = nullptr;
    QMainWindow *window = nullptr;
    Ui::MainWindow windowUi;
    GUI *faustUi = nullptr;
};

App::App(int &argc, char **argv)
    : QApplication(argc, argv),
      _impl(new Impl)
{
    setApplicationName("Jest");
}

App::~App()
{
}

void App::init(int termPipe)
{
    Impl &impl = *_impl;

    if (termPipe != -1) {
        impl.termPipeNotifier = new QSocketNotifier(termPipe, QSocketNotifier::Read, this);
        connect(
            impl.termPipeNotifier, &QSocketNotifier::activated,
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
    impl.window = window;
    impl.windowUi.setupUi(window);

    window->setWindowTitle(applicationDisplayName());
    window->show();

    connect(
        impl.windowUi.actionOpen, &QAction::triggered,
        this, [this]() { chooseFile(); });

    ///
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

    DSPWrapperPtr wrapper = DSPWrapper::compile(fileName);
    impl.dspWrapper = wrapper;
    impl.dspFile = fileName;

    if (impl.watcher) {
        impl.watcher->deleteLater();
    }

    impl.watcher = new QFileSystemWatcher(this);
    impl.watcher->addPath(fileName);
    connect(
        impl.watcher, &QFileSystemWatcher::fileChanged,
        this, [this, fileName]() { Log::i("DSP file changed"); loadFile(fileName); });

    ///
    if (impl.faustUi) {
        impl.faustUi->stop();
        impl.faustUi = nullptr;
    }

    QFrame *mainFrame = impl.windowUi.mainFrame;

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

    impl.client.setDsp(wrapper);

    ///
    dsp *dsp = wrapper->getDsp();

    ///
    GUI *faustUI = QTUI_create();
    impl.faustUi = faustUI;
    dsp->buildUserInterface(faustUI);
    {
        layout->addWidget(QTUI_widget(faustUI));
        impl.window->setWindowTitle(
            QString("%1: %2").arg(applicationDisplayName())
            .arg(QFileInfo(fileName).baseName()));
        impl.window->adjustSize();
    }
    faustUI->run();
}

void App::chooseFile()
{
    Impl &impl = *_impl;

    QString fileName = QFileDialog::getOpenFileName(
        impl.window, tr("Open file"), QString(), tr("Faust DSP (*.dsp)"));

    if (fileName.isEmpty())
        return;

    loadFile(fileName);
}

} // namespace jest
