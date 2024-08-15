#include "data/clientDataSource.h"

#include "log.h"
#include "platform.h"
#include "tile/tileTask.h"
#include "util/geom.h"
#include "data/propertyItem.h"
#include "data/tileData.h"
#include "tile/tile.h"
#include "view/view.h"

#include "mapbox/geojsonvt.hpp"

// RapidJson parser
#include "mapbox/geojson.hpp"
#include <mapbox/geojson_impl.hpp>


#include <regex>

namespace Tangram {

using namespace mapbox;

geojsonvt::Options options() {
    geojsonvt::Options opt;
    opt.maxZoom = 18;
    opt.indexMaxZoom = 5;
    opt.indexMaxPoints = 100000;
    opt.solidChildren = true;
    opt.tolerance = 3;
    opt.extent = 4096;
    opt.buffer = 0;
    return opt;
}

struct ClientDataSource::Storage {
    std::unique_ptr<geojsonvt::GeoJSONVT> tiles;
    geometry::feature_collection<double> features;
    std::vector<Properties> properties;
};

struct ClientDataSource::PolylineBuilderData : mapbox::geometry::line_string<double> {
    virtual ~PolylineBuilderData() = default;
};

ClientDataSource::PolylineBuilder::PolylineBuilder() {
    data = std::make_unique<PolylineBuilderData>();
}

ClientDataSource::PolylineBuilder::~PolylineBuilder() = default;

void ClientDataSource::PolylineBuilder::beginPolyline(size_t numberOfPoints) {
    data->reserve(numberOfPoints);
}

void ClientDataSource::PolylineBuilder::addPoint(Tangram::LngLat point) {
    data->emplace_back(point.longitude, point.latitude);
}

struct ClientDataSource::PolygonBuilderData : mapbox::geometry::polygon<double> {
    virtual ~PolygonBuilderData() = default;
};

ClientDataSource::PolygonBuilder::PolygonBuilder() {
    data = std::make_unique<PolygonBuilderData>();
}

ClientDataSource::PolygonBuilder::~PolygonBuilder() = default;

void ClientDataSource::PolygonBuilder::beginPolygon(size_t numberOfRings) {
    data->reserve(numberOfRings);
}

void ClientDataSource::PolygonBuilder::beginRing(size_t numberOfPoints) {
    data->emplace_back();
    data->back().reserve(numberOfPoints);
}

void ClientDataSource::PolygonBuilder::addPoint(LngLat point) {
    data->back().emplace_back(point.longitude, point.latitude);
}

std::shared_ptr<TileTask> ClientDataSource::createTask(TileID _tileId) {
    return std::make_shared<TileTask>(_tileId, shared_from_this());
}

// TODO: pass scene's resourcePath to constructor to be used with `stringFromFile`
ClientDataSource::ClientDataSource(Platform& _platform, const std::string& _name,
                                   const std::string& _url, bool _generateCentroids,
                                   TileSource::ZoomOptions _zoomOptions)

    : TileSource(_name, nullptr, _zoomOptions),
      m_generateCentroids(_generateCentroids),
      m_platform(_platform) {

    m_generateGeometry = true;
    m_store = std::make_unique<Storage>();

    if (!_url.empty()) {
        UrlCallback onUrlFinished = [&, this](UrlResponse&& response) {
            if (response.error) {
                LOGE("Unable to retrieve data from '%s': %s", _url.c_str(), response.error);
            } else {
                addData(std::string(response.content.begin(), response.content.end()));
                generateTiles();
            }
            m_hasPendingData = false;
        };
        m_platform.startUrlRequest(Url(_url), std::move(onUrlFinished));
        m_hasPendingData = true;
    }

}

ClientDataSource::~ClientDataSource() = default;

struct add_centroid {

    geometry::point<double>& pt;

    bool operator()(const geometry::polygon<double>& geom) {
        if (geom.empty()) { return false; }
        pt = centroid(geom.front().begin(), geom.front().end()-1);
        return !(std::isnan(pt.x) || std::isnan(pt.y));
    }

