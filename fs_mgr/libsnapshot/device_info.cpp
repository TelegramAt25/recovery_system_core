// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "device_info.h"

#include <android-base/logging.h>
#include <fs_mgr.h>
#include <fs_mgr_overlayfs.h>
#include <libfiemap/image_manager.h>

namespace android {
namespace snapshot {

#ifdef LIBSNAPSHOT_USE_HAL
using android::hardware::boot::V1_0::BoolResult;
using android::hardware::boot::V1_0::CommandResult;
#endif

using namespace std::chrono_literals;
using namespace std::string_literals;

constexpr bool kIsRecovery = true;

std::string DeviceInfo::GetMetadataDir() const {
    return "/metadata/ota"s;
}

std::string DeviceInfo::GetSlotSuffix() const {
    return fs_mgr_get_slot_suffix();
}

std::string DeviceInfo::GetOtherSlotSuffix() const {
    return fs_mgr_get_other_slot_suffix();
}

const android::fs_mgr::IPartitionOpener& DeviceInfo::GetPartitionOpener() const {
    return opener_;
}

std::string DeviceInfo::GetSuperDevice(uint32_t slot) const {
    return fs_mgr_get_super_partition_name(slot);
}

bool DeviceInfo::IsOverlayfsSetup() const {
    return fs_mgr_overlayfs_is_setup();
}

#ifdef LIBSNAPSHOT_USE_HAL
bool DeviceInfo::EnsureBootHal() {
    if (!boot_control_) {
        auto hal = android::hardware::boot::V1_0::IBootControl::getService();
        if (!hal) {
            LOG(ERROR) << "Could not find IBootControl HAL";
            return false;
        }
        boot_control_ = android::hardware::boot::V1_1::IBootControl::castFrom(hal);
        if (!boot_control_) {
            LOG(ERROR) << "Could not find IBootControl 1.1 HAL";
            return false;
        }
    }
    return true;
}
#endif

bool DeviceInfo::SetBootControlMergeStatus([[maybe_unused]] MergeStatus status) {
#ifdef LIBSNAPSHOT_USE_HAL
    if (!EnsureBootHal()) {
        return false;
    }
    if (!boot_control_->setSnapshotMergeStatus(status)) {
        LOG(ERROR) << "Unable to set the snapshot merge status";
        return false;
    }
    return true;
#else
    LOG(ERROR) << "HAL support not enabled.";
    return false;
#endif
}

bool DeviceInfo::IsRecovery() const {
    return kIsRecovery;
}

bool DeviceInfo::IsFirstStageInit() const {
    return first_stage_init_;
}

bool DeviceInfo::SetSlotAsUnbootable([[maybe_unused]] unsigned int slot) {
#ifdef LIBSNAPSHOT_USE_HAL
    if (!EnsureBootHal()) {
        return false;
    }

    CommandResult result = {};
    auto cb = [&](CommandResult r) -> void { result = r; };
    boot_control_->setSlotAsUnbootable(slot, cb);
    if (!result.success) {
        LOG(ERROR) << "Error setting slot " << slot << " unbootable: " << result.errMsg;
        return false;
    }
    return true;
#else
    LOG(ERROR) << "HAL support not enabled.";
    return false;
#endif
}

std::unique_ptr<android::fiemap::IImageManager> DeviceInfo::OpenImageManager() const {
    return IDeviceInfo::OpenImageManager("ota");
}

std::unique_ptr<android::fiemap::IImageManager> ISnapshotManager::IDeviceInfo::OpenImageManager(
        const std::string& gsid_dir) const {
    if (IsRecovery() || IsFirstStageInit()) {
        android::fiemap::ImageManager::DeviceInfo device_info = {
                .is_recovery = {IsRecovery()},
        };
        return android::fiemap::ImageManager::Open(gsid_dir, device_info);
    } else {
        // For now, use a preset timeout.
        return android::fiemap::IImageManager::Open(gsid_dir, 15000ms);
    }
}

}  // namespace snapshot
}  // namespace android
