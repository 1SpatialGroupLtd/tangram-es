#include "scene/dataLayer.h"

namespace Tangram {

DataLayer::DataLayer(SceneLayer layer, std::string source, std::vector<std::string> collections) :
    SceneLayer(std::move(layer)),
    m_source(std::move(source)),
    m_collections(std::move(collections)) {}

void DataLayer::merge(const std::string& name, SceneLayer& other)
{
    const DataLayer& otherDataLayer = static_cast<const DataLayer&>(other);

    if(!otherDataLayer.m_source.empty())
        m_source = otherDataLayer.m_source;

    mergeSceneLayer(name, other);
}
}
