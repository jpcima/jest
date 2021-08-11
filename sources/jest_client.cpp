#include "jest_client.h"
#include "jest_dsp.h"
#include "jest_parameters.h"
#include "utility/logs.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace jest {

Client::Client()
{
}

Client::~Client()
{
    if (jack_client_t *client = _lazyClient)
        jack_client_close(client);
}

void Client::setDsp(DSPWrapperPtr dspWrapper)
{
    jack_client_t *client = getJackClient();

    ///
    size_t oldNumInputs = _inputs.size();
    size_t oldNumOutputs = _outputs.size();
    std::vector<std::vector<std::string>> inputConnections(oldNumInputs);
    std::vector<std::vector<std::string>> outputConnections(oldNumOutputs);
    for (size_t i = 0; i < oldNumInputs; ++i)
        inputConnections[i] = saveJackConnections(_inputs[i]);
    for (size_t i = 0; i < oldNumOutputs; ++i)
        outputConnections[i] = saveJackConnections(_outputs[i]);

    ///
    jack_deactivate(client);

    _dspWrapper = dspWrapper;

    unsigned sampleRate = jack_get_sample_rate(client);

    dsp *dsp = dspWrapper ? dspWrapper->getDsp() : nullptr;
    if (dsp) {
        Log::i("Initialize DSP");
        dsp->init(sampleRate);
    }

    Log::i("Update JACK I/O");
    updateJackIOs();

    jack_activate(client);

    ///
    size_t newNumInputs = _inputs.size();
    size_t newNumOutputs = _outputs.size();

    Log::s("%zu inputs, %zu outputs", newNumInputs, newNumOutputs);

    for (size_t i = 0; i < std::min(oldNumInputs, newNumInputs); ++i)
        restoreJackConnections(_inputs[i], inputConnections[i]);
    for (size_t i = 0; i < std::min(oldNumOutputs, newNumOutputs); ++i)
        restoreJackConnections(_outputs[i], outputConnections[i]);
}

void Client::setControls(const float *initialValues, size_t numInitialValues)
{
    DSPWrapperPtr dspWrapper = _dspWrapper;
    dsp *dsp = dspWrapper ? dspWrapper->getDsp() : nullptr;
    if (dsp && numInitialValues > 0) {
        std::vector<Parameter> inputParameters;
        collectDspParameters(dsp, &inputParameters, nullptr);
        for (size_t i = 0; i < numInitialValues && i < inputParameters.size(); ++i) {
            float lo = inputParameters[i].min;
            float hi = inputParameters[i].max;
            *inputParameters[i].zone = std::max(lo, std::min(hi, initialValues[i]));
        }
    }
}

void Client::setClientName(const std::string &clientName)
{
    _clientName = clientName;
}

jack_client_t *Client::getJackClient()
{
    jack_client_t *client = _lazyClient;
    if (client)
        return client;

    Log::i("Opening JACK client");

    client = jack_client_open(_clientName.c_str(), JackNoStartServer, nullptr);
    if (!client) {
        panic("Could not open JACK client");
    }

    unsigned sampleRate = jack_get_sample_rate(client);
    Log::s("New JACK client at %u Hz sample rate", sampleRate);

    jack_set_process_callback(client, &process, this);

    _lazyClient = client;
    return client;
}

void Client::updateJackIOs()
{
    jack_client_t *client = _lazyClient;
    dsp *dsp = _dspWrapper ? _dspWrapper->getDsp() : nullptr;

    size_t oldInputCount = _inputs.size();
    size_t oldOutputCount = _outputs.size();

    size_t newInputCount = dsp ? dsp->getNumInputs() : 0;
    size_t newOutputCount = dsp ? dsp->getNumOutputs() : 0;

    for (size_t i = oldInputCount; i < newInputCount; ++i) {
        std::string name = "in_" + std::to_string(i + 1);
        jack_port_t *port = jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (!port)
            panic("Could not register JACK input");
        _inputs.push_back(port);
    }
    for (size_t i = oldInputCount; i > newInputCount; --i) {
        jack_port_t *port = _inputs.back();
        if (jack_port_unregister(client, port) != 0)
            panic("Could not unregister JACK input");
        _inputs.pop_back();
    }

    for (size_t i = oldOutputCount; i < newOutputCount; ++i) {
        std::string name = "out_" + std::to_string(i + 1);
        jack_port_t *port = jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!port)
            panic("Could not register JACK output");
        _outputs.push_back(port);
    }
    for (size_t i = oldOutputCount; i > newOutputCount; --i) {
        jack_port_t *port = _outputs.back();
        if (jack_port_unregister(client, port) != 0)
            panic("Could not unregister JACK output");
        _outputs.pop_back();
    }

    _portBufs.resize(newInputCount + newOutputCount);
}

std::vector<std::string> Client::saveJackConnections(jack_port_t *port)
{
    std::vector<std::string> connections;

    const char **con = jack_port_get_connections(port);
    if (!con)
        return connections;

    size_t count;
    for (count = 0; con[count]; ++count);

    connections.reserve(count);
    for (size_t i = 0; i < count; ++i)
        connections.emplace_back(con[i]);

    jack_free(con);
    return connections;
}

void Client::restoreJackConnections(jack_port_t *port, const std::vector<std::string> &connections)
{
    jack_client_t *client = getJackClient();
    int flags = jack_port_flags(port);

    const char *src = nullptr;
    const char *dst = nullptr;

    if (flags & JackPortIsOutput)
        src = jack_port_name(port);
    else
        dst = jack_port_name(port);

    size_t count = connections.size();
    for (size_t i = 0; i < count; ++i) {
        if (flags & JackPortIsOutput)
            dst = connections[i].c_str();
        else
            src = connections[i].c_str();
        jack_connect(client, src, dst);
    }
}

int Client::process(jack_nframes_t nframes, void *arg)
{
    Client *self = (Client *)arg;

    size_t numInputs = self->_inputs.size();
    size_t numOutputs = self->_outputs.size();

    float **inputs = self->_portBufs.data();
    float **outputs = inputs + numInputs;

    for (size_t i = 0; i < numInputs; ++i) {
        inputs[i] = (float *)jack_port_get_buffer(self->_inputs[i], nframes);
    }
    for (size_t i = 0; i < numOutputs; ++i) {
        outputs[i] = (float *)jack_port_get_buffer(self->_outputs[i], nframes);
    }

    dsp *dsp = self->_dspWrapper ? self->_dspWrapper->getDsp() : nullptr;

    if (dsp) {
        dsp->compute((int)nframes, inputs, outputs);
    }
    else {
        for (size_t i = 0; i < numOutputs; ++i)
            std::memset(outputs[i], 0, nframes * sizeof(float));
    }

    return 0;
}

} // namespace jest
