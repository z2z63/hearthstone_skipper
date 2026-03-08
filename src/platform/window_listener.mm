#include <Cocoa/Cocoa.h>
#include <functional>

#include "spdlog/spdlog.h"
#include "window_listener.h"
#include <QRect>
#include <QTimer>
#include <QWidget>

@interface AppLaunchObserver : NSObject
@property HearthStoneWindowListener *listener;
@property AXUIElementRef hearthstoneWindowRef;
@property(nonatomic, assign) AXObserverRef windowObserver;
@property pid_t pid;
@property std::shared_ptr<spdlog::logger> logger;
@property int retryCount;
@end

@implementation AppLaunchObserver
- (instancetype)initWithListener:(HearthStoneWindowListener *)listener {
    self = [super init];
    self.logger = spdlog::get("skipper");
    self.windowObserver = nullptr;
    self.retryCount = 30;
    if (self) {
        self.listener = listener;
        NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
        [center addObserver:self
                   selector:@selector(appGetFocused:)
                       name:NSWorkspaceDidActivateApplicationNotification
                     object:nil];
        [center addObserver:self
                   selector:@selector(appGetFocused:)
                       name:NSWorkspaceActiveSpaceDidChangeNotification
                     object:nil];
        [center addObserver:self
                   selector:@selector(appLoseFocused:)
                       name:NSWorkspaceDidDeactivateApplicationNotification
                     object:nil];
        [center addObserver:self
                   selector:@selector(appLaunched:)
                       name:NSWorkspaceDidLaunchApplicationNotification
                     object:nil];
        [center addObserver:self
                   selector:@selector(appTerminated:)
                       name:NSWorkspaceDidTerminateApplicationNotification
                     object:nil];
    }
    return self;
}

void windowPositionUpdateCallback(AXObserverRef observer, AXUIElementRef element, CFStringRef notification,
                                  void *refcon) {
    auto *self = (__bridge AppLaunchObserver *)refcon;
    auto rect = [self getWindowLocation];
    if (rect.isNull()) {
        return;
    }
    emit self.listener->onAppMove(rect);
}

// static NSString *bundleIdentifier = @"org.alacritty";
static NSString *bundleIdentifier = @"unity.Blizzard Entertainment.Hearthstone";

- (void)appGetFocused:(NSNotification *)notification {
    NSDictionary *userInfo = [notification userInfo];
    NSRunningApplication *app = userInfo[NSWorkspaceApplicationKey];
    if (![app.bundleIdentifier isEqual:bundleIdentifier]) {
        return;
    }
    if (self.windowObserver == nullptr) {
        SPDLOG_LOGGER_INFO(_logger, "Hearthstone get focus, try setup window listener");
        [self setupListener:app];
    }
    QRect rect = [self getWindowLocation];
    if (!rect.isNull()) {
        emit self.listener->onAppGetFocus(rect);
    }
}

- (void)appLoseFocused:(NSNotification *)notification {
    NSDictionary *userInfo = [notification userInfo];
    NSRunningApplication *app = userInfo[NSWorkspaceApplicationKey];
    if ([app.bundleIdentifier isEqual:bundleIdentifier]) {
        emit self.listener->onAppLoseFocus();
    }
}

- (void)appLaunched:(NSNotification *)notification {
    NSDictionary *userInfo = [notification userInfo];
    NSRunningApplication *app = userInfo[NSWorkspaceApplicationKey];
    if (![app.bundleIdentifier isEqual:bundleIdentifier]) {
        return;
    }
    [self startRetrySetupListener:app.processIdentifier];
    //    QRect rect = [self getWindowLocation];
    //    if (rect.isNull()) {
    //        SPDLOG_LOGGER_INFO(_logger, "window rect is null");
    //        return;
    //    }
    //    emit self.listener->onAppLaunch(rect);
}

- (void)appTerminated:(NSNotification *)notification {
    NSDictionary *userInfo = [notification userInfo];
    NSRunningApplication *app = userInfo[NSWorkspaceApplicationKey];
    if (![app.bundleIdentifier isEqual:bundleIdentifier]) {
        return;
    }
    [self closeListener];
    if (self.hearthstoneWindowRef) {
        CFRelease(self.hearthstoneWindowRef);
        self.hearthstoneWindowRef = nullptr;
    }
    self.pid = 0;
    emit self.listener->onAppTerminate();
}

