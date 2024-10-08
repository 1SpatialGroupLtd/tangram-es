//
//  TGCameraPosition.h
//  tangram
//
//  Created by Matt Blair on 7/18/18.
//

#import "TGExport.h"
#import "TGGeometry.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreLocation/CoreLocation.h>
/**
 A camera position defining a viewpoint for a map.
 */
TG_EXPORT
@interface TGCameraPosition : NSObject <NSCopying>

/**
 Initialize a camera position with a location and orientation.

 @param center The location at the center of the map view
 @param zoom The zoom level of the map view, lower values show more area
 @param rotation The orientation of the map view in degrees clockwise from North
 @param tilt The tilt of the map view in degrees away from straight down
 */
- (instancetype)initWithCenter:(CLLocationCoordinate2D)center
                          zoom:(CGFloat)zoom
                      rotation:(CGFloat)rotation
                          tilt:(CGFloat)tilt;

/**
 The coordinate at the center of the map view.
 */
@property (assign, nonatomic) CLLocationCoordinate2D center;

/**
 The zoom level of the map view. Lower values show more area.
 */
@property (assign, nonatomic) CGFloat zoom;

/**
 The orientation of the map view in degrees clockwise from North.
 */
@property (assign, nonatomic) CGFloat rotation;

/**
 The tilt of the map view in degrees away from straight down.
 */
@property (assign, nonatomic) CGFloat tilt;

@end // interface TGCameraPosition
