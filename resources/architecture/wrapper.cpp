#include <faust/dsp/dsp.h>
#include <faust/gui/meta.h>
#include <faust/gui/UI.h>

<<includeIntrinsic>>
<<includeclass>>

extern "C" {

__attribute__((visibility("default")))
dsp *createDSPInstance()
{
    return new FAUSTCLASS;
}

} // extern "C"
