#include <faust/dsp/dsp.h>
#include <faust/gui/meta.h>
#include <faust/gui/UI.h>

class mydsp final : public dsp {

private:
    int fSampleRate;

public:
    void metadata(Meta *m) override
    {
        m->declare("name", "mydsp");
    }

    int getNumInputs() override
    {
        return 1;
    }
    int getNumOutputs() override
    {
        return 1;
    }

    static void classInit(int sampleRate)
    {
    }

    void instanceConstants(int sampleRate) override
    {
        fSampleRate = sampleRate;
    }

    void instanceResetUserInterface() override
    {
    }

    void instanceClear() override
    {
    }

    void init(int sampleRate) override
    {
        classInit(sampleRate);
        instanceInit(sampleRate);
    }
    void instanceInit(int sampleRate) override
    {
        instanceConstants(sampleRate);
        instanceResetUserInterface();
        instanceClear();
    }

    mydsp *clone() override
    {
        return new mydsp;
    }

    int getSampleRate() override
    {
        return fSampleRate;
    }

    void buildUserInterface(UI *ui_interface) override
    {
        ui_interface->openVerticalBox("mydsp");
        ui_interface->closeBox();
    }

    void compute(int count, FAUSTFLOAT **inputs, FAUSTFLOAT **outputs) override
    {
        FAUSTFLOAT *input0 = inputs[0];
        FAUSTFLOAT *output0 = outputs[0];
        for (int i = 0; i < count; ++i) {
            output0[i] = input0[i];
        }
    }

};

extern "C" {

__attribute__((visibility("default")))
dsp *createDSPInstance()
{
    return new mydsp;
}

} // extern "C"