    bool operator()(const geometry::multi_polygon<double>& geom) {
        float largestArea = 0.f;
        size_t largestAreaIndex = 0;
        for (size_t index = 0; index < geom.size(); index++) {
            auto& g = geom[index];
            if (g.empty()){
                continue;
            }
            auto area = signedArea(g.front().begin(), g.front().end());
            if (area > largestArea) {
                largestArea = area;
                largestAreaIndex = index;
            }
        }
        return (*this)(geom[largestAreaIndex]);
    }

    template <typename T>
    bool operator()(T) {
        // Ignore all other geometry types
        return false;
    }
};

struct prop_visitor {
    Properties& props;
    std::string& key;
    void operator()(std::string v) {
        props.set(key, std::move(v));
    }
    void operator()(bool v) {
        props.set(key, double(v));
    }
    void operator()(uint64_t v) {
        props.set(key, double(v));
    }
    void operator()(int64_t v) {
        props.set(key, double(v));
    }
    void operator()(double v) {
        props.set(key, v);
    }

    template <typename T>
    void operator()(T) {
        // Ignore all other basic value types
    }
};

void ClientDataSource::generateTiles() {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    if (m_generateCentroids) {
        for (const auto &feat : m_store->features) {
            geometry::point<double> centroid;
            const auto& properties = m_store->properties[feat.id.get<uint64_t>()];
            if (geometry::geometry<double>::visit(feat.geometry, add_centroid{ centroid })) {
                uint64_t id = m_store->features.size();
                m_store->features.emplace_back(centroid, id);
                m_store->properties.push_back(properties);
                auto& props = m_store->properties.back();
                props.set("label_placement", 1.0);
            }
        }
    }

    m_store->tiles = std::make_unique<geojsonvt::GeoJSONVT>(m_store->features, options());
    m_generation++;
}

void ClientDataSource::loadTileData(std::shared_ptr<TileTask> _task, TileTaskCb _cb) {

    if (m_hasPendingData) {
        return;
    }

    if (_task->needsLoading()) {
        _task->startedLoading();

        _cb.func(_task);
    }

    // Load subsources
    TileSource::loadTileData(_task, _cb);
}

void ClientDataSource::clearFeatures() {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    m_store->features.clear();
    m_store->properties.clear();
}

size_t ClientDataSource::addData(const std::string& _data)
{
    return addData(_data.c_str(), _data.size());
}

size_t ClientDataSource::addData(const char* _data, size_t length) {
    size_t added{};
    std::lock_guard<std::mutex> lock(m_mutexStore);

    const auto json = geojson::parse(_data, length);
    auto features = geojsonvt::geojson::visit(json, geojsonvt::ToFeatureCollection{});

    if(m_canUpdateFeatures)
    {
        // reserving some extra features to avoid later huge allocations when we append features
        m_store->features.reserve(features.size() + 500);
        m_store->properties.reserve(features.size() + 500);
    }

    for (auto& feature : features) {
        if(m_canUpdateFeatures)
        {
            if(!feature.id.is<uint64_t>() || feature.id.get<uint64_t>() == 0)
                continue;
        }
        else {
            feature.id = uint64_t(m_store->properties.size());
        }

        feature.indexInArray = m_store->properties.size();

        m_store->properties.emplace_back();

        Properties& props = m_store->properties.back();

        for (const auto& prop : feature.properties) {
            auto key = prop.first;
            prop_visitor visitor = {props, key};
            mapbox::util::apply_visitor(visitor, prop.second);
        }

        feature.properties.clear();

        ++added;
    }

    m_store->features.insert(m_store->features.end(),
                             std::make_move_iterator(features.begin()),
                             std::make_move_iterator(features.end()));
    return added;
}

void ClientDataSource::addPointFeature(Properties&& properties, LngLat coordinates) {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    geometry::point<double> geom {coordinates.longitude, coordinates.latitude};

    uint64_t id = m_store->features.size();
    m_store->features.emplace_back(geom, id);
    m_store->properties.emplace_back(properties);
}

void ClientDataSource::addPolylineFeature(Properties&& properties, PolylineBuilder&& polyline) {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    uint64_t id = m_store->features.size();
    auto geom = std::move(polyline.data);
    m_store->features.emplace_back(*geom, id);
    m_store->properties.emplace_back(properties);
}

void ClientDataSource::addPolygonFeature(Properties&& properties, PolygonBuilder&& polygon) {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    uint64_t id = m_store->features.size();
    auto geom = std::move(polygon.data);
    m_store->features.emplace_back(*geom, id);
    m_store->properties.emplace_back(properties);
}

struct add_geometry {

