// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
#pragma once

#include "Model/InitErrors.h"

#include <stdint.h>
#include <memory>

namespace metaview {
class DirectDisplayManager;
class Display;

class IDisplayDetection {
  public:
    static std::unique_ptr<IDisplayDetection> create();
    virtual ~IDisplayDetection() = default;

    /**
     * @brief Enumerate connected direct mode displays
     *
     * @param directDisplayManager the DirectDisplayManager instance.
     *
     * @return uint64_t of the adapter LUID for the connected display, if any.
     */
    virtual uint64_t enumerateDisplays(
        DirectDisplayManager& directDisplayManager) = 0;

    /**
     * @brief Get the LUID from the previous enumerateDisplays() call.
     *
     * @pre enumerateDisplays() has been called successfully.
     *
     * @return uint64_t of the adapter LUID for the connected display, if any.
     */
    virtual uint64_t getLuid() const = 0;

    /**
     * @brief Get the status/error codes so far.
     *
     * @return InitErrors
     */
    virtual InitErrors getStatus() const = 0;

    /**
     * @brief Get chosen HMD display
     *
     * May be null if we haven't found one.
     */
    virtual Display* getDisplay() = 0;

  protected:
    IDisplayDetection() = default;
};
}  // namespace metaview
