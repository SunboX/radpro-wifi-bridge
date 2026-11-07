# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

"""Use CMake-patched sources for PlatformIO's separate networking libraries too."""

from pathlib import Path

Import("env")  # Provided by PlatformIO

framework_source = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32")) / "libraries/WiFi/src/WiFiClient.cpp"
http_parser_source = framework_source.parents[3] / "libraries/WebServer/src/Parsing.cpp"


def use_patched_wifi_client(build_env, node):
    source = Path(node.srcnode().get_abspath()).resolve()
    if source not in (framework_source.resolve(), http_parser_source.resolve()):
        return node
    patched = Path(build_env.subst("$BUILD_DIR")) / "framework-patches" / source.name
    if not patched.is_file():
        raise RuntimeError("CMake did not generate the Arduino WiFiClient compatibility patch")
    return build_env.File(str(patched))


env.AddBuildMiddleware(use_patched_wifi_client, "*WiFiClient.cpp")
env.AddBuildMiddleware(use_patched_wifi_client, "*WebServer/src/Parsing.cpp")
