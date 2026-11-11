#import <UIKit/UIKit.h>
#include "BuildInfo.hpp"
#include "Cases.hpp"
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <sys/utsname.h>
#include <thread>

@interface PerfDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@property(nonatomic, strong) NSMutableDictionary* report;
@property(nonatomic, strong) NSMutableArray* telemetry;
@property(nonatomic, strong) NSString* reportPath;
@property(nonatomic) BOOL finished;
@end

@implementation PerfDelegate

- (void)recordTelemetry:(NSString*)event
{
    NSProcessInfo* process = NSProcessInfo.processInfo;
    UIDevice* device = UIDevice.currentDevice;
    [self.telemetry addObject:@{
        @"event": event,
        @"unix_seconds": @(NSDate.date.timeIntervalSince1970),
        @"thermal_state": @(process.thermalState),
        @"low_power": @(process.lowPowerModeEnabled),
        @"battery_level": @(device.batteryLevel),
        @"battery_state": @(device.batteryState)
    }];
}

- (void)finish:(NSString*)status error:(NSString*)error output:(NSString*)output
{
    if (self.finished)
    {
        return;
    }

    self.finished = YES;
    [self recordTelemetry:@"finished"];
    NSMutableArray* rows = [NSMutableArray array];
    for (NSString* line in [output componentsSeparatedByString:@"\n"])
    {
        if (line.length == 0)
        {
            continue;
        }

        id row = [NSJSONSerialization JSONObjectWithData:[line dataUsingEncoding:NSUTF8StringEncoding]
            options:0 error:nil];
        if (![row isKindOfClass:NSDictionary.class])
        {
            status = @"failed";
            error = @"Case emitted a result that is not a JSON object";
            break;
        }

        [rows addObject:row];
    }

    self.report[@"status"] = status;
    self.report[@"error"] = error;
    self.report[@"results"] = rows;
    self.report[@"telemetry"] = self.telemetry;
    self.report[@"finished_at"] = @(NSDate.date.timeIntervalSince1970);
    NSError* failure = nil;
    NSData* data = [NSJSONSerialization dataWithJSONObject:self.report options:0 error:&failure];
    BOOL saved = data != nil && [data writeToFile:self.reportPath options:NSDataWritingAtomic error:&failure];
    if (!saved)
    {
        NSLog(@"Cannot save benchmark report: %@", failure);
    }

    // This development-only runner deliberately exits after saving, including
    // on failure or timeout. The timeout also bounds a disconnected run.
    //
    std::exit(saved ? 0 : 1);
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options
{
    application.idleTimerDisabled = YES;
    UIDevice.currentDevice.batteryMonitoringEnabled = YES;
    self.telemetry = [NSMutableArray array];
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController* controller = [UIViewController new];
    UILabel* label = [[UILabel alloc] initWithFrame:self.window.bounds];
    label.text = @"SmartGrid Perf";
    label.textAlignment = NSTextAlignmentCenter;
    label.backgroundColor = UIColor.systemBackgroundColor;
    controller.view = label;
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];

    NSArray<NSString*>* arguments = NSProcessInfo.processInfo.arguments;
    NSUInteger index = [arguments indexOfObject:@"--request"];
    NSDictionary* request = nil;
    if (index != NSNotFound && index + 1 < arguments.count)
    {
        request = [NSJSONSerialization JSONObjectWithData:[arguments[index + 1] dataUsingEncoding:NSUTF8StringEncoding]
            options:0 error:nil];
    }

    NSString* runID = [request isKindOfClass:NSDictionary.class] ? request[@"run_id"] : nil;
    if (![runID isKindOfClass:NSString.class] || [[NSUUID alloc] initWithUUIDString:runID] == nil)
    {
        std::exit(1);
    }

    NSString* directory = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject
        stringByAppendingPathComponent:@"perf"];
    [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    self.reportPath = [directory stringByAppendingPathComponent:[runID stringByAppendingString:@".json"]];
    self.report = [request mutableCopy];
    self.report[@"git_commit"] = @(x_perfCommit);
    self.report[@"git_dirty"] = @(x_perfDirty);
    self.report[@"compiler"] = @(__clang_version__);
    self.report[@"build"] = @"Release; -O3 -DNDEBUG -funroll-loops; LTO";
    self.report[@"started_at"] = @(NSDate.date.timeIntervalSince1970);
    struct utsname hardware;
    uname(&hardware);
    self.report[@"device"] = @{
        @"model": @(hardware.machine),
        @"os": UIDevice.currentDevice.systemVersion,
        @"physical_memory_bytes": @(NSProcessInfo.processInfo.physicalMemory)
    };
    [self recordTelemetry:@"started"];
    [NSNotificationCenter.defaultCenter addObserverForName:NSProcessInfoThermalStateDidChangeNotification
        object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*)
    {
        [self recordTelemetry:@"thermal_changed"];
    }];

    double timeout = [request[@"timeout"] doubleValue];
    if (timeout <= 0 || timeout > 86400)
    {
        [self finish:@"failed" error:@"Invalid timeout" output:@""];
        return YES;
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeout * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^
    {
        [self finish:@"timeout" error:@"Case exceeded its timeout" output:@""];
    });

    NSString* name = request[@"case"];
    const BenchmarkCase* selected = nullptr;
    for (const auto& test : BenchmarkCases())
    {
        if ([name isEqualToString:@(test.m_name)])
        {
            selected = &test;
            break;
        }
    }

    if (selected == nullptr)
    {
        [self finish:@"failed" error:@"Unknown benchmark case" output:@""];
        return YES;
    }

    self.report[@"case_notes"] = @(selected->m_notes);
    label.text = [NSString stringWithFormat:@"Running %@", name];
    std::vector<std::string> words;
    for (NSString* word in request[@"arguments"])
    {
        words.emplace_back(word.UTF8String);
    }

    std::thread([self, selected, words]
    {
        @autoreleasepool
        {
            std::ostringstream output;
            std::string error;
            try
            {
                selected->m_run(words, output);
            }
            catch (const std::exception& failure)
            {
                error = failure.what();
            }
            catch (...)
            {
                error = "Unknown C++ exception";
            }

            NSString* result = @(output.str().c_str());
            NSString* failure = @(error.c_str());
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [self finish:failure.length == 0 ? @"completed" : @"failed" error:failure output:result];
            });
        }
    }).detach();
    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    [self recordTelemetry:@"resigned_active"];
}

@end

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(PerfDelegate.class));
    }
}
