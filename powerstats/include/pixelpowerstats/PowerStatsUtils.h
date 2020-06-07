#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_POWERSTATSUTILS_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_POWERSTATSUTILS_H

#include <cstdint>
#include <string>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {
namespace utils {

bool extractStat(const char *line, const std::string &prefix, uint64_t &stat);

}  // namespace utils
}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_POWERSTATSUTILS_H