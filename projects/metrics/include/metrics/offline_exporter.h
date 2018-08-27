#pragma once
#include "exporter.h"
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace metrics {

class OfflineExporter : public Exporter{
public:
    OfflineExporter(std::string_view fileName);
    ~OfflineExporter();

protected:
    void notifyNewSnapshot(const nlohmann::json& snapshot) override final;

private:
    std::string m_fileName;
    nlohmann::json m_snapshots;
};

}
