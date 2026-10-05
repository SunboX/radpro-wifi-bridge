#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 André Fiedler
# SPDX-License-Identifier: GPL-3.0-or-later

"""Compile and run the standalone host/portal tests without flashing hardware."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTRA_SOURCES = {
    "bridge_led_policy_test": ["src/BridgeDiagnostics.cpp", "lib/AppSupport/Mqtt/MqttFaultPolicy.cpp"],
    "cooperative_pump_test": ["lib/AppSupport/Runtime/CooperativePump.cpp"],
    "device_manager_gc01_metadata_test": ["lib/DeviceManager/DeviceManager.cpp"],
    "device_info_prometheus_test": ["lib/AppSupport/DeviceInfo/DeviceInfoPage.cpp", "lib/AppSupport/DeviceInfo/DeviceInfoStore.cpp"],
    "led_controller_test": ["lib/AppSupport/Led/LedController.cpp"],
    "openradiation_settings_persistence_test": ["lib/AppSupport/AppConfig/AppConfig.cpp"],
    "safecast_settings_persistence_test": ["lib/AppSupport/AppConfig/AppConfig.cpp"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filter", default="", help="Run tests whose names contain this string")
    parser.add_argument("--cxx", default="c++", help="C++17 compiler")
    parser.add_argument("--node", default="node", help="Node.js executable")
    parser.add_argument("--sdk", help="Optional macOS SDK path for -isysroot")
    parser.add_argument("--promtool", help="Optional promtool path to validate exported metrics")
    args = parser.parse_args()
    output = ROOT / "build" / "host-tests"
    output.mkdir(parents=True, exist_ok=True)
    includes = [ROOT / path for path in (
        "test/host/include", "lib/AppSupport", "lib/DeviceManager", "src",
        "lib/AppSupport/DeviceInfo", "lib/UsbCdcHost",
        ".pio/libdeps/esp32-s3-devkitc-1/ArduinoJson/src")]
    tests = [path for path in sorted((ROOT / "test/host").glob("*_test.*"))
             if path.suffix in (".cpp", ".js") and args.filter in path.stem]
    if not tests:
        parser.error("No tests matched")
    required = []
    if any(path.suffix == ".cpp" for path in tests):
        required.append(args.cxx)
    if any(path.suffix == ".js" for path in tests):
        required.append(args.node)
    if args.promtool and any(path.stem == "device_info_prometheus_test" for path in tests):
        required.append(args.promtool)
    for executable in required:
        if not shutil.which(executable):
            parser.error(f"Executable not found: {executable}")
    failures = 0
    for path in tests:
        name = path.stem
        if path.suffix == ".js":
            command = [args.node, str(path)]
        else:
            dirs = list(includes)
            if name == "led_controller_test":
                dirs[0], dirs[1] = dirs[1], dirs[0]
            command = [args.cxx, "-std=c++17", "-DF(x)=x"]
            if args.sdk:
                command += ["-isysroot", args.sdk]
            command += [arg for directory in dirs for arg in ("-I", str(directory))]
            command += [str(path), *[str(ROOT / source) for source in EXTRA_SOURCES.get(name, [])],
                        "-o", str(output / name)]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            if result.returncode:
                print(f"FAIL {name}\n{result.stdout}{result.stderr}", flush=True)
                failures += 1
                continue
            command = [str(output / name)]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        (output / f"{name}.log").write_text(result.stdout + result.stderr)
        print(f"{'PASS' if result.returncode == 0 else 'FAIL'} {name}", flush=True)
        if result.returncode:
            print(result.stdout + result.stderr, flush=True)
            failures += 1
        elif name == "device_info_prometheus_test" and args.promtool:
            fixtures = output / "metrics"
            subprocess.run([str(output / name), "--dump", str(fixtures)], check=True)
            for fixture in sorted(fixtures.glob("*.prom")):
                result = subprocess.run([args.promtool, "check", "metrics", "--extended", "--lint=none"],
                                        input=fixture.read_text(), capture_output=True, text=True)
                print(f"{'PASS' if result.returncode == 0 else 'FAIL'} promtool {fixture.stem}", flush=True)
                if result.returncode:
                    print(result.stdout + result.stderr, flush=True)
                    failures += 1
    print(f"{len(tests)} test programs run; {failures} failures", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
