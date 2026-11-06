# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

"""Exercise the build's patched Arduino method, including genuine socket errors."""

import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx", default="c++")
parser.add_argument("--sdk")
args = parser.parse_args()
packages = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) / "packages"
framework = Path(os.environ.get("RADPRO_ARDUINO_FRAMEWORK_DIR", packages / "framework-arduinoespressif32"))
source = framework / "libraries/WiFi/src/WiFiClient.cpp"
original = source.read_text()
cmake = shutil.which("cmake") or str(packages / "tool-cmake/bin/cmake")

harness = r'''
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#define MSG_DONTWAIT 1
#define log_d(...) ((void)0)
#define log_i(...) ((void)0)
static int result = 0, error = 0, calls = 0;
static int recv(int fd, void *, size_t size, int flags) {
    if (fd != 54 || size != 0 || flags != MSG_DONTWAIT) return -1;
    ++calls;
    if (result < 0) errno = error;
    return result;
}
class WiFiClient {
    bool _connected = true;
public:
    int fd() { return 54; }
    uint8_t connected();
};
METHOD
int main() {
    // Successful probes must ignore an error left by a different socket.
    for (int stale : {0, ECONNRESET, ENOTCONN, EPIPE, ECONNREFUSED, ECONNABORTED}) {
        WiFiClient client; result = 0; errno = stale;
        if (!client.connected()) { std::puts("stale errno disconnected healthy socket"); return 1; }
    }
    // Real failures from this probe must still disconnect the socket.
    for (int fatal : {ECONNRESET, ENOTCONN, EPIPE, ECONNREFUSED, ECONNABORTED}) {
        WiFiClient client; result = -1; error = fatal; errno = 0;
        if (client.connected()) return 2;
        int previous = calls;
        if (client.connected() || calls != previous) return 3;
    }
    for (int retryable : {EWOULDBLOCK, ENOENT}) {
        WiFiClient client; result = -1; error = retryable; errno = ECONNRESET;
        if (!client.connected()) return 4;
    }
    return 0;
}
'''


def method(text):
    return text[text.index("uint8_t WiFiClient::connected()"):text.index("IPAddress WiFiClient::remoteIP")]


with tempfile.TemporaryDirectory(prefix="radpro-wifi-client-") as temporary:
    work = Path(temporary)
    fixture = work / "libraries/WiFi/src/WiFiClient.cpp"
    fixture.parent.mkdir(parents=True)
    fixture.write_text(original)
    (work / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(wifi_client_patch LANGUAGES CXX)
add_library(arduino_under_test STATIC "{fixture}")
function(idf_component_get_property output component property)
    set(${{output}} arduino_under_test PARENT_SCOPE)
endfunction()
include("{ROOT / 'cmake/arduino_wifi_client.cmake'}")
get_target_property(patched_sources arduino_under_test SOURCES)
file(WRITE "${{CMAKE_BINARY_DIR}}/selected-sources.txt" "${{patched_sources}}")
''')
    command = [cmake, "-S", str(work), "-B", str(work / "build")]
    subprocess.run(command, check=True, capture_output=True, text=True)
    patched_file = work / "build/framework-patches/WiFiClient.cpp"
    assert (work / "build/selected-sources.txt").read_text() == str(patched_file)
    patched = patched_file.read_text()
    assert patched == original.replace(
        "        // recv only sets errno if res is <= 0\n        if (res <= 0){",
        "        // errno is meaningful only when recv() fails.\n        if (res < 0){")
    # The mixed Arduino/ESP-IDF build also compiles a WiFi library via SCons.
    class BuildEnvironment:
        def PioPlatform(self): return self
        def get_package_dir(self, package): return str(framework)
        def subst(self, value): return str(work / "build")
        def File(self, path): return Path(path)
        def AddBuildMiddleware(self, callback, pattern): self.callback = callback

    class SourceNode:
        def __init__(self, path): self.path = path
        def srcnode(self): return self
        def get_abspath(self): return str(self.path)

    build_env = BuildEnvironment()
    runpy.run_path(str(ROOT / "tools/arduino_wifi_client.py"),
                   init_globals={"env": build_env, "Import": lambda name: None})
    assert build_env.callback(build_env, SourceNode(source)) == patched_file
    unrelated = SourceNode(work / "other/WiFiClient.cpp")
    assert build_env.callback(build_env, unrelated) is unrelated
    patched_file.unlink()
    try:
        build_env.callback(build_env, SourceNode(source))
        raise AssertionError("Missing patched source was silently accepted")
    except RuntimeError:
        pass
    patched_file.write_text(patched)
    for label, text, expected in [("original", original, 1), ("patched", patched, 0)]:
        cpp = work / f"{label}.cpp"
        cpp.write_text(harness.replace("METHOD", method(text)))
        compile_command = [args.cxx, "-std=c++17"]
        if args.sdk:
            compile_command += ["-isysroot", args.sdk]
        binary = work / label
        subprocess.run(compile_command + [str(cpp), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
    # A future framework change must stop the build until this patch is reviewed.
    fixture.write_text(original.replace("// recv only sets errno if res is <= 0", "// changed upstream"))
    rejected = subprocess.run(command, capture_output=True, text=True)
    assert rejected.returncode != 0 and "review the stale-errno compatibility patch" in rejected.stderr
    assert source.read_text() == original

print("Original stale-errno failure reproduced; patched method preserves healthy sockets and rejects real errors")
