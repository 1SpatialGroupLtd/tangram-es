//
//  TGMapView.mm
//  TangramMap
//
//  Created by Matt Blair on 7/10/18.
//

#import "TGMapView.h"
#import "TGMapView+Internal.h"
#import "TGCameraPosition.h"
#import "TGCameraPosition+Internal.h"
#import "TGURLHandler.h"
#import "TGLabelPickResult.h"
#import "TGLabelPickResult+Internal.h"
#import "TGMapData.h"
#import "TGMapData+Internal.h"
#import "TGMapViewDelegate.h"
#import "TGMarkerPickResult.h"
#import "TGMarkerPickResult+Internal.h"
#import "TGMarker.h"
#import "TGMarker+Internal.h"
#import "TGRecognizerDelegate.h"
#import "TGSceneUpdate.h"
#import "TGTypes+Internal.h"
#import <GLKit/GLKit.h>
#include "data/clientDataSource.h"
#include "data/propertyItem.h"
#include "iosPlatform.h"
#include "map.h"
#include <unordered_map>
#include <functional>

/**
 Map region change states
 Used to determine map region change transitions when animating or responding to input gestures.
 IDLE: Map is still, no state transitions happening
 Jumping: Map region changing without animating
 Animating: Map region changing with animation
 */
typedef NS_ENUM(NSInteger, TGMapRegionChangeState) {
    TGMapRegionIdle = 0,
    TGMapRegionJumping,
    TGMapRegionAnimating,
};

@interface TGMapView () <UIGestureRecognizerDelegate, GLKViewDelegate> {
    BOOL _shouldCaptureFrame;
    BOOL _captureFrameWaitForViewComplete;
    BOOL _prevMapViewComplete;
    BOOL _viewInBackground;
    BOOL _renderRequested;
}

@property (nullable, strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKView *glView;
@property (strong, nonatomic) CADisplayLink *displayLink;
@property (strong, nonatomic) NSMutableDictionary<NSString *, TGMarker *> *markersById;
@property (strong, nonatomic) NSMutableDictionary<NSString *, TGMapData *> *dataLayersByName;
@property (nonatomic, copy, nullable) void (^cameraAnimationCallback)(BOOL);
@property TGMapRegionChangeState currentState;
@property BOOL prevCameraEasing;

@end // interface TGMapView

@implementation TGMapView

@synthesize displayLink = _displayLink;

#pragma mark Lifecycle Methods

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithFrame:(CGRect)frame setupGestures:(bool)setupGestures
{
    self = [super initWithFrame:frame];
    
    if (self) {
        [self setupWithGestures:setupGestures];
    }
}
- (instancetype)initWithFrame:(CGRect)frame urlHandler:(id<TGURLHandler>)urlHandler
{
    self = [super initWithFrame:frame];
    if (self) {
        self.urlHandler = urlHandler;
        [self setup];
    }
    return self;
}

- (void)dealloc
{
    [self validateContext];
    if (_map) {
        delete _map;
    }

    if (_displayLink) {
        [_displayLink invalidate];
    }
    [self invalidateContext];
}

- (void)setup {
    [self setupWithGestures:true];
}

- (void)setupWithGestures:(bool)setupGestures
{
    __weak TGMapView* weakSelf = self;
    _map = new Tangram::Map(std::make_unique<Tangram::iOSPlatform>(weakSelf));

    NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(didEnterBackground:)
                               name:UIApplicationDidEnterBackgroundNotification
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(didLeaveBackground:)
                               name:UIApplicationWillEnterForegroundNotification
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(didLeaveBackground:)
                               name:UIApplicationDidBecomeActiveNotification
                             object:nil];

    _prevMapViewComplete = NO;
    _captureFrameWaitForViewComplete = YES;
    _shouldCaptureFrame = NO;
    _viewInBackground = [UIApplication sharedApplication].applicationState == UIApplicationStateBackground;
    _renderRequested = YES;
    _continuous = NO;
    _preferredFramesPerSecond = 60;
    _markersById = [[NSMutableDictionary alloc] init];
    _dataLayersByName = [[NSMutableDictionary alloc] init];
    _resourceRoot = [[NSBundle mainBundle] resourceURL];
    _currentState = TGMapRegionIdle;
    _prevCameraEasing = false;

    self.clipsToBounds = YES;
    self.opaque = YES;
    self.autoresizesSubviews = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    if (!_urlHandler) {
        _urlHandler = [[TGDefaultURLHandler alloc] init];
    }

    if(!_viewInBackground) {
        [self setupGL];
    }
   if(setupGestures)
       [self setupGestureRecognizers];

    self.map->setCameraAnimationListener([weakSelf](bool finished){
        void (^callback)(BOOL) = weakSelf.cameraAnimationCallback;
        if (callback) {
            callback(!finished);
            [weakSelf setMapRegionChangeState:TGMapRegionIdle];
        }
        weakSelf.cameraAnimationCallback = nil;
    });
}

- (void)didEnterBackground:(__unused NSNotification *)notification
{
    _viewInBackground = YES;
    _displayLink.paused = YES;
}

- (void)didLeaveBackground:(__unused NSNotification *)notification
{
    _viewInBackground = NO;
    if (!_context) {
        [self setupGL];
    }
    _displayLink.paused = NO;
}

- (void)validateContext
{
    if (![[EAGLContext currentContext] isEqual:_context]) {
        [EAGLContext setCurrentContext:_context];
    }
}

- (void)invalidateContext
{
    if ([[EAGLContext currentContext] isEqual:_context]) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)setupGL
{
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (!_context) {
        NSLog(@"Failed to create GLES context");
        return;
    }

    _glView = [[GLKView alloc] initWithFrame:self.bounds context:_context];

    _glView.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    _glView.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    _glView.drawableStencilFormat = GLKViewDrawableStencilFormat8;
    _glView.drawableMultisample = GLKViewDrawableMultisampleNone;
    _glView.opaque = self.opaque;
    _glView.delegate = self;
    _glView.autoresizingMask = self.autoresizingMask;

    [self validateContext];

    [_glView bindDrawable];

    [self insertSubview:_glView atIndex:0];

    // By default,  a GLKView's contentScaleFactor property matches the scale of
    // the screen that contains it, so the framebuffer is already configured for
    // rendering at the full resolution of the display.
    self.contentScaleFactor = _glView.contentScaleFactor;

    _map->setupGL();
    _map->setPixelScale(_glView.contentScaleFactor);

    [self trySetMapDefaultBackground:self.backgroundColor];
}