    static constexpr double extent = 4096.0;

    // Transform a geojsonvt::TilePoint into the corresponding Tangram::Point
    Point transformPoint(geometry::point<int16_t> pt) {
        return { pt.x / extent, 1. - pt.y / extent };
    }

    Feature& feature;

    bool operator()(const geometry::point<int16_t>& p) {
        feature.geometryType = GeometryType::points;
        feature.points.push_back(transformPoint(p));
        return true;
    }
    bool operator()(const geometry::line_string<int16_t>& geom) {
        feature.geometryType = GeometryType::lines;
        feature.lines.emplace_back();
        Line& line = feature.lines.back();
        for (const auto& p : geom) {
            auto tp = transformPoint(p);
            if (line.size() > 0 && tp == line.back()) { continue; }
            line.push_back(tp);
        }
        return true;
    }
    bool operator()(const geometry::polygon<int16_t>& geom) {
        feature.geometryType = GeometryType::polygons;
        feature.polygons.emplace_back();
        for (const auto& ring : geom) {
            feature.polygons.back().emplace_back();
            Line& line = feature.polygons.back().back();
            for (const auto& p : ring) {
                auto tp = transformPoint(p);
                if (line.size() > 0 && tp == line.back()) { continue; }
                line.push_back(tp);
            }
        }
        return true;
    }

    bool operator()(const geometry::multi_point<int16_t>& geom) {
        for (auto& g : geom) { (*this)(g); }
        return true;
    }

    bool operator()(const geometry::multi_line_string<int16_t>& geom) {
        for (auto& g : geom) { (*this)(g); }
        return true;
    }

    bool operator()(const geometry::multi_polygon<int16_t>& geom) {
        for (auto& g : geom) { (*this)(g); }
        return true;
    }

    template <typename T>
    bool operator()(T) {
        // Ignore GeometryCollection
        return false;
    }
};

std::shared_ptr<TileData> ClientDataSource::parse(const TileTask& _task) const {

    std::lock_guard<std::mutex> lock(m_mutexStore);

    auto data = std::make_shared<TileData>();

    if (!m_store->tiles) { return nullptr; }
    auto tile = m_store->tiles->getTile(_task.tileId().z, _task.tileId().x, _task.tileId().y);

    data->layers.emplace_back("");  // empty name will skip filtering by 'collection'
    Layer& layer = data->layers.back();

    for (auto& it : tile.features) {
        Feature feature(m_id);

        if (geometry::geometry<int16_t>::visit(it.geometry, add_geometry{ feature })) {
            if(it.indexInArray >= m_store->features.size()) {
                LOGE("indexOfArray is out of bound!");
            }
            else
            {
                feature.props = m_store->properties[it.indexInArray];
                layer.features.emplace_back(std::move(feature));
            }
        }
    }

    return data;
}

}

static std::vector<uint64_t> filterIds(mapbox::geojson::feature_collection& features)
{
    std::vector<uint64_t> ids;

    for(int i = 0; i < features.size(); i++) {
        if (!features[i].id.is<uint64_t>() || features[i].id.get<uint64_t>() == 0) {
            features.erase(features.begin() + i);
            i--;
            continue;
        }

        ids.push_back(features[i].id.get<uint64_t>());
    }

    return ids;
}

