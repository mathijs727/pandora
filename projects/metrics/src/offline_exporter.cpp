#include "metrics/offline_exporter.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace metrics {

OfflineExporter::OfflineExporter(Group& group, std::string_view fileName)
    : Exporter(group)
    , m_fileName(fileName)
{
}

OfflineExporter::~OfflineExporter()
{
    std::ofstream outFile;
    outFile.open(m_fileName);
    outFile << m_messageListJSON.dump(4) << std::endl;
    outFile.close();
}

void OfflineExporter::processMessage(const Message& message)
{
    /*std::ostringstream joinedIdentifier;
    std::copy(message.identifier.begin(), message.identifier.end(),
        std::ostream_iterator<std::string>(joinedIdentifier, "-"));
    std::cout << joinedIdentifier.str() << "; value changed to: " << content.newValue << "; at time stamp: " << timeMicroSeconds << std::endl;*/

    int timeStampMicroSeconds = message.timeStamp.count();
    nlohmann::json jsonMessage = {
        { "identifier", message.identifier },
        { "timestamp", timeStampMicroSeconds },
    };

    if (std::holds_alternative<ValueChangedMessage>(message.content)) {
        auto content = std::get<ValueChangedMessage>(message.content);
        jsonMessage["content"] = {
            { "type", "value_changed" },
            { "new_value", content.newValue }
        };
    }

    m_messageListJSON.push_back(jsonMessage);
}

}
