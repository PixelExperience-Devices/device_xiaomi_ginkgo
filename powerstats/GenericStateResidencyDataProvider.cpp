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
#include <android-base/strings.h>
#include <pixelpowerstats/GenericStateResidencyDataProvider.h>
#include <pixelpowerstats/PowerStatsUtils.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

std::vector<StateResidencyConfig> generateGenericStateResidencyConfigs(
    const StateResidencyConfig &stateConfig,
    const std::vector<std::pair<std::string, std::string>> &stateHeaders) {
    std::vector<StateResidencyConfig> stateResidencyConfigs;
    stateResidencyConfigs.reserve(stateHeaders.size());
    for (auto h : stateHeaders) {
        StateResidencyConfig cfg = {stateConfig};
        cfg.name = h.first;
        cfg.header = h.second;
        stateResidencyConfigs.emplace_back(cfg);
    }
    return stateResidencyConfigs;
}

PowerEntityConfig::PowerEntityConfig(const std::vector<StateResidencyConfig> &stateResidencyConfigs)
    : PowerEntityConfig("", stateResidencyConfigs) {}

PowerEntityConfig::PowerEntityConfig(const std::string &header,
                                     const std::vector<StateResidencyConfig> &stateResidencyConfigs)
    : mHeader(header) {
    mStateResidencyConfigs.reserve(stateResidencyConfigs.size());
    for (uint32_t i = 0; i < stateResidencyConfigs.size(); ++i) {
        mStateResidencyConfigs.emplace_back(i, stateResidencyConfigs[i]);
    }
}

static bool parseState(PowerEntityStateResidencyData &data, const StateResidencyConfig &config,
                       FILE *fp, char *&line, size_t &len) {
    size_t numFieldsRead = 0;
    const size_t numFields =
        config.entryCountSupported + config.totalTimeSupported + config.lastEntrySupported;

    while ((numFieldsRead < numFields) && (getline(&line, &len, fp) != -1)) {
        uint64_t stat = 0;
        // Attempt to extract data from the current line
        if (config.entryCountSupported && utils::extractStat(line, config.entryCountPrefix, stat)) {
            data.totalStateEntryCount =
                config.entryCountTransform ? config.entryCountTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.totalTimeSupported &&
                   utils::extractStat(line, config.totalTimePrefix, stat)) {
            data.totalTimeInStateMs =
                config.totalTimeTransform ? config.totalTimeTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.lastEntrySupported &&
                   utils::extractStat(line, config.lastEntryPrefix, stat)) {
            data.lastEntryTimestampMs =
                config.lastEntryTransform ? config.lastEntryTransform(stat) : stat;
            ++numFieldsRead;
        }
    }

    // End of file was reached and not all state data was parsed. Something
    // went wrong
    if (numFieldsRead != numFields) {
        LOG(ERROR) << __func__ << ": failed to parse stats for:" << config.name;
        return false;
    }

    return true;
}

template <class T, class Func>
static auto findNext(const std::vector<T> &collection, FILE *fp, char *&line, size_t &len,
                     Func pred) {
    // handling the case when there is no header to look for
    if (pred(collection.front(), "")) {
        return collection.cbegin();
    }

    while (getline(&line, &len, fp) != -1) {
        for (auto it = collection.cbegin(); it != collection.cend(); ++it) {
            if (pred(*it, line)) {
                return it;
            }
        }
    }

    return collection.cend();
}

static bool getStateData(
    PowerEntityStateResidencyResult &result,
    const std::vector<std::pair<uint32_t, StateResidencyConfig>> &stateResidencyConfigs, FILE *fp,
    char *&line, size_t &len) {
    size_t numStatesRead = 0;
    size_t numStates = stateResidencyConfigs.size();
    auto nextState = stateResidencyConfigs.cbegin();
    auto endState = stateResidencyConfigs.cend();
    auto pred = [](auto a, char const *b) {
        // return true if b matches the header contained in a, ignoring whitespace
        return (a.second.header == android::base::Trim(std::string(b)));
    };

    result.stateResidencyData.resize(numStates);

    // Search for state headers until we have found them all or can't find anymore
    while ((numStatesRead < numStates) &&
           (nextState = findNext<std::pair<uint32_t, StateResidencyConfig>>(
                stateResidencyConfigs, fp, line, len, pred)) != endState) {
        // Found a matching state header. Parse the contents
        PowerEntityStateResidencyData data = {.powerEntityStateId = nextState->first};
        if (parseState(data, nextState->second, fp, line, len)) {
            result.stateResidencyData[numStatesRead] = data;
            ++numStatesRead;
        } else {
            break;
        }
    }

    // There was a problem parsing and we failed to get data for all of the states
    if (numStatesRead != numStates) {
        return false;
    }

    return true;
}

bool GenericStateResidencyDataProvider::getResults(
    std::unordered_map<uint32_t, PowerEntityStateResidencyResult> &results) {
    // Using FILE* instead of std::ifstream for performance reasons (b/122253123)
    std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(mPath.c_str(), "r"), fclose);
    if (!fp) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << mPath
                    << " Error = " << strerror(errno);
        return false;
    }

    size_t len = 0;
    char *line = nullptr;
    size_t numEntitiesRead = 0;
    size_t numEntities = mPowerEntityConfigs.size();
    auto nextConfig = mPowerEntityConfigs.cbegin();
    auto endConfig = mPowerEntityConfigs.cend();
    auto pred = [](auto a, char const *b) {
        // return true if b matches the header contained in a, ignoring whitespace
        return (a.second.mHeader == android::base::Trim(std::string(b)));
    };

    // Search for entity headers until we have found them all or can't find anymore
    while ((numEntitiesRead < numEntities) &&
           (nextConfig = findNext<decltype(mPowerEntityConfigs)::value_type>(
                mPowerEntityConfigs, fp.get(), line, len, pred)) != endConfig) {
        // Found a matching header. Retrieve its state data
        PowerEntityStateResidencyResult result = {.powerEntityId = nextConfig->first};
        if (getStateData(result, nextConfig->second.mStateResidencyConfigs, fp.get(), line, len)) {
            results.emplace(nextConfig->first, result);
            ++numEntitiesRead;
        } else {
            break;
        }
    }

    free(line);

    // There was a problem gathering state residency data for one or more entities
    if (numEntitiesRead != numEntities) {
        LOG(ERROR) << __func__ << ":Failed to get results for " << mPath;
        return false;
    }

    return true;
}

void GenericStateResidencyDataProvider::addEntity(uint32_t id, const PowerEntityConfig &config) {
    mPowerEntityConfigs.emplace_back(id, config);
}

std::vector<PowerEntityStateSpace> GenericStateResidencyDataProvider::getStateSpaces() {
    std::vector<PowerEntityStateSpace> stateSpaces;
    stateSpaces.reserve(mPowerEntityConfigs.size());
    for (auto config : mPowerEntityConfigs) {
        PowerEntityStateSpace s = {.powerEntityId = config.first};
        s.states.resize(config.second.mStateResidencyConfigs.size());

        for (uint32_t i = 0; i < config.second.mStateResidencyConfigs.size(); ++i) {
            s.states[i] = {
                .powerEntityStateId = config.second.mStateResidencyConfigs[i].first,
                .powerEntityStateName = config.second.mStateResidencyConfigs[i].second.name};
        }
        stateSpaces.emplace_back(s);
    }
    return stateSpaces;
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
