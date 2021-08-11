#pragma once
#include "jest_dsp.h"
#include <QDockWidget>
#include <memory>

namespace jest {

class SettingsPanel : public QDockWidget {
    Q_OBJECT

public:
    SettingsPanel();
    ~SettingsPanel();

    CompileSettings getCurrentSettings() const;
    void setCurrentSettings(const CompileSettings &cs);

signals:
    void settingsChanged();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace jest
