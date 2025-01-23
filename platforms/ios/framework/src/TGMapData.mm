//
//  TGMapData.mm
//  TangramMap
//
//  Created by Karim Naaji on 2/24/16.
//  Updated by Matt Blair on 8/21/18.
//  Copyright (c) 2017 Mapzen. All rights reserved.
//

#import "TGGeoPolygon.h"
#import "TGGeoPolyline.h"
#import "TGMapData.h"
#import "TGMapData+Internal.h"
#import "TGMapView.h"
#import "TGMapView+Internal.h"
#import "TGTypes+Internal.h"

#include "tangram.h"
#include <memory>

static inline void TGFeaturePropertiesConvertToCoreProperties(TGFeatureProperties* properties, Tangram::Properties& tgProperties)
{
    for (NSString* key in properties) {
        NSString* value = [properties objectForKey:key];
        tgProperties.set(std::string([key UTF8String]), [value UTF8String]);
    }
}

@interface TGMapData () {
    std::shared_ptr<Tangram::ClientDataSource> dataSource;
}

@property (copy, nonatomic) NSString* name;
@property (weak, nonatomic) TGMapView* map;

@end

@implementation TGMapData

- (instancetype)initWithMapView:(__weak TGMapView *)mapView name:(NSString *)name source:(std::shared_ptr<Tangram::ClientDataSource>)source
{
    self = [super init];

    if (self) {
        self.name = name;
        self.map = mapView;
        dataSource = source;
    }

    return self;
}

- (void)setFeatures:(NSArray<TGMapFeature *> *)features
{
    if (!self.map) {
        return;
    }
    
    [self.map clearTileCache: dataSource->id()];
    
    for (TGMapFeature *feature in features) {
        Tangram::Properties properties;
        TGFeaturePropertiesConvertToCoreProperties(feature.properties, properties);

        if (CLLocationCoordinate2D *point = [feature point]) {

            dataSource->addPointFeature(std::move(properties), TGConvertCLLocationCoordinate2DToCoreLngLat(*point));

        } else if (TGGeoPolyline *polyline = [feature polyline]) {

            Tangram::ClientDataSource::PolylineBuilder builder;
            size_t numberOfPoints = polyline.count;
            builder.beginPolyline(numberOfPoints);
            for (size_t i = 0; i < numberOfPoints; i++) {
                builder.addPoint(TGConvertCLLocationCoordinate2DToCoreLngLat(polyline.coordinates[i]));
            }
            dataSource->addPolylineFeature(std::move(properties), std::move(builder));

        } else if (TGGeoPolygon *polygon = [feature polygon]) {

            Tangram::ClientDataSource::PolygonBuilder builder;
            builder.beginPolygon(polygon.rings.count);
            for (TGGeoPolyline *ring in polygon.rings) {
                size_t numberOfPoints = ring.count;
                builder.beginRing(numberOfPoints);
                for (size_t i = 0; i < numberOfPoints; i++) {
                    builder.addPoint(TGConvertCLLocationCoordinate2DToCoreLngLat(ring.coordinates[i]));
                }
            }
            dataSource->addPolygonFeature(std::move(properties), std::move(builder));

        }
    }
    dataSource->generateTiles();
}

- (void)generateTilesIfNeeded: (size_t)length generateTiles: (BOOL)generateTiles
{
    if(length > 0 && generateTiles)
    {
       [self.mapView clearTileCache:dataSource->id()];
       dataSource->generateTiles();
       [self.mapView requestRender];
    }
}

- (size_t)removeGeoJsonById:(const size_t*)ids length:(size_t)length  generateTiles:(BOOL)generateTiles
{
    size_t number = dataSource->removeFeatures(ids, length);
    [self generateTilesIfNeeded:number generateTiles:generateTiles];
    
    return number;
}

- (void)setGeoJson:(NSString *)data
{
    if (!self.map) {
        return;
    }

    std::string sourceData = std::string([data UTF8String]);

    dataSource->clearFeatures();
    [self.map clearTileCache: dataSource->id()];
    
    dataSource->addData(sourceData);
    dataSource->generateTiles();
}

- (void)clear
{
    if (!self.map) return;

    dataSource->clearFeatures();
    [self.map clearTileCache: dataSource->id()];

    dataSource->generateTiles();
}

- (BOOL)remove
{
    if (!self.map) {
        return NO;
    }

    BOOL removed = [self.map removeDataSource:dataSource name:self.name];
    self.map = nil;
    return removed;
}

- (BOOL)visible
{
    if (!self.map) return;
    return dataSource->isVisible();
}

- (void)setVisible:(BOOL)visible
{
    if (!self.map) return;
    dataSource->setVisible(visible);
}

- (size_t) setGeoJsonFromBytes: (const char*) geoJsonBytes length: (size_t)length
{
    if (!self.map) return;
    dataSource->clearFeatures();
    [self.map clearTileCache: dataSource->id()];
    auto count = dataSource->addData(geoJsonBytes, length);
    dataSource->generateTiles();
}

- (ssize_t) appendOrUpdateGeoJson:(nonnull const char*) geoJsonBytes length: (size_t)length generateTiles:(bool) generateTiles
{
    if (!self.map) return;
    
    auto count = dataSource->appendOrUpdateFeatures(geoJsonBytes, length);
    [self generateTilesIfNeeded:count generateTiles:generateTiles];
    
    return count;
}

- (void) enableGeoJsonUpdate:(bool) enable
{
    if (!self.map) return;
    dataSource->setCanUpdateFeatures(enable);
}
- (void) generateTiles{
    if (!self.map) return;
    dataSource->generateTiles();
}

@end
