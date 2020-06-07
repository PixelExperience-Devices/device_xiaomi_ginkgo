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

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pixelpowerstats/PowerStats.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include <inttypes.h>
#include <time.h>

namespace android {
namespace hardware {
namespace power {
namespace stats {
namespace V1_0 {
namespace implementation {

void PowerStats::setRailDataProvider(std::unique_ptr<IRailDataProvider> dataProvider) {
    mRailDataProvider = std::move(dataProvider);
}

Return<void> PowerStats::getRailInfo(getRailInfo_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->getRailInfo(_hidl_cb);
}

Return<void> PowerStats::getEnergyData(const hidl_vec<uint32_t> &railIndices,
                                       getEnergyData_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->getEnergyData(railIndices, _hidl_cb);
}

Return<void> PowerStats::streamEnergyData(uint32_t timeMs, uint32_t samplingRate,
                                          streamEnergyData_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, 0, 0, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->streamEnergyData(timeMs, samplingRate, _hidl_cb);
}

uint32_t PowerStats::addPowerEntity(const std::string &name, PowerEntityType type) {
    uint32_t id = mPowerEntityInfos.size();
    mPowerEntityInfos.push_back({id, name, type});
    return id;
}

void PowerStats::addStateResidencyDataProvider(sp<IStateResidencyDataProvider> p) {
    std::vector<PowerEntityStateSpace> stateSpaces = p->getStateSpaces();
    for (auto stateSpace : stateSpaces) {
        mPowerEntityStateSpaces.emplace(stateSpace.powerEntityId, stateSpace);
        mStateResidencyDataProviders.emplace(stateSpace.powerEntityId, p);
    }
}

Return<void> PowerStats::getPowerEntityInfo(getPowerEntityInfo_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mPowerEntityInfos.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    _hidl_cb(mPowerEntityInfos, Status::SUCCESS);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateInfo(const hidl_vec<uint32_t> &powerEntityIds,
                                                 getPowerEntityStateInfo_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mPowerEntityStateSpaces.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    std::vector<PowerEntityStateSpace> stateSpaces;

    // If powerEntityIds is empty then return state space info for all entities
    if (powerEntityIds.size() == 0) {
        stateSpaces.reserve(mPowerEntityStateSpaces.size());
        for (auto i : mPowerEntityStateSpaces) {
            stateSpaces.emplace_back(i.second);
        }
        _hidl_cb(stateSpaces, Status::SUCCESS);
        return Void();
    }

    // Return state space information only for valid ids
    auto ret = Status::SUCCESS;
    stateSpaces.reserve(powerEntityIds.size());
    for (const uint32_t id : powerEntityIds) {
        auto stateSpace = mPowerEntityStateSpaces.find(id);
        if (stateSpace != mPowerEntityStateSpaces.end()) {
            stateSpaces.emplace_back(stateSpace->second);
        } else {
            ret = Status::INVALID_INPUT;
        }
    }

    _hidl_cb(stateSpaces, ret);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateResidencyData(
    const hidl_vec<uint32_t> &powerEntityIds, getPowerEntityStateResidencyData_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mStateResidencyDataProviders.empty() || mPowerEntityStateSpaces.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    // If powerEntityIds is empty then return data for all supported entities
    if (powerEntityIds.size() == 0) {
        std::vector<uint32_t> ids;
        for (auto stateSpace : mPowerEntityStateSpaces) {
            ids.emplace_back(stateSpace.first);
        }
        return getPowerEntityStateResidencyData(ids, _hidl_cb);
    }

    std::unordered_map<uint32_t, PowerEntityStateResidencyResult> stateResidencies;
    std::vector<PowerEntityStateResidencyResult> results;
    results.reserve(powerEntityIds.size());

    // return results for only the given powerEntityIds
    bool invalidInput = false;
    bool filesystemError = false;
    for (auto id : powerEntityIds) {
        auto dataProvider = mStateResidencyDataProviders.find(id);
        // skip if the given powerEntityId does not have an associated StateResidencyDataProvider
        if (dataProvider == mStateResidencyDataProviders.end()) {
            invalidInput = true;
            continue;
        }

        // get the results if we have not already done so.
        if (stateResidencies.find(id) == stateResidencies.end()) {
            if (!dataProvider->second->getResults(stateResidencies)) {
                LOG(INFO) << "trying getResults() again";
                if (!dataProvider->second->getResults(stateResidencies)) {
                    filesystemError = true;
                } else {
                    LOG(INFO) << "getResults() retry success";
                }
            }
        }

        // append results
        auto stateResidency = stateResidencies.find(id);
        if (stateResidency != stateResidencies.end()) {
            results.emplace_back(stateResidency->second);
        }
    }

    auto ret = Status::SUCCESS;
    if (filesystemError) {
        ret = Status::FILESYSTEM_ERROR;
    } else if (invalidInput) {
        ret = Status::INVALID_INPUT;
    }

    _hidl_cb(results, ret);
    return Void();
}

//
// Debugging utilities to support printing data via debug()
//

static uint64_t getTimeElapsedMs(const struct timespec &now, const struct timespec &then) {
    uint64_t thenMs = then.tv_sec * 1000 + (then.tv_nsec / 1000000);
    uint64_t nowMs = now.tv_sec * 1000 + (now.tv_nsec / 1000000);
    return (nowMs - thenMs);
}

static const char RESIDENCY_HEADER[] =
        "\n============= PowerStats HAL 1.0 state residencies ==============\n";
static const char RESIDENCY_FOOTER[] =
        "========== End of PowerStats HAL 1.0 state residencies ==========\n";

static bool DumpResidencyDataToFd(
        const std::unordered_map<uint32_t, std::string> &entityNames,
        const std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> stateNames,
        const hidl_vec<PowerEntityStateResidencyResult> &results, int fd) {
    std::ostringstream dumpStats;
    const char *headerFormat = "  %14s   %14s   %16s   %15s   %17s\n";
    const char *dataFormat =
            "  %14s   %14s   %13" PRIu64 " ms   %15" PRIu64 "   %14" PRIu64 " ms\n";

    dumpStats << RESIDENCY_HEADER;
    dumpStats << android::base::StringPrintf(headerFormat, "Entity", "State", "Total time",
                                             "Total entries", "Last entry tstamp");

    for (auto result : results) {
        for (auto stateResidency : result.stateResidencyData) {
            dumpStats << android::base::StringPrintf(
                    dataFormat, entityNames.at(result.powerEntityId).c_str(),
                    stateNames.at(result.powerEntityId)
                              .at(stateResidency.powerEntityStateId).c_str(),
                    stateResidency.totalTimeInStateMs, stateResidency.totalStateEntryCount,
                    stateResidency.lastEntryTimestampMs);
        }
    }

    dumpStats << RESIDENCY_FOOTER;

    return android::base::WriteStringToFd(dumpStats.str(), fd);
}

static bool DumpResidencyDataDiffToFd(
        const std::unordered_map<uint32_t, std::string> &entityNames,
        const std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> stateNames,
        uint64_t elapsedTimeMs, const hidl_vec<PowerEntityStateResidencyResult> &prevResults,
        const hidl_vec<PowerEntityStateResidencyResult> &results, int fd) {
    std::ostringstream dumpStats;
    const char *headerFormat = "  %14s   %14s   %16s (%14s)   %15s (%16s)   %17s (%14s)\n";
    const char *dataFormatWithDelta =
            "  %14s   %14s   %13" PRIu64 " ms (%14" PRId64 ")   %15" PRIu64 " (%16" PRId64 ")"
            "   %14" PRIu64 " ms (%14" PRId64 ")\n";
    const char *dataFormatWithoutDelta =
            "  %14s   %14s   %13" PRIu64 " ms (          none)   %15" PRIu64 " (            none)"
            "   %14" PRIu64 " ms (          none)\n";

    dumpStats << RESIDENCY_HEADER;
    dumpStats << "Elapsed time: "
              << (elapsedTimeMs == 0 ? "unknown" : std::to_string(elapsedTimeMs)) << " ms\n";

    dumpStats << android::base::StringPrintf(headerFormat, "Entity", "State", "Total time",
                                             "Delta   ", "Total entries", "Delta   ",
                                             "Last entry tstamp", "Delta ");

    // Process prevResults into a 2-tier lookup table for easy reference
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, PowerEntityStateResidencyData>>
            prevResultsMap;
    for (auto prevResult : prevResults) {
        prevResultsMap.emplace(prevResult.powerEntityId,
                               std::unordered_map<uint32_t, PowerEntityStateResidencyData>());
        for (auto stateResidency : prevResult.stateResidencyData) {
            prevResultsMap.at(prevResult.powerEntityId)
                    .emplace(stateResidency.powerEntityStateId, stateResidency);
        }
    }

    // Iterate over the new result data (one "result" per entity)
    for (auto result : results) {
        uint32_t entityId = result.powerEntityId;
        const char *entityName = entityNames.at(entityId).c_str();

        // Look up previous result data for the same entity
        auto prevEntityResultIt = prevResultsMap.find(entityId);

        // Iterate over individual states within the current entity's new result
        for (auto stateResidency : result.stateResidencyData) {
            uint32_t stateId = stateResidency.powerEntityStateId;
            const char *stateName = stateNames.at(entityId).at(stateId).c_str();

            // If a previous result was found for the same entity, see if that
            // result also contains data for the current state
            bool prevValueFound = false;
            if (prevEntityResultIt != prevResultsMap.end()) {
                auto prevStateResidencyIt = prevEntityResultIt->second.find(stateId);
                // If a previous result was found for the current entity and state, calculate the
                // deltas and display them along with new result
                if (prevStateResidencyIt != prevEntityResultIt->second.end()) {
                    int64_t deltaTotalTime = stateResidency.totalTimeInStateMs -
                                             prevStateResidencyIt->second.totalTimeInStateMs;
                    int64_t deltaTotalCount = stateResidency.totalStateEntryCount -
                                              prevStateResidencyIt->second.totalStateEntryCount;
                    int64_t deltaTimestamp = stateResidency.lastEntryTimestampMs -
                                             prevStateResidencyIt->second.lastEntryTimestampMs;

                    dumpStats << android::base::StringPrintf(
                            dataFormatWithDelta, entityName, stateName,
                            stateResidency.totalTimeInStateMs, deltaTotalTime,
                            stateResidency.totalStateEntryCount, deltaTotalCount,
                            stateResidency.lastEntryTimestampMs, deltaTimestamp);
                    prevValueFound = true;
                }
            }

            // If no previous result was found for the current entity and state, display the new
            // result without deltas
            if (!prevValueFound) {
                dumpStats << android::base::StringPrintf(
                        dataFormatWithoutDelta, entityName, stateName,
                        stateResidency.totalTimeInStateMs, stateResidency.totalStateEntryCount,
                        stateResidency.lastEntryTimestampMs);
            }
        }
    }

    dumpStats << RESIDENCY_FOOTER;

    return android::base::WriteStringToFd(dumpStats.str(), fd);
}

void PowerStats::debugStateResidency(const std::unordered_map<uint32_t, std::string> &entityNames,
                                     int fd, bool delta) {
    static struct timespec prevDataTime;
    static bool prevDataTimeValid = false;
    struct timespec dataTime;
    bool dataTimeValid;

    // Get power entity state space information
    Status status;
    hidl_vec<PowerEntityStateSpace> stateSpaces;
    getPowerEntityStateInfo({}, [&status, &stateSpaces](auto rStateSpaces, auto rStatus) {
        status = rStatus;
        stateSpaces = rStateSpaces;
    });
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Error getting state info";
        return;
    }

    // Construct lookup table of powerEntityId, powerEntityStateId to state name
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> stateNames;
    for (auto stateSpace : stateSpaces) {
        stateNames.emplace(stateSpace.powerEntityId, std::unordered_map<uint32_t, std::string>());
        auto &entityStateNames = stateNames.at(stateSpace.powerEntityId);
        for (auto state : stateSpace.states) {
            entityStateNames.emplace(state.powerEntityStateId, state.powerEntityStateName);
        }
    }

    // Get power entity state residency data
    hidl_vec<PowerEntityStateResidencyResult> results;
    getPowerEntityStateResidencyData(
            {}, [&status, &results, &dataTime, &dataTimeValid](auto rResults, auto rStatus) {
                status = rStatus;
                results = rResults;
                dataTimeValid = (clock_gettime(CLOCK_BOOTTIME, &dataTime) == 0);
            });

    // This implementation of getPowerEntityStateResidencyData supports the
    // return of partial results if status == FILESYSTEM_ERROR.
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Error getting residency data -- Some results missing";
    }

    if (!delta) {
        // If no delta argument was supplied, just dump the latest data
        if (!DumpResidencyDataToFd(entityNames, stateNames, results, fd)) {
            PLOG(ERROR) << "Failed to dump residency data to fd";
        }
    } else {
        // If the delta argument was supplied, calculate the elapsed time since the previous
        // result and then dump the latest data along with elapsed time and deltas
        static hidl_vec<PowerEntityStateResidencyResult> prevResults;
        uint64_t elapsedTimeMs = 0;
        if (dataTimeValid && prevDataTimeValid) {
            elapsedTimeMs = getTimeElapsedMs(dataTime, prevDataTime);
        }

        if (!DumpResidencyDataDiffToFd(entityNames, stateNames, elapsedTimeMs, prevResults,
                                       results, fd)) {
            PLOG(ERROR) << "Failed to dump residency data delta to fd";
        }

        prevResults = results;
        prevDataTime = dataTime;
        prevDataTimeValid = dataTimeValid;
    }
}

static const char ENERGYDATA_HEADER[] =
        "\n============= PowerStats HAL 1.0 rail energy data ==============\n";
static const char ENERGYDATA_FOOTER[] =
        "========== End of PowerStats HAL 1.0 rail energy data ==========\n";

static bool DumpEnergyDataToFd(
        const std::unordered_map<uint32_t, std::pair<std::string, std::string>> &railNames,
        const hidl_vec<EnergyData> &energyData, int fd) {
    std::ostringstream dumpStats;
    const char *headerFormat = "  %14s   %18s   %18s\n";
    const char *dataFormat = "  %14s   %18s   %14.2f mWs\n";

    dumpStats << ENERGYDATA_HEADER;
    dumpStats << android::base::StringPrintf(headerFormat, "Subsys", "Rail", "Cumulative Energy");

    for (auto data : energyData) {
        dumpStats << android::base::StringPrintf(dataFormat, railNames.at(data.index).first.c_str(),
                                                 railNames.at(data.index).second.c_str(),
                                                 static_cast<float>(data.energy) / 1000.0);
    }

    dumpStats << ENERGYDATA_FOOTER;

    return android::base::WriteStringToFd(dumpStats.str(), fd);
}

static bool DumpEnergyDataDiffToFd(
        const std::unordered_map<uint32_t, std::pair<std::string, std::string>> &railNames,
        uint64_t elapsedTimeMs, const hidl_vec<EnergyData> &prevEnergyData,
        const hidl_vec<EnergyData> &energyData, int fd) {
    std::ostringstream dumpStats;
    const char *headerFormat = "  %14s   %18s   %18s (%14s)\n";
    const char *dataFormatWithDelta = "  %14s   %18s   %14.2f mWs (%14.2f)\n";
    const char *dataFormatWithoutDelta = "  %14s   %18s   %14.2f mWs (          none)\n";

    dumpStats << ENERGYDATA_HEADER;
    dumpStats << "Elapsed time: "
              << (elapsedTimeMs == 0 ? "unknown" : std::to_string(elapsedTimeMs)) << " ms\n";

    dumpStats << android::base::StringPrintf(headerFormat, "Subsys", "Rail", "Cumulative Energy",
                                             "Delta   ");

    std::unordered_map<uint32_t, uint64_t> prevEnergyDataMap;
    for (auto data : prevEnergyData) {
        prevEnergyDataMap.emplace(data.index, data.energy);
    }

    for (auto data : energyData) {
        const char *subsysName = railNames.at(data.index).first.c_str();
        const char *railName = railNames.at(data.index).second.c_str();

        auto prevEnergyDataIt = prevEnergyDataMap.find(data.index);

        if (prevEnergyDataIt != prevEnergyDataMap.end()) {
            int64_t deltaEnergy = data.energy - prevEnergyDataIt->second;

            dumpStats << android::base::StringPrintf(dataFormatWithDelta, subsysName, railName,
                                                     static_cast<float>(data.energy) / 1000.0,
                                                     static_cast<float>(deltaEnergy) / 1000.0);
        } else {
            dumpStats << android::base::StringPrintf(dataFormatWithoutDelta, subsysName, railName,
                                                     static_cast<float>(data.energy) / 1000.0);
        }
    }

    dumpStats << ENERGYDATA_FOOTER;

    return android::base::WriteStringToFd(dumpStats.str(), fd);
}

void PowerStats::debugEnergyData(int fd, bool delta) {
    static struct timespec prevDataTime;
    static bool prevDataTimeValid = false;
    struct timespec dataTime;
    bool dataTimeValid = false;

    std::unordered_map<uint32_t, std::pair<std::string, std::string>> railNames;
    getRailInfo([&railNames](auto infos, auto /* status */) {
        // Don't care about the status. infos will be nonempty if rail energy is supported.
        for (auto info : infos) {
            railNames.emplace(info.index, std::make_pair(info.subsysName, info.railName));
        }
    });
    if (railNames.empty()) {
        return;
    }

    Status status;
    hidl_vec<EnergyData> energyData;
    getEnergyData(
            {}, [&status, &energyData, &dataTime, &dataTimeValid](auto rEnergyData, auto rStatus) {
                status = rStatus;
                energyData = rEnergyData;
                dataTimeValid = (clock_gettime(CLOCK_BOOTTIME, &dataTime) == 0);
            });

    // getEnergyData returns no results if status != SUCCESS.
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Error getting rail data";
        return;
    }

