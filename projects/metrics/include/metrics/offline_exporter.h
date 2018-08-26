#pragma once
#include "exporter.h"
#include <string_view>
#include <nlohmann/json.hpp>

namespace metrics {

class OfflineExporter : public Exporter{
public:
    OfflineExporter(Group& group, std::string_view fileName);
    ~OfflineExporter();

private:
    void processMessage(const Message& message) override final;
private:
    std::string m_fileName;
    nlohmann::json m_messageListJSON;
};

}