- (void)setupDisplayLink
{
    BOOL visible = self.superview && self.window;
    if (visible && !_displayLink) {
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkUpdate:)];
        _displayLink.preferredFramesPerSecond = self.preferredFramesPerSecond;
        [_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
        _renderRequested = YES;
    } else if (!visible && _displayLink) {
        [_displayLink invalidate];
        _displayLink = nil;
    }
}

- (void)setupGestureRecognizers
{
    _tapGestureRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(respondToTapGesture:)];
    _tapGestureRecognizer.numberOfTapsRequired = 1;
    // TODO: Figure a way to have a delay set for it not to tap gesture not to wait long enough for a doubletap gesture to be recognized
    _tapGestureRecognizer.delaysTouchesEnded = NO;

    _doubleTapGestureRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(respondToDoubleTapGesture:)];
    _doubleTapGestureRecognizer.numberOfTapsRequired = 2;
    // Ignore single tap when double tap occurs
    [_tapGestureRecognizer requireGestureRecognizerToFail:_doubleTapGestureRecognizer];

    _panGestureRecognizer = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(respondToPanGesture:)];
    _panGestureRecognizer.maximumNumberOfTouches = 2;

    _pinchGestureRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(respondToPinchGesture:)];
    _rotationGestureRecognizer = [[UIRotationGestureRecognizer alloc] initWithTarget:self action:@selector(respondToRotationGesture:)];
    _shoveGestureRecognizer = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(respondToShoveGesture:)];
    _shoveGestureRecognizer.minimumNumberOfTouches = 2;
    _shoveGestureRecognizer.maximumNumberOfTouches = 2;
    _longPressGestureRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(respondToLongPressGesture:)];

    // Use the delegate method 'shouldRecognizeSimultaneouslyWithGestureRecognizer' for gestures that can be concurrent
    _panGestureRecognizer.delegate = self;
    _pinchGestureRecognizer.delegate = self;
    _rotationGestureRecognizer.delegate = self;

    [self addGestureRecognizer:_tapGestureRecognizer];
    [self addGestureRecognizer:_doubleTapGestureRecognizer];
    [self addGestureRecognizer:_panGestureRecognizer];
    [self addGestureRecognizer:_pinchGestureRecognizer];
    [self addGestureRecognizer:_rotationGestureRecognizer];
    [self addGestureRecognizer:_shoveGestureRecognizer];
    [self addGestureRecognizer:_longPressGestureRecognizer];
}

#pragma mark UIView methods

- (void)setBackgroundColor:(UIColor *)backgroundColor
{
    [super setBackgroundColor:backgroundColor];
    [self trySetMapDefaultBackground:backgroundColor];
}

- (void)didMoveToWindow
{
    [self setupDisplayLink];
    [super didMoveToWindow];
}

- (void)didMoveToSuperview
{
    [self setupDisplayLink];
    [super didMoveToSuperview];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    CGSize size = self.bounds.size;
    self.map->resize(size.width * self.contentScaleFactor, size.height * self.contentScaleFactor);

    [self requestRender];
}

#pragma mark Memory Management

- (void)didReceiveMemoryWarning
{
    self.map->onMemoryWarning();
}

#pragma mark Rendering Behavior

- (void)requestRender
{
    _renderRequested = YES;
}

- (void)displayLinkUpdate:(CADisplayLink *)sender
{
    if (_renderRequested || self.continuous) {
        _renderRequested = NO;

        CFTimeInterval dt = _displayLink.targetTimestamp - _displayLink.timestamp;
        auto mapState = self.map->update(dt);

        BOOL mapViewComplete = mapState.viewComplete();
        BOOL viewComplete = mapViewComplete && !_prevMapViewComplete;

        BOOL cameraEasing = mapState.viewChanging();
        if (cameraEasing) {
            [self setMapRegionChangeState:TGMapRegionAnimating];
        } else if (_prevCameraEasing) {
            [self setMapRegionChangeState:TGMapRegionIdle];
        }

        _prevCameraEasing = cameraEasing;

        // When invoking delegate selectors like this below, we don't need to check whether the delegate is `nil`. `nil` is
        // a valid object that returns `0`, `nil`, or `NO` from all messages, including `respondsToSelector`. So we can use
        // `respondsToSelector` to check for delegate nullity and selector response at the same time. MEB 2018.7.16

        if (viewComplete && [self.mapViewDelegate respondsToSelector:@selector(mapViewDidCompleteLoading:)]) {
            [self.mapViewDelegate mapViewDidCompleteLoading:self];
        }

        if ([self.mapViewDelegate respondsToSelector:@selector(mapView:didCaptureScreenshot:)]) {
            if (_shouldCaptureFrame && (!_captureFrameWaitForViewComplete || viewComplete)) {

                UIImage *screenshot = [_glView snapshot];

                // For now we only have the GLKView to capture. In the future, to capture a view heirarchy including any
                // other subviews, we can use the alternative approach below. MEB 2018.7.16
                // UIGraphicsBeginImageContext(self.frame.size);
                // [self drawViewHierarchyInRect:self.frame afterScreenUpdates:YES];
                // UIImage* screenshot = UIGraphicsGetImageFromCurrentImageContext();
                // UIGraphicsEndImageContext();

                [self.mapViewDelegate mapView:self didCaptureScreenshot:screenshot];

                _shouldCaptureFrame = NO;
            }
        }

        _prevMapViewComplete = mapViewComplete;

        [self.glView display];

        if (mapState.isAnimating()) {
            [self requestRender];
        }
    }
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    if (_viewInBackground) {
        return;
    }

    self.map->render();
}

#pragma mark Screenshots

- (void)captureScreenshot:(BOOL)waitForViewComplete
{
    _captureFrameWaitForViewComplete = waitForViewComplete;
    _shouldCaptureFrame = YES;
    [self requestRender];
}

