#include "jest_app.h"
#include "utility/logs.h"
#include <unistd.h>
#include <thread>
#include <cstdio>
#include <cerrno>
#include <csignal>

struct Pipe {
    ~Pipe() { for (int x : fd) if (x != -1) close(x); }
    int fd[2] {-1, -1};
};

///
static constexpr int term_signals[] = {SIGTERM, SIGINT};
static Pipe term_pipe;

static void signal_handler_entry()
{
    for (;;)
        pause();
}

static void signal_handler_term(int)
{
    char byte = 1;
    ssize_t count;
    while ((count = write(term_pipe.fd[1], &byte, 1)),
           (count == 0 || (count == -1 && errno == EINTR)));
}

///
int main(int argc, char *argv[])
{
    if (pipe(term_pipe.fd) != 0) {
        return 1;
    }

    for (int sig : term_signals) {
        struct sigaction sa = {};
        sa.sa_handler = &signal_handler_term;
        sigemptyset(&sa.sa_mask);
        sigaction(sig, &sa, nullptr);
    }

    std::thread(signal_handler_entry).detach();
    sigset_t sigset;
    sigemptyset(&sigset);
    for (int sig : term_signals) {
        sigaddset(&sigset, sig);
    }
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    ///
    jest::App app(argc, argv);
    app.init(term_pipe.fd[0]);
    int ret = app.exec();
    app.shutdown();

    ///
    return ret;
}
