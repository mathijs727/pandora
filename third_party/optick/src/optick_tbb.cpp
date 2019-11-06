#include "optick/optick_tbb.h"
#include "optick/optick.h"
#include <atomic>
#include <fmt/format.h>
#include <string>

static thread_local bool registeredWithOptick { false };

void tryRegisterThreadWithOptick()
{
    static std::atomic_int workerIDs { 0 };
    if (!registeredWithOptick) {
        const int workerID = workerIDs.fetch_add(1);
        std::string workerName = fmt::format("Worker {}", workerID);
        OPTICK_THREAD(workerName.c_str());
        registeredWithOptick = true;
    }
}

void setThisMainThreadOptick()
{
    registeredWithOptick = true;
}