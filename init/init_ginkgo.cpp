/*
 * Copyright (C) 2020 Paranoid Android
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

#include <cstdlib>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>

#include <android-base/properties.h>
#include "property_service.h"
#include "vendor_init.h"

using android::base::GetProperty;
using std::string;

void property_override(string prop, string value)
{
    auto pi = (prop_info*) __system_property_find(prop.c_str());

    if (pi != nullptr)
        __system_property_update(pi, value.c_str(), value.size());
    else
        __system_property_add(prop.c_str(), prop.size(), value.c_str(), value.size());
}

void vendor_load_properties()
{
    string device, model, desc;

    string region = GetProperty("ro.boot.hwc", "");
    string hwversion = GetProperty("ro.boot.hwversion", "");

    if (region == "Global_B" && (hwversion == "18.31.0" || 
        hwversion == "18.39.0" || hwversion == "19.39.0")) {
        device = "willow";
        model = "Redmi Note 8T";
        desc = "willow_eea-user 11 RKQ1.201004.002 V12.0.2.0.RCXEUXM release-keys";
    } else {
        device = "ginkgo";
        model = "Redmi Note 8";
        desc = "ginkgo-user 11 RKQ1.201004.002 V12.0.3.0.RCOCNXM release-keys";
    }

    // Override all partitions' props
    string prop_partitions[] = { "", "odm.", "product.", "system.",
					"system_ext.", "bootimage.", "vendor." };

    for (const string &prop : prop_partitions) {
        property_override(string("ro.product.") + prop + string("name"), device);
        property_override(string("ro.product.") + prop + string("device"), device);
        property_override(string("ro.product.") + prop + string("model"), model);
        property_override(string("ro.") + prop + string("build.product"), device);
        property_override(string("ro.") + prop + string("build.description"), desc);
    }

    // Set hardware revision
    property_override("ro.boot.hardware.revision", hwversion);

    // Set hardware SKU prop
    property_override("ro.boot.product.hardware.sku", device);

    // Set camera model for EXIF data
    property_override("persist.vendor.camera.model", model);

}