#pragma mark Markers

- (NSArray<TGMarker *> *)markers
{
    if (!self.map) {
        NSArray* values = [[NSArray alloc] init];
        return values;
    }

    return [self.markersById allValues];
}

- (TGMarker*)markerAdd
{
    TGMarker* marker = [[TGMarker alloc] initWithMap:self.map];
    NSString* key = [NSString stringWithFormat:@"%d", marker.identifier];
    self.markersById[key] = marker;
    return marker;
}

- (void)markerRemove:(TGMarker *)marker
{
    NSString* key = [NSString stringWithFormat:@"%d", marker.identifier];
    self.map->markerRemove(marker.identifier);
    [self.markersById removeObjectForKey:key];
    marker.identifier = 0;
}

- (void)markerRemoveAll
{
    if (!self.map) { return; }
    for (id markerId in self.markersById) {
        TGMarker* marker = [self.markersById objectForKey:markerId];
        marker.identifier = 0;
    }
    [self.markersById removeAllObjects];
    self.map->markerRemoveAll();
}

#pragma mark Debugging

- (void)setDebugFlag:(TGDebugFlag)debugFlag value:(BOOL)on
{
    Tangram::setDebugFlag((Tangram::DebugFlags)debugFlag, on);
}

- (BOOL)getDebugFlag:(TGDebugFlag)debugFlag
{
    return Tangram::getDebugFlag((Tangram::DebugFlags)debugFlag);
}

- (void)toggleDebugFlag:(TGDebugFlag)debugFlag
{
    Tangram::toggleDebugFlag((Tangram::DebugFlags)debugFlag);
}

#pragma mark Data Layers

- (TGMapData *)addDataLayer:(NSString *)name
{
    return [self addDataLayer:name generateCentroid:NO];
}

- (TGMapData *)addDataLayer:(NSString *)name generateCentroid:(BOOL)generateCentroid
{
    if (!self.map) { return nil; }

    std::string dataLayerName = std::string([name UTF8String]);
    auto source = std::make_shared<Tangram::ClientDataSource>(self.map->getPlatform(),
                    dataLayerName, "", generateCentroid);
    self.map->addTileSource(source);

    __weak TGMapView* weakSelf = self;
    TGMapData* clientData = [[TGMapData alloc] initWithMapView:weakSelf name:name source:source];
    self.dataLayersByName[name] = clientData;

    return clientData;
}

- (BOOL)removeDataSource:(std::shared_ptr<Tangram::TileSource>)tileSource name:(NSString *)name
{
    if (!self.map || !tileSource) { return NO; }

    [self.dataLayersByName removeObjectForKey:name];
    return self.map->removeTileSource(*tileSource);
}

- (void)clearDataSource:(std::shared_ptr<Tangram::TileSource>)tileSource
{
    if (!self.map || !tileSource) { return; }

    self.map->clearTileSource(*tileSource, true, true);
}

#pragma mark Loading Scenes

std::vector<Tangram::SceneUpdate> unpackSceneUpdates(NSArray<TGSceneUpdate *> *sceneUpdates)
{
    std::vector<Tangram::SceneUpdate> updates;
    if (sceneUpdates) {
        for (TGSceneUpdate* update in sceneUpdates) {
            updates.push_back({std::string([update.path UTF8String]), std::string([update.value UTF8String])});
        }
    }
    return updates;
}

- (Tangram::SceneReadyCallback)sceneReadyListener {
    __weak TGMapView* weakSelf = self;

    return [weakSelf](int sceneID, const Tangram::SceneError *sceneError) {
        __strong TGMapView* strongSelf = weakSelf;

        if (!strongSelf) {
            return;
        }

        [strongSelf.markersById removeAllObjects];
        [strongSelf requestRender];

        if (![strongSelf.mapViewDelegate respondsToSelector:@selector(mapView:didLoadScene:withError:)]) {
            return;
        }

        NSError* error = nil;

        if (sceneError) {
            error = TGConvertCoreSceneErrorToNSError(sceneError);
        }

        [strongSelf.mapViewDelegate mapView:strongSelf didLoadScene:sceneID withError:error];
    };
}

- (int)loadSceneFromURL:(NSURL *)url withUpdates:(nullable NSArray<TGSceneUpdate *> *)updates
{
    if (!self.map) { return -1; }

    auto sceneUpdates = unpackSceneUpdates(updates);

    self.map->setSceneReadyListener([self sceneReadyListener]);
    return self.map->loadScene([[url absoluteString] UTF8String], false, sceneUpdates);
}

- (int)loadSceneAsyncFromURL:(NSURL *)url withUpdates:(nullable NSArray<TGSceneUpdate *> *)updates
{
    if (!self.map) { return -1; }

    auto sceneUpdates = unpackSceneUpdates(updates);

    self.map->setSceneReadyListener([self sceneReadyListener]);
    return self.map->loadSceneAsync([[url absoluteString] UTF8String], false, sceneUpdates);
}

- (int)loadSceneFromYAML:(NSString *)yaml relativeToURL:(NSURL *)url withUpdates:(nullable NSArray<TGSceneUpdate *> *)updates
{
    if (!self.map) { return -1; }

    auto sceneUpdates = unpackSceneUpdates(updates);

    self.map->setSceneReadyListener([self sceneReadyListener]);
    return self.map->loadSceneYaml([yaml UTF8String], [[url absoluteString] UTF8String], false, sceneUpdates);
}

- (int)loadSceneAsyncFromYAML:(NSString *)yaml relativeToURL:(NSURL *)url withUpdates:(nullable NSArray<TGSceneUpdate *> *)updates
{
    if (!self.map) { return -1; }

    auto sceneUpdates = unpackSceneUpdates(updates);

    self.map->setSceneReadyListener([self sceneReadyListener]);
    return self.map->loadSceneYamlAsync([yaml UTF8String], [[url absoluteString] UTF8String], false, sceneUpdates);
}

#pragma mark Coordinate Conversions

