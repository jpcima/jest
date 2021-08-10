#include "jest_app.h"
#include "jest_dsp.h"
#include "jest_parameters.h"
#include "jest_worker.h"
#include "jest_client.h"
#include "utility/logs.h"
#include "ui_jest_main_window.h"
#include "faust/MyQTUI.h"
#include <nsm.h>
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
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <vector>
#include <stdexcept>

struct nsm_delete { void operator()(nsm_client_t *x) const noexcept { nsm_free(x); } };
using nsm_u = std::unique_ptr<nsm_client_t, nsm_delete>;

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

    nsm_u _nsmClient;
    bool _nsmIsOpen = false;
    QString _nsmSessionPath;
    QString _nsmDisplayName;

    void initWithArgs();
    void initWithNsm(const char *nsmUrl);

    void loadFileEx(const QString &fileName, const QVector<float> &controlValues);
    void requestCurrentFile(const QVector<float> &controlValues);
    void startedCompiling(const CompileRequest &request);
    void finishedCompiling(const CompileRequest &request, const CompileResult &result);

    ///
    class MainWindow : public QMainWindow {
    public:
        MainWindow();

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;
        void closeEvent(QCloseEvent *event) override;
    };

    ///
    static int nsmOpen(const char *name, const char *display_name, const char *client_id, char **out_msg, void *userdata);
    static int nsmSave(char **out_msg, void *userdata);
    static void nsmShowOptionalGui(void *userdata);
    static void nsmHideOptionalGui(void *userdata);
    static void nsmLog(void *userdata, const char *format, ...);
};

App::App(int &argc, char **argv)
    : QApplication(argc, argv),
      _impl(new Impl)
{
    setApplicationName("jest");
    setApplicationDisplayName("Jest");
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
    Impl::MainWindow *window = new Impl::MainWindow;
    impl._window = window;
    impl._windowUi.setupUi(window);

    QIcon icon(":/icons/jest.svg");
    window->setWindowIcon(icon);

    ///
    const char *nsmUrl = getenv("NSM_URL");
    bool isUnderNsm = nsmUrl != nullptr;
    if (isUnderNsm) {
        Log::i("Session manager at %s", nsmUrl);
        impl.initWithNsm(nsmUrl);
    }
    else
        impl.initWithArgs();

    ///
    window->setWindowTitle(applicationDisplayName());
    if (!isUnderNsm)
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

    if (isUnderNsm)
        window->setEnabled(impl._nsmIsOpen);

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
                impl.requestCurrentFile({});
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
    impl.loadFileEx(fileName, {});
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
void App::Impl::initWithArgs()
{
    App *self = static_cast<App *>(QCoreApplication::instance());

    QCommandLineParser clp;
    clp.addPositionalArgument("file", tr("The file to open."));
    clp.addHelpOption();
    clp.process(*self);

    const QStringList positional = clp.positionalArguments();
    const QString fileToLoad = positional.value(0);

    if (!fileToLoad.isEmpty())
        QMetaObject::invokeMethod(self, [self, fileToLoad]() { self->loadFile(fileToLoad); }, Qt::QueuedConnection);
}

void App::Impl::initWithNsm(const char *nsmUrl)
{
    nsm_client_t *nsmClient = nsm_new();
    if (!nsmClient)
        throw std::bad_alloc();

    _nsmClient.reset(nsmClient);

    nsm_set_open_callback(nsmClient, &nsmOpen, this);
    nsm_set_save_callback(nsmClient, &nsmSave, this);
    nsm_set_show_optional_gui_callback(nsmClient, &nsmShowOptionalGui, this);
    nsm_set_hide_optional_gui_callback(nsmClient, &nsmHideOptionalGui, this);
    nsm_set_log_callback(nsmClient, &nsmLog, this);

    if (nsm_init(nsmClient, nsmUrl) != 0)
        throw std::runtime_error("Cannot connect to session manager");

    const QByteArray nsmAppName = applicationName().toUtf8();
    const QByteArray nsmProcessName = arguments().at(0).toUtf8();
    const char *nsmCapabilities = ":optional-gui:";
    nsm_send_announce(nsmClient, nsmAppName.constData(), nsmCapabilities, nsmProcessName.constData());

    if (_window->isVisible())
        nsm_send_gui_is_shown(nsmClient);
    else
        nsm_send_gui_is_hidden(nsmClient);

    ///
    App *self = static_cast<App *>(QCoreApplication::instance());
    QTimer *nsmTimer = new QTimer(self);
    connect(nsmTimer, &QTimer::timeout, self, [nsmClient]() { nsm_check_nowait(nsmClient); });
    nsmTimer->start(50);
}

///
void App::Impl::loadFileEx(const QString &fileName, const QVector<float> &controlValues)
{
    _fileToLoad = fileName;
    _fileToLoadMtime = QFileInfo(fileName).fileTime(QFile::FileModificationTime);

    requestCurrentFile(controlValues);

    _fileCheckTimer->start();
}

void App::Impl::requestCurrentFile(const QVector<float> &controlValues)
{
    CompileRequest req;
    req.fileName = _fileToLoad;
    req.initialControlValues = controlValues;
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
    DSPWrapperPtr oldWrapper = _dspWrapper;
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

    _client.setControls(request.initialControlValues.data(), request.initialControlValues.size());
}

///
App::Impl::MainWindow::MainWindow()
{
    setAcceptDrops(true);
}

void App::Impl::MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
        event->acceptProposedAction();
}

