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

#include <pixelpowerstats/PowerStatsUtils.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {
namespace utils {

bool extractStat(const char *line, const std::string &prefix, uint64_t &stat) {
    char const *prefixStart = strstr(line, prefix.c_str());
    if (prefixStart == nullptr) {
        // Did not find the given prefix
        return false;
    }

    stat = strtoull(prefixStart + prefix.length(), nullptr, 0);
    return true;
}

}  // namespace utils
}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android