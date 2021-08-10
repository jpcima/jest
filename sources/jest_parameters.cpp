#include "jest_parameters.h"
#include <faust/gui/UI.h>

namespace jest {

class ParameterCollector : public UI {
public:
    using REAL = FAUSTFLOAT;

    std::vector<Parameter> *inputs = nullptr;
    std::vector<Parameter> *outputs = nullptr;

    void collectInput(const char *label, REAL *zone, REAL init, REAL min, REAL max)
    {
        if (inputs) {
            Parameter param;
            param.label.assign(label);
            param.zone = zone;
            param.init = init;
            param.min = min;
            param.max = max;
            inputs->push_back(std::move(param));
        }
    }
    void collectOutput(const char *label, REAL *zone, REAL init, REAL min, REAL max)
    {
        if (outputs) {
            Parameter param;
            param.label.assign(label);
            param.zone = zone;
            param.init = init;
            param.min = min;
            param.max = max;
            outputs->push_back(std::move(param));
        }
    }

    // -- widget's layouts
    void openTabBox(const char *label) override {}
    void openHorizontalBox(const char *label) override {}
    void openVerticalBox(const char *label) override {}
    void closeBox() override {}

    // -- active widgets
    void addButton(const char *label, REAL *zone) override { collectInput(label, zone, 0, 0, 1); }
    void addCheckButton(const char *label, REAL *zone) override { collectInput(label, zone, 0, 0, 1); }
    void addVerticalSlider(const char *label, REAL *zone, REAL init, REAL min, REAL max, REAL) override { collectInput(label, zone, init, min, max); }
    void addHorizontalSlider(const char *label, REAL *zone, REAL init, REAL min, REAL max, REAL) override { collectInput(label, zone, init, min, max); }
    void addNumEntry(const char *label, REAL *zone, REAL init, REAL min, REAL max, REAL) override { collectInput(label, zone, init, min, max); }

    // -- passive widgets
    void addHorizontalBargraph(const char *label, REAL *zone, REAL min, REAL max) override { collectOutput(label, zone, 0, min, max); }
    void addVerticalBargraph(const char *label, REAL *zone, REAL min, REAL max) override { collectOutput(label, zone, 0, min, max); }

    // -- soundfiles
    void addSoundfile(const char *, const char *, Soundfile **) override {}

    // -- metadata declarations
    void declare(REAL *, const char *, const char *) override {}
};

void collectDspParameters(dsp *dsp, std::vector<Parameter> *inputs, std::vector<Parameter> *outputs)
{
    ParameterCollector ui;
    ui.inputs = inputs;
    ui.outputs = outputs;
    dsp->buildUserInterface(&ui);
}

} // namespace jest
