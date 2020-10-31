// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#include "DisplayDetection.h"
#include "Log.h"
#include "Model/DirectDisplayManager.h"
#include "Model/GetOutputDevice.h"

namespace {
using namespace metaview;

class DisplayDetection : public IDisplayDetection {
  public:
    ~DisplayDetection() override = default;
    uint64_t enumerateDisplays(
        DirectDisplayManager& directDisplayManager) override;
    uint64_t getLuid() const override { return luid_; }
    InitErrors getStatus() const override { return status_; }
    Display* getDisplay() override { return disp_.get(); }

  private:
    InitErrors status_ = InitErrors::NotInitialized;
    std::unique_ptr<Display> disp_;
    uint64_t luid_{0};
};

uint64_t DisplayDetection::enumerateDisplays(
    DirectDisplayManager& directDisplayManager) {
    DEBUGLOG("Enumerating displays...");
    auto displays = directDisplayManager.getDirectMetaViewDisplays();
    if (displays.size() == 1) {
        DEBUGLOG("Found exactly one display we can use, great! "
                 << winrt::to_string(displays.front().getDisplayName()));
        // keep a copy of this display
        disp_ = std::make_unique<Display>(displays.front());
        luid_ = displays.front().getAdapter();
        status_ = InitErrors::None;
        return luid_;
    }
    // if we get here, we failed...
    luid_ = getDefaultLuid();
    if (displays.empty()) {
        status_ = InitErrors::NoDevice;
        DEBUGLOG(
            "Found no displays we can use. List of all displays found, none of "
            "which are useful:");
        auto allDisplays = directDisplayManager.getAllDisplays();
        for (auto const& d : allDisplays) {
            DEBUGLOG("- " << winrt::to_string(d.getDisplayName()));
        }
    } else {
        status_ = InitErrors::TooManyDevices;
        DEBUGLOG("Found too many usable displays?");
        for (auto const& d : displays) {
            DEBUGLOG("- " << winrt::to_string(d.getDisplayName()));
        }
    }

    return luid_;
}
}  // namespace
namespace metaview {
std::unique_ptr<IDisplayDetection> IDisplayDetection::create() {
    return std::make_unique<DisplayDetection>();
}
}  // namespace metaview
