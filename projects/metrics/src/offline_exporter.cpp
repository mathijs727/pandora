#include "metrics/offline_exporter.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace metrics {

OfflineExporter::OfflineExporter(std::string_view fileName)
    : m_fileName(fileName)
{
}

OfflineExporter::~OfflineExporter()
{
    std::ofstream outFile;
    outFile.open(m_fileName);
    outFile << m_snapshots.dump(4) << std::endl;
    outFile.close();
}

void OfflineExporter::notifyNewSnapshot(const nlohmann::json& snapshot)
{
    m_snapshots.push_back(snapshot);
}

}