- (CGPoint)viewPositionFromCoordinate:(CLLocationCoordinate2D)coordinate clipToViewport:(BOOL)clip
{
    if (!self.map) {
        return CGPointZero;
    }

    double screenX, screenY;
    bool inViewport = self.map->lngLatToScreenPosition(coordinate.longitude, coordinate.latitude, &screenX, &screenY, clip);
    screenX /= self.contentScaleFactor;
    screenY /= self.contentScaleFactor;
    return CGPointMake(screenX, screenY);
}

- (BOOL)viewportContainsCoordinate:(CLLocationCoordinate2D)coordinate
{
    if (!self.map) {
        return false;
    }

    double screenX, screenY;
    return self.map->lngLatToScreenPosition(coordinate.longitude, coordinate.latitude, &screenX, &screenY);
}

- (CLLocationCoordinate2D)coordinateFromViewPosition:(CGPoint)viewPosition
{
    if (!self.map) { return kCLLocationCoordinate2DInvalid; }

    viewPosition.x *= self.contentScaleFactor;
    viewPosition.y *= self.contentScaleFactor;

    CLLocationCoordinate2D coordinate;
    if (self.map->screenPositionToLngLat(viewPosition.x, viewPosition.y,
        &coordinate.longitude, &coordinate.latitude)) {
        return coordinate;
    }

    return kCLLocationCoordinate2DInvalid;
}

#pragma mark Picking Map Objects

- (CGFloat) getZoomVelocity
{
    if (!self.map) { return; }

    return self.map->getZoomVelocity();
}

- (void)setPickRadius:(CGFloat)logicalPixels
{
    if (!self.map) { return; }

    self.map->setPickRadius(logicalPixels);
}

- (void)pickFeatureAt:(CGFloat)x
                    y:(CGFloat)y
                    identifier:(int)identifier
{
    if (!self.map) { return; }

    x *= self.contentScaleFactor;
    y *= self.contentScaleFactor;

    __weak TGMapView* weakSelf = self;
    self.map->pickFeatureAt(x, y, identifier, [weakSelf, identifier](const Tangram::FeaturePickResult* featureResult) {
        __strong TGMapView* strongSelf = weakSelf;

        if (!strongSelf || ![strongSelf.mapViewDelegate respondsToSelector:@selector(mapView:didSelectFeature:atScreenPosition:)]) {
            return;
        }

        CGPoint position = CGPointZero;

        if (!featureResult) {
            [strongSelf.mapViewDelegate mapView:strongSelf didSelectFeature:nil atScreenPosition:position identifier:identifier];
            return;
        }

        NSMutableDictionary* featureProperties = [[NSMutableDictionary alloc] init];

        const auto& properties = featureResult->properties;
        position = CGPointMake(featureResult->position[0] / strongSelf.contentScaleFactor,
                               featureResult->position[1] / strongSelf.contentScaleFactor);

        for (const auto& item : properties->items()) {
            NSString* key = [NSString stringWithUTF8String:item.key.c_str()];
            NSString* value = [NSString stringWithUTF8String:properties->asString(item.value).c_str()];
            featureProperties[key] = value;
        }

        [strongSelf.mapViewDelegate mapView:strongSelf didSelectFeature:featureProperties atScreenPosition:position identifier:identifier];
    });
}

- (void)pickMarkerAt:(CGFloat)x
                   y:(CGFloat)y
          identifier:(int)identifier
{
    if (!self.map) { return; }

    x *= self.contentScaleFactor;
    y *= self.contentScaleFactor;

    __weak TGMapView* weakSelf = self;
    self.map->pickMarkerAt(x, y, identifier, [weakSelf, identifier](const Tangram::MarkerPickResult* markerPickResult) {
        __strong TGMapView* strongSelf = weakSelf;

        if (!strongSelf || ![strongSelf.mapViewDelegate respondsToSelector:@selector(mapView:didSelectMarker:atScreenPosition:identifier:)]) {
            return;
        }

        CGPoint position = CGPointZero;

        if (!markerPickResult) {
            [strongSelf.mapViewDelegate mapView:strongSelf didSelectMarker:nil atScreenPosition:position identifier:identifier];
            return;
        }

        NSString* key = [NSString stringWithFormat:@"%d", markerPickResult->id];
        TGMarker* marker = [strongSelf.markersById objectForKey:key];

        if (!marker) {
            [strongSelf.mapViewDelegate mapView:strongSelf didSelectMarker:nil atScreenPosition:position identifier:identifier];
            return;
        }

        position = CGPointMake(markerPickResult->position[0] / strongSelf.contentScaleFactor,
                               markerPickResult->position[1] / strongSelf.contentScaleFactor);

        CLLocationCoordinate2D coordinate = CLLocationCoordinate2DMake(markerPickResult->coordinates.latitude,
                                                                       markerPickResult->coordinates.longitude);

        TGMarkerPickResult* result = [[TGMarkerPickResult alloc] initWithCoordinate:coordinate marker:marker];
        [strongSelf.mapViewDelegate mapView:strongSelf didSelectMarker:result atScreenPosition:position identifier:identifier];
    });
}

- (void)pickLabelAt:(CGFloat)x
                  y:(CGFloat)y
         identifier:(int)identifier
{
    if (!self.map) { return; }

    x *= self.contentScaleFactor;
    y *= self.contentScaleFactor;

    __weak TGMapView* weakSelf = self;
    self.map->pickLabelAt(x, y, identifier, [weakSelf, identifier](const Tangram::LabelPickResult* labelPickResult) {
        __strong TGMapView* strongSelf = weakSelf;

        if (!strongSelf || ![strongSelf.mapViewDelegate respondsToSelector:@selector(mapView:didSelectLabel:atScreenPosition:)]) {
            return;
        }

        CGPoint position = CGPointMake(0.0, 0.0);

        if (!labelPickResult) {
            [strongSelf.mapViewDelegate mapView:strongSelf didSelectLabel:nil atScreenPosition:position identifier:identifier];
            return;
        }

        NSMutableDictionary* featureProperties = [[NSMutableDictionary alloc] init];

        const auto& touchItem = labelPickResult->touchItem;
        const auto& properties = touchItem.properties;
        position = CGPointMake(touchItem.position[0] / strongSelf.contentScaleFactor,
                               touchItem.position[1] / strongSelf.contentScaleFactor);

        for (const auto& item : properties->items()) {
            NSString* key = [NSString stringWithUTF8String:item.key.c_str()];
            NSString* value = [NSString stringWithUTF8String:properties->asString(item.value).c_str()];
            featureProperties[key] = value;
        }

        CLLocationCoordinate2D coordinate = {labelPickResult->coordinates.latitude, labelPickResult->coordinates.longitude};
        TGLabelPickResult* tgLabelPickResult = [[TGLabelPickResult alloc] initWithCoordinate:coordinate
                                                                                         type:(TGLabelType)labelPickResult->type
                                                                                   properties:featureProperties];
        [strongSelf.mapViewDelegate mapView:strongSelf didSelectLabel:tgLabelPickResult atScreenPosition:position identifier:identifier];
    });
}

