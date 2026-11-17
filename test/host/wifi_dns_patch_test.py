# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

"""Exercise the actual original/patched Arduino DNS entry point."""
import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--cxx", default="c++")
parser.add_argument("--sdk")
args = parser.parse_args()
packages = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) / "packages"
framework = Path(os.environ.get("RADPRO_ARDUINO_FRAMEWORK_DIR", packages / "framework-arduinoespressif32"))
source = framework / "libraries/WiFi/src/WiFiGeneric.cpp"
original = source.read_text()
cmake = shutil.which("cmake") or str(packages / "tool-cmake/bin/cmake")

harness = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
struct IPAddress {
    uint32_t value = 0;
    bool fromString(const char *s) { if (s && std::strcmp(s,"192.0.2.1")==0) {value=42;return true;}return false; }
    IPAddress &operator=(uint32_t v) {value=v;return *this;}
    operator uint32_t() const {return value;}
};
struct ip_addr_t { struct {struct {uint32_t addr;} ip4;} u_addr; };
using err_t = int;
constexpr int ERR_OK=0, ERR_INPROGRESS=-5, WIFI_DNS_IDLE_BIT=1, WIFI_DNS_DONE_BIT=2;
int _arduino_event_group=0, rawCalls=0, resolverCalls=0;
void *lateArg=nullptr;
void (*lateCallback)(const char*,const ip_addr_t*,void*)=nullptr;
void xEventGroupSetBits(int,int) {}
void log_e(const char*,...) {}
int dns_gethostbyname(const char*,ip_addr_t*,decltype(lateCallback) callback,void *arg) {
 ++rawCalls;lateCallback=callback;lateArg=arg;return ERR_INPROGRESS;
}
extern "C" int radpro_resolve_hostname(const char *name,uint32_t *address) {
 ++resolverCalls;*address=std::strcmp(name,"success.example")==0 ? 123 : 0;return *address!=0;
}
struct WiFiGenericClass {
 int waitStatusBits(int,int){return 0;}
 void clearStatusBits(int){}
 void setStatusBits(int){}
 int hostByName(const char*,IPAddress&);
};
METHOD
int main() {
 WiFiGenericClass wifi;IPAddress address;
 assert(wifi.hostByName("192.0.2.1",address)==1 && uint32_t(address)==42);
 assert(rawCalls==0 && resolverCalls==0);
 assert(wifi.hostByName("slow.example",address)==0 && uint32_t(address)==0);
#if ORIGINAL
 // Original API keeps the caller's pointer after returning a timeout. A
 // delayed network callback can write into storage whose lifetime has ended.
 assert(rawCalls==1 && resolverCalls==0 && lateArg==&address);
 ip_addr_t delayed{};delayed.u_addr.ip4.addr=999;
 lateCallback("slow.example",&delayed,lateArg);
 assert(uint32_t(address)==999);
#else
 assert(rawCalls==0 && lateCallback==nullptr && resolverCalls==1);
 assert(wifi.hostByName("success.example",address)==1 && uint32_t(address)==123);
 assert(wifi.hostByName(nullptr,address)==0 && uint32_t(address)==0);
#endif
}
'''

with tempfile.TemporaryDirectory(prefix="radpro-dns-") as temporary:
    work = Path(temporary)
    fixture = work / "libraries/WiFi/src/WiFiGeneric.cpp"
    fixture.parent.mkdir(parents=True)
    fixture.write_text(original)
    (work / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(dns_patch LANGUAGES CXX)
add_library(arduino_under_test STATIC "{fixture}")
function(idf_component_get_property output component property)
 set(${{output}} arduino_under_test PARENT_SCOPE)
endfunction()
include("{ROOT / 'cmake/arduino_dns.cmake'}")
get_target_property(selected arduino_under_test SOURCES)
file(WRITE "${{CMAKE_BINARY_DIR}}/selected.txt" "${{selected}}")
''')
    command = [cmake, "-S", str(work), "-B", str(work / "build")]
    subprocess.run(command, check=True, capture_output=True, text=True)
    patched_file = work / "build/framework-patches/WiFiGeneric.cpp"
    assert (work / "build/selected.txt").read_text() == str(patched_file)
    patched = patched_file.read_text()
    assert "dns_gethostbyname(" not in patched
    assert "wifi_dns_found_callback" not in patched
    for label, text in (("original", original), ("patched", patched)):
        start = text.index("static void wifi_dns_found_callback(") if label == "original" else text.index('extern "C" int radpro_resolve_hostname')
        method = text[start:text.index("IPAddress WiFiGenericClass::calculateNetworkID")]
        cpp = work / f"{label}.cpp"
        cpp.write_text(harness.replace("METHOD", method))
        binary = work / label
        compile_command = [args.cxx, "-std=c++17", f"-DORIGINAL={int(label == 'original')}"]
        if args.sdk:
            compile_command += ["-isysroot", args.sdk]
        subprocess.run(compile_command + [str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

    class Env:
        def PioPlatform(self): return self
        def get_package_dir(self, package): return str(framework)
        def subst(self, value): return str(work / "build")
        def File(self, path): return Path(path)
        def AddBuildMiddleware(self, callback, pattern): self.callback = callback
    class Node:
        def srcnode(self): return self
        def get_abspath(self): return str(source)
    env = Env()
    runpy.run_path(str(ROOT / "tools/arduino_wifi_client.py"),
                   init_globals={"env":env,"Import":lambda name:None})
    assert env.callback(env, Node()) == patched_file
print("Arduino DNS patch and expired-caller regression tests passed")
