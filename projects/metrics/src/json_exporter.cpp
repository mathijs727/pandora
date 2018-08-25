#include "metrics/json_exporter.h"
#include <iostream>
#include <sstream>

namespace metrics
{

void JSONExporter::processMessage(const ChangeMessage& message) {
    std::ostringstream joinedIdentifier;
    std::copy(message.identifier.begin(), message.identifier.end(),
        std::ostream_iterator<std::string>(joinedIdentifier, "-"));

    std::cout << joinedIdentifier.str() << "; value changed to: " << message.newValue << std::endl;
}

}