#pragma mark Changing the Map Viewport

- (CGFloat)minimumZoomLevel
{
    if (!self.map) { return 0; }
    return self.map->getMinZoom();
}

- (void)setMinimumZoomLevel:(CGFloat)minimumZoomLevel
{
    if (!self.map) { return; }
    self.map->setMinZoom(minimumZoomLevel);
}

- (CGFloat)maximumZoomLevel
{
    if (!self.map) { return 0; }
    return self.map->getMaxZoom();
}

- (void)setMaximumZoomLevel:(CGFloat)maximumZoomLevel
{
    if (!self.map) { return; }
    self.map->setMaxZoom(maximumZoomLevel);
}

- (void)setPosition:(CLLocationCoordinate2D)position {
    if (!self.map) { return; }

    [self setMapRegionChangeState:TGMapRegionJumping];
    self.map->setPosition(position.longitude, position.latitude);
    [self setMapRegionChangeState:TGMapRegionIdle];
}

- (CLLocationCoordinate2D)position
{
    if (!self.map) { return kCLLocationCoordinate2DInvalid; }

    CLLocationCoordinate2D coordinate;
    self.map->getPosition(coordinate.longitude, coordinate.latitude);

    return coordinate;
}

- (void)setZoom:(CGFloat)zoom
{
    if (!self.map) { return; }

    [self setMapRegionChangeState:TGMapRegionJumping];
    self.map->setZoom(zoom);
    [self setMapRegionChangeState:TGMapRegionIdle];
}

- (CGFloat)zoom
{
    if (!self.map) { return 0.0; }
    return self.map->getZoom();
}

- (void)setRotation:(CLLocationDirection)rotation
{
    if (!self.map) { return; }

    [self setMapRegionChangeState:TGMapRegionJumping];
    self.map->setRotation(rotation);
    [self setMapRegionChangeState:TGMapRegionIdle];
}

- (CGFloat)rotation
{
    if (!self.map) { return 0.0; }
    return self.map->getRotation();
}

- (CGFloat)tilt
{
    if (!self.map) { return 0.f; }
    return self.map->getTilt();
}

- (void)setTilt:(CGFloat)tilt
{
    if (!self.map) { return; }

    [self setMapRegionChangeState:TGMapRegionJumping];
    return self.map->setTilt(tilt);
    [self setMapRegionChangeState:TGMapRegionIdle];
}

- (TGCameraPosition *)cameraPosition
{
    Tangram::CameraPosition camera = self.map->getCameraPosition();
    TGCameraPosition *result = [[TGCameraPosition alloc] initWithCoreCamera:&camera];
    return result;
}

- (void)setCameraPosition:(TGCameraPosition *)cameraPosition
{
    Tangram::CameraPosition result = [cameraPosition convertToCoreCamera];
    [self setMapRegionChangeState:TGMapRegionJumping];
    self.map->setCameraPosition(result);
    [self setMapRegionChangeState:TGMapRegionIdle];
}

- (void)setCameraPosition:(TGCameraPosition *)cameraPosition
             withDuration:(NSTimeInterval)duration
                 easeType:(TGEaseType)easeType
                 callback:(void (^)(BOOL))callback
{
    Tangram::CameraPosition camera = [cameraPosition convertToCoreCamera];
    Tangram::EaseType ease = TGConvertTGEaseTypeToCoreEaseType(easeType);
    if (duration > 0) {
        [self setMapRegionChangeState:TGMapRegionAnimating];
    } else {
        [self setMapRegionChangeState:TGMapRegionJumping];
    }
    self.map->setCameraPositionEased(camera, duration, ease);
    self.cameraAnimationCallback = callback;
}

- (void)flyToCameraPosition:(TGCameraPosition *)cameraPosition callback:(void (^)(BOOL))callback
{
    [self flyToCameraPosition:cameraPosition withDuration:-1.0 speed:1.0 callback:callback];
}

- (void)flyToCameraPosition:(TGCameraPosition *)cameraPosition
               withDuration:(NSTimeInterval)duration
                   callback:(void (^)(BOOL))callback
{
    [self flyToCameraPosition:cameraPosition withDuration:duration speed:1.0 callback:callback];
}

- (void)flyToCameraPosition:(TGCameraPosition *)cameraPosition
                  withSpeed:(CGFloat)speed
                   callback:(void (^)(BOOL))callback
{
    [self flyToCameraPosition:cameraPosition withDuration:-1 speed:speed callback:callback];
}

- (TGCameraPosition *)cameraThatFitsBounds:(TGCoordinateBounds)bounds
{
    if (!self.map) { return nil; }
    Tangram::LngLat sw = TGConvertCLLocationCoordinate2DToCoreLngLat(bounds.sw);
    Tangram::LngLat ne = TGConvertCLLocationCoordinate2DToCoreLngLat(bounds.ne);
    Tangram::CameraPosition camera = self.map->getEnclosingCameraPosition(sw, ne);
    return [[TGCameraPosition alloc] initWithCoreCamera:&camera];
}