- (QRect)getWindowLocation {
    if (self.hearthstoneWindowRef == nullptr) {
        return QRect();
    }

    CFTypeRef positionValue = nullptr;
    CFTypeRef sizeValue = nullptr;

    if (AXUIElementCopyAttributeValue(self.hearthstoneWindowRef, kAXPositionAttribute, &positionValue) !=
        kAXErrorSuccess) {
        return QRect();
    }
    if (AXUIElementCopyAttributeValue(self.hearthstoneWindowRef, kAXSizeAttribute, &sizeValue) != kAXErrorSuccess) {
        CFRelease(positionValue);
        return QRect();
    }

    QRect result;
    CGPoint position;
    CGSize size;
    if (AXValueGetType((AXValueRef)positionValue) == kAXValueTypeCGPoint &&
        AXValueGetType((AXValueRef)sizeValue) == kAXValueTypeCGSize) {
        AXValueGetValue((AXValueRef)positionValue, kAXValueTypeCGPoint, &position);
        AXValueGetValue((AXValueRef)sizeValue, kAXValueTypeCGSize, &size);
        result = QRect((int)position.x, (int)position.y, (int)size.width, (int)size.height);
    }

    CFRelease(positionValue);
    CFRelease(sizeValue);
    return result;
}

- (void)startRetrySetupListener:(pid_t)pid {
    self.pid = pid;
    self.retryCount = 0;
    [self retrySetupListener];
}

- (void)retrySetupListener {
    NSRunningApplication *currentApp = [NSRunningApplication runningApplicationWithProcessIdentifier:self.pid];
    if (currentApp == nil) {
        SPDLOG_LOGGER_WARN(_logger, "Hearthstone process is gone, stop retry");
        self.retryCount = 30;
        return;
    }
    [self setupListener:currentApp];
    QRect rect = [self getWindowLocation];
    if (!rect.isNull()) {
        SPDLOG_LOGGER_INFO(_logger, "found Hearthstone window at retry#{}", self.retryCount);
        self.retryCount = 30;
        emit self.listener->onAppLaunch(rect);
        return;
    }

    if (self.retryCount < 30) {
        self.retryCount++;
        SPDLOG_LOGGER_WARN(_logger, "Hearthstone window not found, retry for #{}", self.retryCount);
        QTimer::singleShot(500, [self]() { [self retrySetupListener]; });
        return;
    }
    SPDLOG_LOGGER_WARN(_logger, "Hearthstone window not found, stop retry#{}", self.retryCount);
    self.retryCount = 30;
}

- (void)setupListener:(NSRunningApplication *)app {
    if (self.hearthstoneWindowRef != nullptr) {
        return;
    }
    self.pid = app.processIdentifier;
    AXUIElementRef appRef = AXUIElementCreateApplication(self.pid);
    CFArrayRef windows = nullptr;
    AXUIElementCopyAttributeValue(appRef, kAXWindowsAttribute, (CFTypeRef *)&windows);
    CFRelease(appRef);
    if (windows == nullptr) {
        SPDLOG_LOGGER_WARN(_logger, "Hearthstone has no window, will retry");
        return;
    }
    if (CFArrayGetCount(windows) == 0) {
        CFRelease(windows);
        SPDLOG_LOGGER_WARN(_logger, "Hearthstone has zero window, will retry");
        return;
    }
    self.hearthstoneWindowRef = (AXUIElementRef)CFRetain(CFArrayGetValueAtIndex(windows, 0));
    CFRelease(windows);

    AXObserverCreate(self.pid, windowPositionUpdateCallback, &self->_windowObserver);
    AXObserverAddNotification(self.windowObserver, self.hearthstoneWindowRef, kAXMovedNotification,
                              (__bridge void *)self);
    AXObserverAddNotification(self.windowObserver, self.hearthstoneWindowRef, kAXResizedNotification,
                              (__bridge void *)self);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(self.windowObserver), kCFRunLoopDefaultMode);
    SPDLOG_LOGGER_INFO(_logger, "setup Hearthstone window listener");
}

- (void)closeListener {
    if (self.windowObserver) {
        AXObserverRemoveNotification(self.windowObserver, self.hearthstoneWindowRef, kAXMovedNotification);
        AXObserverRemoveNotification(self.windowObserver, self.hearthstoneWindowRef, kAXResizedNotification);
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), AXObserverGetRunLoopSource(self.windowObserver),
                              kCFRunLoopDefaultMode);
        CFRelease(self.windowObserver);
        self.windowObserver = nullptr;
    }
}
@end

void setWindowStayOnTop(QWidget *widget) {
    auto view = (NSView *)widget->winId();
    NSWindow *window = view.window;
    window.level = NSFloatingWindowLevel;
    window.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
}

HearthStoneWindowListener::HearthStoneWindowListener(QObject *parent) : QObject(parent) {
    static AppLaunchObserver *observer = [[AppLaunchObserver alloc] initWithListener:this];
}
