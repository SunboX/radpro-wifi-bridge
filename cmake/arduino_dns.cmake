# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

# Arduino 2.0.17 calls the raw DNS API off tcpip_thread and passes a caller's
# stack IPAddress to a callback that can outlive its 15-second wait. Use the
# bridge's bounded socket-API worker instead. Do not modify the shared SDK.
idf_component_get_property(arduino_target framework-arduinoespressif32 COMPONENT_LIB)
get_target_property(arduino_sources ${arduino_target} SOURCES)
get_target_property(arduino_dir ${arduino_target} SOURCE_DIR)
set(dns_found FALSE)
foreach(source IN LISTS arduino_sources)
    get_filename_component(source_path "${source}" ABSOLUTE BASE_DIR "${arduino_dir}")
    if(source_path MATCHES "/libraries/WiFi/src/WiFiGeneric\\.cpp$")
        if(dns_found)
            message(FATAL_ERROR "Multiple Arduino DNS sources; review the compatibility patch")
        endif()
        set(dns_found TRUE)
        file(READ "${source_path}" original)
        string(FIND "${original}" "static void wifi_dns_found_callback(" begin)
        string(FIND "${original}" "IPAddress WiFiGenericClass::calculateNetworkID" end)
        if(begin EQUAL -1 OR end EQUAL -1 OR NOT end GREATER begin)
            message(FATAL_ERROR "Arduino DNS changed; review the compatibility patch")
        endif()
        string(SUBSTRING "${original}" 0 ${begin} prefix)
        string(SUBSTRING "${original}" ${end} -1 suffix)
        set(replacement [=[extern "C" int radpro_resolve_hostname(const char *, uint32_t *);

int WiFiGenericClass::hostByName(const char *hostname, IPAddress &result)
{
    result = static_cast<uint32_t>(0);
    if (!hostname || !hostname[0])
        return 0;
    if (result.fromString(hostname))
        return static_cast<uint32_t>(result) != 0;
    uint32_t address = 0;
    const int ok = radpro_resolve_hostname(hostname, &address);
    result = address;
    if (!ok)
        log_e("DNS Failed for %s", hostname);
    return ok;
}

]=])
        set(patched "${prefix}${replacement}${suffix}")
        set(destination "${CMAKE_BINARY_DIR}/framework-patches/WiFiGeneric.cpp")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/framework-patches")
        set(existing "")
        if(EXISTS "${destination}")
            file(READ "${destination}" existing)
        endif()
        if(NOT patched STREQUAL existing)
            file(WRITE "${destination}" "${patched}")
        endif()
        list(REMOVE_ITEM arduino_sources "${source}")
        list(APPEND arduino_sources "${destination}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_path}")
    endif()
endforeach()
if(NOT dns_found)
    message(FATAL_ERROR "Arduino DNS source missing; compatibility patch was not applied")
endif()
set_property(TARGET ${arduino_target} PROPERTY SOURCES "${arduino_sources}")