- (TGCameraPosition *)cameraThatFitsBounds:(TGCoordinateBounds)bounds withPadding:(UIEdgeInsets)padding
{
    if (!self.map) { return nil; }

    Tangram::LngLat sw = TGConvertCLLocationCoordinate2DToCoreLngLat(bounds.sw);
    Tangram::LngLat ne = TGConvertCLLocationCoordinate2DToCoreLngLat(bounds.ne);
    Tangram::EdgePadding pad = TGConvertUIEdgeInsetsToCoreEdgePadding(padding, self.map->getPixelScale());
    Tangram::CameraPosition camera = self.map->getEnclosingCameraPosition(sw, ne, pad);
    return [[TGCameraPosition alloc] initWithCoreCamera:&camera];
}

#pragma mark Camera type

- (TGCameraType)cameraType
{
    switch (self.map->getCameraType()) {
        case 0:
            return TGCameraTypePerspective;
        case 1:
            return TGCameraTypeIsometric;
        case 2:
            return TGCameraTypeFlat;
        default:
            return TGCameraTypePerspective;
    }
}

- (void)setCameraType:(TGCameraType)cameraType
{
    if (!self.map) { return; }

    self.map->setCameraType(static_cast<int>(cameraType));
}

#pragma mark View Padding

- (UIEdgeInsets)padding
{
    if (!self.map) { return UIEdgeInsetsZero; }

    return TGConvertCoreEdgePaddingToUIEdgeInsets(self.map->getPadding(), self.map->getPixelScale());
}

- (void)setPadding:(UIEdgeInsets)padding
{
    if (!self.map) { return; }

    self.map->setPadding(TGConvertUIEdgeInsetsToCoreEdgePadding(padding, self.map->getPixelScale()));
}

#pragma mark Gesture Recognizers

- (void)setTapGestureRecognizer:(UITapGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_tapGestureRecognizer) {
        [self removeGestureRecognizer:_tapGestureRecognizer];
    }
    _tapGestureRecognizer = recognizer;
    [self addGestureRecognizer:_tapGestureRecognizer];
}

- (void)setDoubleTapGestureRecognizer:(UITapGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_doubleTapGestureRecognizer) {
        [self removeGestureRecognizer:_doubleTapGestureRecognizer];
    }
    _doubleTapGestureRecognizer = recognizer;
    [self addGestureRecognizer:_doubleTapGestureRecognizer];
}

- (void)setPanGestureRecognizer:(UIPanGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_panGestureRecognizer) {
        [self removeGestureRecognizer:_panGestureRecognizer];
    }
    _panGestureRecognizer = recognizer;
    [self addGestureRecognizer:_panGestureRecognizer];
}

- (void)setPinchGestureRecognizer:(UIPinchGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_pinchGestureRecognizer) {
        [self removeGestureRecognizer:_pinchGestureRecognizer];
    }
    _pinchGestureRecognizer = recognizer;
    [self addGestureRecognizer:_pinchGestureRecognizer];
}

- (void)setRotationGestureRecognizer:(UIRotationGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_rotationGestureRecognizer) {
        [self removeGestureRecognizer:_rotationGestureRecognizer];
    }
    _rotationGestureRecognizer = recognizer;
    [self addGestureRecognizer:_rotationGestureRecognizer];
}

- (void)setShoveGestureRecognizer:(UIPanGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_shoveGestureRecognizer) {
        [self removeGestureRecognizer:_shoveGestureRecognizer];
    }
    _shoveGestureRecognizer = recognizer;
    [self addGestureRecognizer:_shoveGestureRecognizer];
}

- (void)setLongPressGestureRecognizer:(UILongPressGestureRecognizer *)recognizer
{
    if (!recognizer) { return; }
    if (_longPressGestureRecognizer) {
        [self removeGestureRecognizer:_longPressGestureRecognizer];
    }
    _longPressGestureRecognizer = recognizer;
    [self addGestureRecognizer:_longPressGestureRecognizer];
}

// Implement touchesBegan to catch down events
- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    self.map->handlePanGesture(0.0f, 0.0f, 0.0f, 0.0f);
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    // make shove gesture exclusive
    if ([gestureRecognizer isEqual:_shoveGestureRecognizer] || [otherGestureRecognizer isEqual:_shoveGestureRecognizer]) {
        return NO;
    }
    return YES;
}

#pragma mark - Gesture Responders

- (void)respondToLongPressGesture:(UILongPressGestureRecognizer *)longPressRecognizer
{
    CGPoint location = [longPressRecognizer locationInView:self];
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizeLongPressGesture:)] ) {
        if (![self.gestureDelegate mapView:self recognizer:longPressRecognizer shouldRecognizeLongPressGesture:location]) { return; }
    }

    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizeLongPressGesture:)]) {
        [self.gestureDelegate mapView:self recognizer:longPressRecognizer didRecognizeLongPressGesture:location];
    }
}

- (void)respondToTapGesture:(UITapGestureRecognizer *)tapRecognizer
{
    CGPoint location = [tapRecognizer locationInView:self];
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizeSingleTapGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:tapRecognizer shouldRecognizeSingleTapGesture:location]) { return; }
    }

    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizeSingleTapGesture:)]) {
        [self.gestureDelegate mapView:self recognizer:tapRecognizer didRecognizeSingleTapGesture:location];
    }
}

- (void)respondToDoubleTapGesture:(UITapGestureRecognizer *)doubleTapRecognizer
{
    CGPoint location = [doubleTapRecognizer locationInView:self];
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizeDoubleTapGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:doubleTapRecognizer shouldRecognizeDoubleTapGesture:location]) { return; }
    }

    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizeDoubleTapGesture:)]) {
        [self.gestureDelegate mapView:self recognizer:doubleTapRecognizer didRecognizeDoubleTapGesture:location];
    }
}

