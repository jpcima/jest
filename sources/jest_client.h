#pragma once
#include <jack/jack.h>
#include <vector>
#include <memory>
class DSPWrapper;
using DSPWrapperPtr = std::shared_ptr<DSPWrapper>;

namespace jest {

class Client {
public:
    Client();
    ~Client();

    void setDsp(DSPWrapperPtr dspWrapper);

private:
    jack_client_t *getJackClient();
    void updateJackIOs();

    std::vector<std::string> saveJackConnections(jack_port_t *port);
    void restoreJackConnections(jack_port_t *port, const std::vector<std::string> &connections);

    static int process(jack_nframes_t nframes, void *arg);

private:
    DSPWrapperPtr _dspWrapper;
    jack_client_t *_lazyClient = nullptr;
    std::vector<jack_port_t *> _inputs;
    std::vector<jack_port_t *> _outputs;
    std::vector<float *> _portBufs;
};

} // namespace jest
