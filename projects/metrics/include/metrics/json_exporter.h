#pragma once
#include "exporter.h"

namespace metrics {

class JSONExporter : public Exporter{
public:
    // Inherit constructor
    using Exporter::Exporter;

private:
    void processMessage(const ChangeMessage& message) override final;
};

}
