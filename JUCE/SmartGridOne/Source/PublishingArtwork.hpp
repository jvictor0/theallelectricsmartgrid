#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

// Picker grants are the only image access. Normalization and thumbnail decode
// run on a utility queue; the safe UI callback receives bounded JPEG bytes.
//
struct PublishingArtwork
{
    struct Image
    {
        std::vector<unsigned char> m_bytes;
        juce::Image m_thumbnail;
        juce::String m_error;
    };

    using Callback = std::function<void(Image)>;
    explicit PublishingArtwork(Callback callback);
    ~PublishingArtwork();
    void ChooseFiles();
    void ChoosePhotos();
    static bool HasPhotos();

    struct Impl;
    std::shared_ptr<Impl> m_impl;
    std::unique_ptr<juce::FileChooser> m_chooser;
};