- (void)respondToPanGesture:(UIPanGestureRecognizer *)panRecognizer
{
    CGPoint displacement = [panRecognizer translationInView:self];

    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizePanGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:panRecognizer shouldRecognizePanGesture:displacement]) {
            return;
        }
    }

    CGPoint velocity = [panRecognizer velocityInView:_glView];
    CGPoint end = [panRecognizer locationInView:_glView];
    CGPoint start = {end.x - displacement.x, end.y - displacement.y};
    
    bool wasAlreadyHandled = false;
    
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizePanGesture:)]) {
        wasAlreadyHandled = [self.gestureDelegate mapView:self recognizer:panRecognizer didRecognizePanGesture:displacement];
    }

    switch (panRecognizer.state) {
        case UIGestureRecognizerStateBegan:
            [self setMapRegionChangeState:TGMapRegionJumping];
            break;
        case UIGestureRecognizerStateChanged:
            if(!wasAlreadyHandled)
            {
                [self setMapRegionChangeState:TGMapRegionJumping];
                
                
                self.map->handlePanGesture(start.x * self.contentScaleFactor, start.y * self.contentScaleFactor, end.x * self.contentScaleFactor, end.y * self.contentScaleFactor);
            }
            break;
        case UIGestureRecognizerStateEnded:
            // no need for fling, not like this at least
            /*
            self.map->handleFlingGesture(end.x * self.contentScaleFactor, end.y * self.contentScaleFactor, velocity.x * self.contentScaleFactor, velocity.y * self.contentScaleFactor);
            [self setMapRegionChangeState:TGMapRegionIdle];
             */
            break;
        default:
            break;
    }

    // Reset translation to zero so that subsequent calls get relative value.
    [panRecognizer setTranslation:CGPointZero inView:self];
}

- (void)respondToPinchGesture:(UIPinchGestureRecognizer *)pinchRecognizer
{
    CGPoint location = [pinchRecognizer locationInView:self];
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizePinchGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:pinchRecognizer shouldRecognizePinchGesture:location]) {
            return;
        }
    }

    CGFloat scale = pinchRecognizer.scale;
    
    bool wasAlreadyHandled = false;
    
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizePinchGesture:)]) {
        [self.gestureDelegate mapView:self recognizer:pinchRecognizer didRecognizePinchGesture:location];
    }
    
    switch (pinchRecognizer.state) {
        case UIGestureRecognizerStateBegan:
            [self setMapRegionChangeState:TGMapRegionJumping];
            break;
        case UIGestureRecognizerStateChanged:

            // NOTE: Based on the documentation https://developer.apple.com/documentation/uikit/uipinchgesturerecognizer,
            // Pinch gesture state should not be set to "Changed" if "both" fingers are not pressed, which does not seem to be the case.
            // Following a work-around to avoid this bug.
            if ([pinchRecognizer numberOfTouches] < 2) {
                break;
            }

            if(!wasAlreadyHandled){
                [self setMapRegionChangeState:TGMapRegionJumping];
                if ([self.gestureDelegate respondsToSelector:@selector(pinchFocus:recognizer:)]) {
                    CGPoint focusPosition = [self.gestureDelegate pinchFocus:self recognizer:pinchRecognizer];
                    self.map->handlePinchGesture(focusPosition.x * self.contentScaleFactor, focusPosition.y * self.contentScaleFactor, scale, pinchRecognizer.velocity);
                } else {
                    self.map->handlePinchGesture(location.x * self.contentScaleFactor, location.y * self.contentScaleFactor, scale, pinchRecognizer.velocity);
                }
            }
            break;
        case UIGestureRecognizerStateEnded:
            [self setMapRegionChangeState:TGMapRegionIdle];
            break;
        default:
            break;
    }

    // Reset scale to 1 so that subsequent calls get relative value.
    [pinchRecognizer setScale:1.0];
}

- (void)respondToRotationGesture:(UIRotationGestureRecognizer *)rotationRecognizer
{
    CGPoint position = [rotationRecognizer locationInView:self];
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizeRotationGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:rotationRecognizer shouldRecognizeRotationGesture:position]) {
            return;
        }
    }

    CGFloat rotation = rotationRecognizer.rotation;
    
    bool wasAlreadyHandled = false;
    
    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizeRotationGesture:)]) {
        wasAlreadyHandled = [self.gestureDelegate mapView:self recognizer:rotationRecognizer didRecognizeRotationGesture:position];
    }
    
    switch (rotationRecognizer.state) {
        case UIGestureRecognizerStateBegan:
            [self setMapRegionChangeState:TGMapRegionJumping];
            break;
        case UIGestureRecognizerStateChanged:
            if(!wasAlreadyHandled)
            {
                [self setMapRegionChangeState:TGMapRegionJumping];
                if ([self.gestureDelegate respondsToSelector:@selector(rotationFocus:recognizer:)]) {
                    CGPoint focusPosition = [self.gestureDelegate rotationFocus:self recognizer:rotationRecognizer];
                    self.map->handleRotateGesture(focusPosition.x * self.contentScaleFactor, focusPosition.y * self.contentScaleFactor, rotation);
                } else {
                    self.map->handleRotateGesture(position.x * self.contentScaleFactor, position.y * self.contentScaleFactor, rotation);
                }
            }
            break;
        case UIGestureRecognizerStateEnded:
            [self setMapRegionChangeState:TGMapRegionIdle];
            break;
        default:
            break;
    }

    // Reset rotation to zero so that subsequent calls get relative value.
    [rotationRecognizer setRotation:0.0];
}

- (void)respondToShoveGesture:(UIPanGestureRecognizer *)shoveRecognizer
{
    CGPoint displacement = [shoveRecognizer translationInView:self];

    if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:shouldRecognizeShoveGesture:)]) {
        if (![self.gestureDelegate mapView:self recognizer:shoveRecognizer shouldRecognizeShoveGesture:displacement]) {
            return;
        }
    }

    switch (shoveRecognizer.state) {
        case UIGestureRecognizerStateBegan:
            [self setMapRegionChangeState:TGMapRegionJumping];
            break;
        case UIGestureRecognizerStateChanged:
        {
            bool wasAlreadyHandled = false;
            
            if ([self.gestureDelegate respondsToSelector:@selector(mapView:recognizer:didRecognizeShoveGesture:)]) {
                wasAlreadyHandled = [self.gestureDelegate mapView:self recognizer:shoveRecognizer didRecognizeShoveGesture:displacement];
            }
            
            if(!wasAlreadyHandled){
                [self setMapRegionChangeState:TGMapRegionJumping];
                self.map->handleShoveGesture(displacement.y);
            }
            break;
        }
        case UIGestureRecognizerStateEnded:
            [self setMapRegionChangeState:TGMapRegionIdle];
            break;
        default:
            break;
    }

    // Reset translation to zero so that subsequent calls get relative value.
    [shoveRecognizer setTranslation:CGPointZero inView:_glView];
}

