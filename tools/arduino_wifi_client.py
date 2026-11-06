# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

"""Use the CMake-patched core source for PlatformIO's separate WiFi library too."""

from pathlib import Path

Import("env")  # Provided by PlatformIO

framework_source = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32")) / "libraries/WiFi/src/WiFiClient.cpp"


def use_patched_wifi_client(build_env, node):
    if Path(node.srcnode().get_abspath()).resolve() != framework_source.resolve():
        return node
    patched = Path(build_env.subst("$BUILD_DIR")) / "framework-patches/WiFiClient.cpp"
    if not patched.is_file():
        raise RuntimeError("CMake did not generate the Arduino WiFiClient compatibility patch")
    return build_env.File(str(patched))


env.AddBuildMiddleware(use_patched_wifi_client, "*WiFiClient.cpp")