void App::Impl::MainWindow::dropEvent(QDropEvent *event)
{
    App *self = static_cast<App *>(QCoreApplication::instance());
    const QMimeData *mimeData = event->mimeData();
    QList<QUrl> urls = mimeData->urls();

    QUrl fileUrl;
    for (int i = 0, n = urls.size(); i < n && fileUrl.isEmpty(); ++i) {
        if (urls[i].isLocalFile())
            fileUrl = urls[i];
    }

    if (!fileUrl.isEmpty()) {
        QString filePath = fileUrl.path();
        self->loadFile(filePath);
    }
}

void App::Impl::MainWindow::closeEvent(QCloseEvent *event)
{
    App *self = static_cast<App *>(QCoreApplication::instance());
    Impl &impl = *self->_impl;
    nsm_client_t *nsmClient = impl._nsmClient.get();
    if (!nsmClient)
        event->accept();
    else {
        event->ignore();
        hide();
        nsm_send_gui_is_hidden(nsmClient);
    }
}

///
int App::Impl::nsmOpen(const char *path, const char *display_name, const char *client_id, char **out_msg, void *userdata)
{
    Impl &impl = *(Impl *)userdata;
    Client &client = impl._client;
    App *self = static_cast<App *>(QCoreApplication::instance());

    Log::i("Session client ID: %s", client_id);

    client.setClientName(client_id);
    impl._nsmSessionPath = QString::fromUtf8(path);
    impl._nsmDisplayName = QString::fromUtf8(display_name);

    if (!client.ensureJackClientOpened())
        return 1;

    ///
    QFile file(impl._nsmSessionPath + ".json");
    if (file.open(QFile::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isNull()) {
            QJsonObject root = doc.object();

            QJsonArray controlValues = root["control-values"].toArray();
            int numControlValues = controlValues.size();
            QVector<float> controlValuesFloat(numControlValues);
            for (int i = 0; i < numControlValues; ++i)
                controlValuesFloat[i] = (float)controlValues[i].toDouble();

            impl.loadFileEx(root["file-path"].toString(), controlValuesFloat);
        }
    }

    ///
    impl._nsmIsOpen = true;
    impl._window->setEnabled(true);

    return 0;
}

int App::Impl::nsmSave(char **out_msg, void *userdata)
{
    Impl &impl = *(Impl *)userdata;

    ///
    QFile file(impl._nsmSessionPath + ".json");
    if (file.open(QFile::WriteOnly)) {
        QJsonObject root;
        root["file-path"] = impl._fileToLoad;

        if (DSPWrapperPtr wrapper = impl._dspWrapper) {
            std::vector<Parameter> inputParameters;
            collectDspParameters(wrapper->getDsp(), &inputParameters, nullptr);
            QJsonArray controlValues;
            for (const Parameter &parameter : inputParameters)
                controlValues.push_back(*parameter.zone);
            root["control-values"] = controlValues;
        }

        QJsonDocument doc;
        doc.setObject(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.flush();
    }

    ///
    return 0;
}

void App::Impl::nsmShowOptionalGui(void *userdata)
{
    Impl &impl = *(Impl *)userdata;
    QMainWindow *window = impl._window;
    window->show();

    nsm_client_t *nsmClient = impl._nsmClient.get();
    nsm_send_gui_is_shown(nsmClient);
}

void App::Impl::nsmHideOptionalGui(void *userdata)
{
    Impl &impl = *(Impl *)userdata;
    QMainWindow *window = impl._window;
    window->hide();

    nsm_client_t *nsmClient = impl._nsmClient.get();
    nsm_send_gui_is_hidden(nsmClient);
}

void App::Impl::nsmLog(void *userdata, const char *format, ...)
{
    (void)userdata;
    va_list ap;
    va_start(ap, format);
    Log::vi(format, ap);
    va_end(ap);
}

} // namespace jest