uint64_t Tangram::ClientDataSource::removeFeatures(const uint64_t *idsArray, uint64_t length) {
    std::lock_guard<std::mutex> lock(m_mutexStore);

    if (!idsArray || !m_canUpdateFeatures || m_store->features.empty() || length == 0)
        return 0;

    uint64_t updated = 0;
    std::vector<uint64_t> ids{idsArray, idsArray + length};
    size_t firstRemovedIndex = 0;
    for (int store_index = 0; store_index < m_store->features.size(); store_index++) {
        for (auto update_index = 0; update_index < ids.size(); update_index++) {
            if (m_store->features[store_index].id.get<uint64_t>() == ids[update_index]) {
                // remember the first removed
                if(updated == 0) firstRemovedIndex = store_index;

                m_store->properties.erase(m_store->properties.begin() + store_index);
                m_store->features.erase(m_store->features.begin() + store_index);

                ids.erase(ids.begin() + update_index);
                ++updated;
                --store_index;
                break;
            }
        }
    }

    if(updated) {
        if(m_store->features.capacity() - m_store->features.size() > 1000)
        {
            m_store->features.reserve(m_store->features.size() + 500);
            m_store->properties.reserve(m_store->features.size() + 500);
        }

        // update the indexInArray after removing the elements
        for (size_t i = firstRemovedIndex; i < m_store->features.size(); ++i)
            m_store->features[i].indexInArray = i;
    }

    return updated;
}

size_t Tangram::ClientDataSource::appendOrUpdateFeatures(const std::string &_data) {
    return appendOrUpdateFeatures(_data.c_str(), _data.size());
}

size_t Tangram::ClientDataSource::appendOrUpdateFeatures(const char* _data, size_t length) {
    std::lock_guard<std::mutex> lock(m_mutexStore);

    if (!m_canUpdateFeatures)
        return 0;

    const auto json = geojson::parse(_data, length);
    auto features = geojsonvt::geojson::visit(json, geojsonvt::ToFeatureCollection{});

    uint64_t updated = 0;

    auto ids = filterIds(features);
    if(ids.empty()) return 0;

    // first try to update features
    for (int store_index = 0; store_index < m_store->features.size(); store_index++) {
        for (int update_index = 0; update_index < ids.size(); update_index++) {
            auto store_feature = m_store->features[store_index];

            if (store_feature.id.get<uint64_t>() == ids[update_index]) {
                Properties &props = m_store->properties[store_index];
                props.clear();

                auto &feature = features[update_index];
                for (const auto &prop: feature.properties) {
                    auto key = prop.first;
                    Tangram::prop_visitor visitor = {props, key};
                    mapbox::util::apply_visitor(visitor, prop.second);
                }
                feature.properties.clear();
                m_store->features[store_index] = std::move(features[update_index]);
                m_store->features[store_index].indexInArray = store_index;

                ++updated;

                features.erase(features.begin() + update_index);
                ids.erase(ids.begin() + update_index);

                if (ids.empty())
                    return updated;

                update_index--;
            }
        }
    }

    if(m_store->features.capacity() < m_store->features.size() + ids.size())
    {
        auto new_cap = m_store->features.size() + ids.size() + 500;
        m_store->features.reserve(new_cap);
        m_store->properties.reserve(new_cap);
    }

    // insert the remaining ones
    for (auto id: ids) {
        auto it = std::find_if(features.begin(), features.end(),
                               [id](const mapbox::geojson::feature &x) {
                                   return x.id.get<uint64_t>() == id;
                               });

        if (it == features.end()) continue;
        m_store->properties.emplace_back();
        Properties &props = m_store->properties.back();

        for (const auto &prop: it->properties) {
            auto key = prop.first;
            prop_visitor visitor = {props, key};
            mapbox::util::apply_visitor(visitor, prop.second);
        }

        it->properties.clear();
        it->indexInArray = m_store->features.size();
        m_store->features.push_back(std::move(*it));
        ++updated;
    }

    return updated;
}