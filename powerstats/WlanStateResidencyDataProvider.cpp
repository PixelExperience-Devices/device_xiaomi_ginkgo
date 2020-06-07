/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "libpixelpowerstats"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <pixelpowerstats/PowerStatsUtils.h>
#include <pixelpowerstats/WlanStateResidencyDataProvider.h>
#include <cstdio>
#include <cstring>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

const uint32_t ACTIVE_ID = 0;
const uint32_t DEEPSLEEP_ID = 1;

WlanStateResidencyDataProvider::WlanStateResidencyDataProvider(uint32_t id, std::string path)
    : mPath(std::move(path)), mPowerEntityId(id) {}

bool WlanStateResidencyDataProvider::getResults(
        std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    PowerEntityStateResidencyResult result = {
        .powerEntityId = mPowerEntityId,
        .stateResidencyData = {{.powerEntityStateId = ACTIVE_ID},
                               {.powerEntityStateId = DEEPSLEEP_ID}}};

    std::string wlanDriverStatus = android::base::GetProperty("wlan.driver.status", "unloaded");
    if (wlanDriverStatus != "ok") {
        LOG(ERROR) << __func__ << ": wlan is " << wlanDriverStatus;
        // Return 0s for Wlan stats, because the driver is unloaded
        results.insert(std::make_pair(mPowerEntityId, result));
        return true;
    }

    // Using FILE* instead of std::ifstream for performance reasons (b/122253123)
    std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(mPath.c_str(), "r"), fclose);
    if (!fp) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << mPath;
        return false;
    }
    size_t numFieldsRead = 0;
    const size_t numFields = 4;
    size_t len = 0;
    char *line = nullptr;

    while ((numFieldsRead < numFields) && (getline(&line, &len, fp.get()) != -1)) {
        uint64_t stat = 0;
        if (utils::extractStat(line, "cumulative_sleep_time_ms:", stat)) {
            result.stateResidencyData[1].totalTimeInStateMs = stat;
            ++numFieldsRead;
        } else if (utils::extractStat(line, "cumulative_total_on_time_ms:", stat)) {
            result.stateResidencyData[0].totalTimeInStateMs = stat;
            ++numFieldsRead;
        } else if (utils::extractStat(line, "deep_sleep_enter_counter:", stat)) {
            result.stateResidencyData[0].totalStateEntryCount = stat;
            result.stateResidencyData[1].totalStateEntryCount = stat;
            ++numFieldsRead;
        } else if (utils::extractStat(line, "last_deep_sleep_enter_tstamp_ms:", stat)) {
            result.stateResidencyData[1].lastEntryTimestampMs = stat;
            ++numFieldsRead;
        }
    }

    free(line);

    // End of file was reached and not all state data was parsed. Something
    // went wrong
    if (numFieldsRead != numFields) {
        LOG(ERROR) << __func__ << ": failed to parse stats for wlan";
        return false;
    }

    results.insert(std::make_pair(mPowerEntityId, result));
    return true;
}

std::vector<PowerEntityStateSpace> WlanStateResidencyDataProvider::getStateSpaces() {
    return {
        {.powerEntityId = mPowerEntityId,
         .states = {{.powerEntityStateId = ACTIVE_ID, .powerEntityStateName = "Active"},
                    {.powerEntityStateId = DEEPSLEEP_ID, .powerEntityStateName = "Deep-Sleep"}}}};
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