    if (!delta) {
        if (!DumpEnergyDataToFd(railNames, energyData, fd)) {
            PLOG(ERROR) << "Failed to dump energy data to fd";
        }
    } else {
        // If the delta argument was supplied, calculate the elapsed time since the previous
        // result and then dump the latest data along with elapsed time and deltas
        static hidl_vec<EnergyData> prevEnergyData;
        uint64_t elapsedTimeMs = 0;
        if (dataTimeValid && prevDataTimeValid) {
            elapsedTimeMs = getTimeElapsedMs(dataTime, prevDataTime);
        }

        if (!DumpEnergyDataDiffToFd(railNames, elapsedTimeMs, prevEnergyData, energyData, fd)) {
            PLOG(ERROR) << "Failed to dump energy data delta to fd";
        }

        prevEnergyData = energyData;
        prevDataTime = dataTime;
        prevDataTimeValid = dataTimeValid;
    }
}

Return<void> PowerStats::debug(const hidl_handle &handle, const hidl_vec<hidl_string> &args) {
    if (handle == nullptr || handle->numFds < 1) {
        return Void();
    }

    int fd = handle->data[0];
    bool delta = (args.size() == 1) && (args[0] == "delta");

    // Get power entity information, which is common across all supported data categories
    Status status;
    hidl_vec<PowerEntityInfo> stateInfos;
    getPowerEntityInfo([&status, &stateInfos](auto rInfos, auto rStatus) {
        status = rStatus;
        stateInfos = rInfos;
    });
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Error getting power entity info";
        return Void();
    }

    // Construct lookup table of powerEntityId to name
    std::unordered_map<uint32_t, std::string> entityNames;
    for (auto info : stateInfos) {
        entityNames.emplace(info.powerEntityId, info.powerEntityName);
    }

    // Generate debug output for supported data categories
    debugStateResidency(entityNames, fd, delta);

    // Generate debug output for energy data
    debugEnergyData(fd, delta);

    fsync(fd);
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
