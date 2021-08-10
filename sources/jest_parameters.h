#pragma once
#include <faust/dsp/dsp.h>
#include <string>
#include <vector>

namespace jest {

struct Parameter {
    std::string label;
    FAUSTFLOAT *zone = nullptr;
    FAUSTFLOAT init = 0;
    FAUSTFLOAT min = 0;
    FAUSTFLOAT max = 0;
};

void collectDspParameters(dsp *dsp, std::vector<Parameter> *inputs, std::vector<Parameter> *outputs);

} // namespace jest
