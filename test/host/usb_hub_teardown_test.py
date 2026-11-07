# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later
"""Simulate a root-port interrupt during the actual pinned hub teardown method."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cxx', default='c++')
p.add_argument('--sdk')
args = p.parse_args()
packages = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))/'packages'
source = packages/'framework-espidf/components/usb/hub.c'
original = source.read_text()
cmake = shutil.which('cmake') or str(packages/'tool-cmake/bin/cmake')

def uninstall(text):
    start=text.index('esp_err_t hub_uninstall(void)'); end=text.index('\nesp_err_t hub_root_start', start)
    return text[start:end]

harness = r'''
#include <cassert>
#include <cstddef>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_STATE=1, HUB_DRIVER_STATE_INSTALLED=0;
#define HUB_DRIVER_ENTER_CRITICAL() ((void)0)
#define HUB_DRIVER_EXIT_CRITICAL() ((void)0)
#define HUB_DRIVER_CHECK_FROM_CRIT(c,e) do { if(!(c)) return e; } while(0)
#define ESP_ERROR_CHECK(c) do { int r=(c); if(r) return r; } while(0)
struct Hub { struct { int driver_state=0; } dynamic; struct { int root_port_hdl=1; } constant; struct { struct { int urb=1; } enum_ctrl; } single_thread; };
using hub_driver_t=Hub;
static Hub hub, *p_hub_driver_obj=&hub;
static bool interruptsDisabled=false, freed=false;
int hcd_port_deinit(int) {
    // An interrupt just before disable must still see valid hub state.
    if (!p_hub_driver_obj) return 7;
    assert(p_hub_driver_obj->constant.root_port_hdl==1);
    interruptsDisabled=true; return 0;
}
void urb_free(int) { assert(interruptsDisabled && !p_hub_driver_obj); }
void heap_caps_free(Hub *) { assert(interruptsDisabled); freed=true; }
METHOD
int main() {
    int result=hub_uninstall();
    if(result==7) return 7;
    assert(result==ESP_OK && freed && !p_hub_driver_obj);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='radpro-usb-hub-') as temp:
    work=Path(temp);fixture=work/'components/usb/hub.c';fixture.parent.mkdir(parents=True);fixture.write_text(original)
    (work/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.16)
project(usb_patch LANGUAGES CXX)
add_library(usb_under_test STATIC "{fixture}")
set_target_properties(usb_under_test PROPERTIES LINKER_LANGUAGE CXX)
function(idf_component_get_property output component property)
set(${{output}} usb_under_test PARENT_SCOPE)
endfunction()
include("{root/'cmake/esp_idf_usb_hub.cmake'}")
''')
    configure=[cmake,'-S',str(work),'-B',str(work/'build')]
    subprocess.run(configure,check=True,capture_output=True,text=True)
    patched=(work/'build/framework-patches/hub.c').read_text()
    for name,text,expected in [('original',original,7),('patched',patched,0)]:
        cpp=work/f'{name}.cpp';cpp.write_text(harness.replace('METHOD',uninstall(text)))
        command=[args.cxx,'-std=c++17']
        if args.sdk:command+=['-isysroot',args.sdk]
        binary=work/name;subprocess.run(command+[str(cpp),'-o',str(binary)],check=True)
        assert subprocess.run([str(binary)]).returncode==expected
    fixture.write_text(original.replace('hub_driver_t *hub_driver_obj = p_hub_driver_obj;','hub_driver_t *changed = p_hub_driver_obj;'))
    rejected=subprocess.run(configure,capture_output=True,text=True)
    assert rejected.returncode and 'review the interrupt teardown compatibility patch' in rejected.stderr
    assert source.read_text()==original
print('Original teardown interrupt race reproduced; patched hub remains valid until interrupts are disabled')