#pragma mark Map region change state notifiers

- (void)notifyGestureDidBegin
{
    [self setMapRegionChangeState:TGMapRegionAnimating];
}

- (void)notifyGestureIsChanging
{
    [self setMapRegionChangeState:TGMapRegionAnimating];
}

- (void)notifyGestureDidEnd
{
    [self setMapRegionChangeState:TGMapRegionIdle];
}

#pragma mark Internal Logic

- (void)flyToCameraPosition:(TGCameraPosition *)cameraPosition
               withDuration:(NSTimeInterval)duration
                      speed:(CGFloat)speed
                   callback:(void (^)(BOOL))callback
{
    if (!_map) { return; }
    Tangram::CameraPosition camera = [cameraPosition convertToCoreCamera];
    if (duration == 0) {
        [self setMapRegionChangeState:TGMapRegionJumping];
    } else {
        [self setMapRegionChangeState:TGMapRegionAnimating];
    }
    self.map->flyTo(camera, duration, speed);
    self.cameraAnimationCallback = callback;
}

- (void)setMapRegionChangeState:(TGMapRegionChangeState)state
{
    if (!_map) { return; }
    switch (_currentState) {
        case TGMapRegionIdle:
            if (state == TGMapRegionJumping) {
                [self regionWillChangeAnimated:NO];
            } else if (state == TGMapRegionAnimating) {
                [self regionWillChangeAnimated:YES];
            }
            break;
        case TGMapRegionJumping:
            if (state == TGMapRegionIdle) {
                [self regionDidChangeAnimated:NO];
            } else if (state == TGMapRegionJumping) {
                [self regionIsChanging];
            }
            break;
        case TGMapRegionAnimating:
            if (state == TGMapRegionIdle) {
                [self regionDidChangeAnimated:YES];
            } else if (state == TGMapRegionAnimating) {
                [self regionIsChanging];
            }
            break;
        default:
            break;
    }
    _currentState = state;
}

- (void)regionWillChangeAnimated:(BOOL)animated
{
    if (!_map) { return; }
    if ([self.mapViewDelegate respondsToSelector:@selector(mapView:regionWillChangeAnimated:)]) {
        [self.mapViewDelegate mapView:self regionWillChangeAnimated:animated];
    }
}

- (void)regionIsChanging
{
    if (!_map) { return; }
    if ([self.mapViewDelegate respondsToSelector:@selector(mapViewRegionIsChanging:)]) {
        [self.mapViewDelegate mapViewRegionIsChanging:self];
    }
}

- (void)regionDidChangeAnimated:(BOOL)animated
{
    if (!_map) { return; }
    if ([self.mapViewDelegate respondsToSelector:@selector(mapView:regionDidChangeAnimated:)]) {
        [self.mapViewDelegate mapView:self regionDidChangeAnimated:animated];
    }
}

- (void)trySetMapDefaultBackground:(UIColor *)backgroundColor
{
    if (_map) {
        CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
        [backgroundColor getRed:&red green:&green blue:&blue alpha:&alpha];
        _map->setDefaultBackgroundColor(red, green, blue);
    }
}

- (bool) layerExists: (NSString*)layerName
{
    if (!_map) { return; }
    std::string layer = std::string([layerName UTF8String]);
    _map->layerExists(layer);
    return false;
}

- (void) setLayer: (NSString*)layerName yaml:(NSString*)yaml
{
    if (!_map) { return; }
    std::string layer = std::string([layerName UTF8String]);
    std::string definition = std::string([yaml UTF8String]);
    _map->setLayer(layer, definition);
}

- (bool) setTileSourceUrl: (NSString*)sourceName url:(NSString*)url
{
    if (!_map) { return; }
    std::string source = std::string([sourceName UTF8String]);
    std::string newUrl = std::string([url UTF8String]);
    
    return _map->setTileSourceUrl(source, newUrl);
}

- (NSString*) getTileSourceUrl: (NSString*)sourceName {
    if (!_map) { return; }
    std::string source = std::string([sourceName UTF8String]);
    std::string url = _map->getTileSourceUrl(source);
    return [NSString stringWithUTF8String:url.c_str()];
}

- (void) clearTileCache: (int)sourceId
{
    if (!_map) { return; }
    _map->clearTileCache(sourceId);
}

- (void)cancelCameraAnimation
{
    if (!_map) { return; }
    _map->cancelCameraAnimation();
}

-(void) handlePanPinchRotateFlingShove:
                        (CGFloat) panStartX
                        panStartY:(CGFloat) panStartY
                        panEndX: (CGFloat) panEndX
                        panEndY: (CGFloat) panEndY
                        pinchPosX: (CGFloat) pinchPosX
                        pinchPosY: (CGFloat) pinchPosY
                        pinchValue: (CGFloat) pinchValue
                        pinchVelocity: (CGFloat) pinchVelocity
                        rotPosX: (CGFloat) rotPosX
                        rotPosY: (CGFloat) rotPosY
                        rotRadians: (CGFloat) rotRadians
                        flingPosX: (CGFloat) flingPosX
                        flingPosY: (CGFloat) flingPosY
                        flingVelocityX: (CGFloat) flingVelocityX
                        flingVelocityY: (CGFloat) flingVelocityY
                         shoveDistance:(CGFloat) shoveDistance
{
    if (!_map) { return; }
 
    _map->handlePanPinchRotateFlingShove(panStartX, panStartY,panEndX, panEndY,pinchPosX,pinchPosY,pinchValue, pinchVelocity,rotPosX, rotPosY,rotRadians, flingPosX,flingPosY, flingVelocityX,flingVelocityY,shoveDistance);
}


- (bool) setTileSourceVisibility: (NSString*)layerName isVisible:(bool) isVisible
{
    if(!_map) return false;
    std::string stdLayerName = std::string([layerName UTF8String]);
    return _map->setTileSourceVisibility(stdLayerName, isVisible);
}
@end // implementation TGMapView
