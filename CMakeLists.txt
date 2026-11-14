cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(radpro-wifi-bridge)

include(cmake/arduino_wifi_client.cmake)
include(cmake/arduino_http_headers.cmake)
include(cmake/esp_idf_usb_hub.cmake)

include(cmake/esp_idf_panic_capture.cmake)
