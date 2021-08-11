#include "jest_settings_panel.h"
#include "ui_jest_settings_panel.h"

namespace jest {

struct SettingsPanel::Impl {
    Ui::SettingsPanel _ui;
    bool _notifyEdits = true;

    CompileSettings getSettingsFromUI();
    void setUIFromSettings(const CompileSettings &cs);
};

SettingsPanel::SettingsPanel()
    : _impl(new Impl)
{
    Impl &impl = *_impl;
    Ui::SettingsPanel &ui = impl._ui;

    setFeatures(QDockWidget::NoDockWidgetFeatures);
    setWindowTitle(tr("Settings"));

    QWidget *contents = new QWidget;
    ui.setupUi(contents);
    setWidget(contents);

    ///
    ui.cbCompiler->addItem(tr("default"), kCompilerDefault);
    ui.cbCompiler->addItem(tr("gcc"), kCompilerGCC);
    ui.cbCompiler->addItem(tr("clang"), kCompilerClang);

    for (int level = 0; level <= 3; ++level)
        ui.cbOptimization->addItem(QString("O%1").arg(level), level);

    ui.cbFloatPrecision->addItem(tr("single"), kCompilerSingleFloat);
    ui.cbFloatPrecision->addItem(tr("double"), kCompilerDoubleFloat);
    ui.cbFloatPrecision->addItem(tr("quad"), kCompilerQuadFloat);

    for (int vs = kCompilerVectorSizeMin; vs <= kCompilerVectorSizeMax; vs += 4)
        ui.cbVectorSize->addItem(QString::number(vs), vs);

    ///
    auto onSettingChanged = [this]() {
        Impl &impl = *_impl;
        if (impl._notifyEdits)
            emit settingsChanged();
    };

    for (QComboBox *cb : {ui.cbCompiler, ui.cbOptimization, ui.cbFloatPrecision, ui.cbVectorSize})
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onSettingChanged);
    for (QAbstractButton *btn : {ui.chkFastMath, ui.chkVectorize, ui.chkMathApp})
        connect(btn, &QAbstractButton::toggled, this, onSettingChanged);

    ///
    impl.setUIFromSettings(CompileSettings());
}

SettingsPanel::~SettingsPanel()
{
}

CompileSettings SettingsPanel::getCurrentSettings() const
{
    Impl &impl = *_impl;
    return impl.getSettingsFromUI();
}

void SettingsPanel::setCurrentSettings(const CompileSettings &cs)
{
    Impl &impl = *_impl;
    impl.setUIFromSettings(cs);
}

CompileSettings SettingsPanel::Impl::getSettingsFromUI()
{
    CompileSettings cs;
    cs.cxxCompiler = _ui.cbCompiler->currentData().toInt();
    cs.cxxOpt = _ui.cbOptimization->currentData().toInt();
    cs.cxxFastMath = _ui.chkFastMath->isChecked();
    cs.faustFloat = _ui.cbFloatPrecision->currentData().toInt();
    cs.faustVec = _ui.chkVectorize->isChecked();
    cs.faustVecSize = _ui.cbVectorSize->currentData().toInt();
    cs.faustMathApp = _ui.chkMathApp->isChecked();
    return cs;
}

void SettingsPanel::Impl::setUIFromSettings(const CompileSettings &cs)
{
    _ui.cbCompiler->setCurrentIndex(_ui.cbCompiler->findData(cs.cxxCompiler));
    _ui.cbOptimization->setCurrentIndex(_ui.cbOptimization->findData(cs.cxxOpt));
    _ui.chkFastMath->setChecked(cs.cxxFastMath);
    _ui.cbFloatPrecision->setCurrentIndex(_ui.cbFloatPrecision->findData(cs.faustFloat));
    _ui.chkVectorize->setChecked(cs.faustVec);
    _ui.cbVectorSize->setCurrentIndex(_ui.cbVectorSize->findData(cs.faustVecSize));
    _ui.chkMathApp->setChecked(cs.faustMathApp);
}

} // namespace jest
