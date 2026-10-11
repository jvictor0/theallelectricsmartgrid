#include "PublishingArtwork.hpp"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#endif
#include <atomic>

struct PublishingArtwork::Impl
{
    Callback m_callback;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_filesPending{false};
#if TARGET_OS_IOS
    UIViewController* m_picker = nil;
    id m_delegate = nil;
#endif

    static void Normalize(NSData* data, const std::shared_ptr<Impl>& state)
    {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^
        {
            @autoreleasepool
            {
                if (state->m_cancelled.load())
                {
                    return;
                }

                Image result;
                constexpr NSUInteger x_maxInput = 64 * 1024 * 1024;
                constexpr NSUInteger x_maxArtwork = 10 * 1024 * 1024;
                CGImageSourceRef source = data.length == 0 || data.length > x_maxInput ? nullptr
                    : CGImageSourceCreateWithData((__bridge CFDataRef)data, nullptr);
                CGImageRef image = source == nullptr ? nullptr : CGImageSourceCreateThumbnailAtIndex(source, 0,
                    (__bridge CFDictionaryRef)@{(__bridge NSString*)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
                        (__bridge NSString*)kCGImageSourceCreateThumbnailWithTransform: @YES,
                        (__bridge NSString*)kCGImageSourceThumbnailMaxPixelSize: @1600});
                if (source != nullptr)
                {
                    CFRelease(source);
                }

                NSMutableData* jpeg = [NSMutableData data];
                CGImageDestinationRef destination = image == nullptr ? nullptr : CGImageDestinationCreateWithData(
                    (__bridge CFMutableDataRef)jpeg, CFSTR("public.jpeg"), 1, nullptr);
                bool encoded = false;
                if (destination != nullptr)
                {
                    CGImageDestinationAddImage(destination, image,
                        (__bridge CFDictionaryRef)@{(__bridge NSString*)kCGImageDestinationLossyCompressionQuality: @0.85});
                    encoded = CGImageDestinationFinalize(destination);
                    CFRelease(destination);
                }

                if (image != nullptr)
                {
                    CGImageRelease(image);
                }

                if (!encoded || jpeg.length > x_maxArtwork)
                {
                    result.m_error = "Could not read image. Choose an image under 64 MiB.";
                }
                else
                {
                    const auto* bytes = static_cast<const unsigned char*>(jpeg.bytes);
                    result.m_bytes.assign(bytes, bytes + jpeg.length);
                    result.m_thumbnail = juce::ImageFileFormat::loadFrom(jpeg.bytes, jpeg.length).rescaled(96, 96);
                }

                juce::MessageManager::callAsync([state, result = std::move(result)]() mutable
                {
                    if (!state->m_cancelled.load())
                    {
                        state->m_callback(std::move(result));
                    }
                });
            }
        });
    }
};

#if TARGET_OS_IOS
@interface SGArtworkPickerDelegate : NSObject <PHPickerViewControllerDelegate>
{
@public
    std::shared_ptr<PublishingArtwork::Impl> m_state;
}
@end

@implementation SGArtworkPickerDelegate
- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results
{
    [picker dismissViewControllerAnimated:YES completion:nil];
    const auto state = m_state;
    if (results.count == 0 || state->m_cancelled.load())
    {
        return;
    }

    NSItemProvider* provider = results.firstObject.itemProvider;
    [provider loadDataRepresentationForTypeIdentifier:@"public.image" completionHandler:^(NSData* data, NSError* error)
    {
        PublishingArtwork::Impl::Normalize(data, state);
    }];
}
@end
#endif

PublishingArtwork::PublishingArtwork(Callback callback) : m_impl(std::make_shared<Impl>())
{
    m_impl->m_callback = std::move(callback);
}

PublishingArtwork::~PublishingArtwork()
{
    m_impl->m_cancelled.store(true);
#if TARGET_OS_IOS
    [m_impl->m_picker dismissViewControllerAnimated:NO completion:nil];
    m_impl->m_delegate = nil;
    m_impl->m_picker = nil;
#endif
}

void PublishingArtwork::ChooseFiles()
{
    if (m_impl->m_filesPending.exchange(true))
    {
        return;
    }

    m_chooser = std::make_unique<juce::FileChooser>("Choose SoundCloud artwork", juce::File(), "*.jpg;*.jpeg;*.png;*.heic", true);
    const auto state = m_impl;
    m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [state](const juce::FileChooser& chooser)
        {
            const auto url = chooser.getURLResult();
            if (!url.isEmpty())
            {
                // URL access retains the document picker grant while reading.
                //
                dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^
                {
                    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress);
                    auto stream = url.createInputStream(options);
                    juce::MemoryBlock data;
                    constexpr size_t x_maxInput = 64 * 1024 * 1024;
                    if (stream != nullptr)
                    {
                        stream->readIntoMemoryBlock(data, x_maxInput + 1);
                    }

                    NSData* native = data.getSize() > x_maxInput ? nil : [NSData dataWithBytes:data.getData() length:data.getSize()];
                    Impl::Normalize(native, state);
                });
            }

            state->m_filesPending.store(false);
        });
}

bool PublishingArtwork::HasPhotos()
{
#if TARGET_OS_IOS
    return true;
#else
    return false;
#endif
}

void PublishingArtwork::ChoosePhotos()
{
#if TARGET_OS_IOS
    UIViewController* controller = nil;
    for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
    {
        if (scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass:[UIWindowScene class]])
        {
            for (UIWindow* window in static_cast<UIWindowScene*>(scene).windows)
            {
                if (window.isKeyWindow)
                {
                    controller = window.rootViewController;
                }
            }
        }
    }

    while (controller.presentedViewController != nil)
    {
        controller = controller.presentedViewController;
    }

    if (controller == nil || m_impl->m_cancelled.load())
    {
        return;
    }

    PHPickerConfiguration* configuration = [[PHPickerConfiguration alloc] init];
    configuration.filter = [PHPickerFilter imagesFilter];
    configuration.selectionLimit = 1;
    PHPickerViewController* picker = [[PHPickerViewController alloc] initWithConfiguration:configuration];
    SGArtworkPickerDelegate* delegate = [[SGArtworkPickerDelegate alloc] init];
    delegate->m_state = m_impl;
    picker.delegate = delegate;
    m_impl->m_picker = picker;
    m_impl->m_delegate = delegate;
    [controller presentViewController:picker animated:YES completion:nil];
#endif
}
