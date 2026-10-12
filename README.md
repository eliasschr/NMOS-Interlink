# NMOS Interlink

NMOS Interlink is an independent, open-source protocol gateway project intended to connect non-NMOS broadcast and control devices with NMOS infrastructures.

## Status

**Pre-alpha / nonfunctional bootstrap.** This repository currently provides only a small C++17 command-line executable and build/test foundation. It does not implement NMOS, device control, network services, protocol adapters, TLS, or high availability. No claim of NMOS conformance, HA safety, or multi-platform compatibility is made at this stage.

## Direction

The planned first adapters are OSC and OSCQuery; Ember+ is planned for a later phase. The intended core will use the IS-04 Node API and IS-12/MS-05 control model as normative references, with modular transport, protocol, device-model, adapter, and HA layers.

The proposed HA direction is explicit device ownership, state reconciliation, configurable priorities, and fencing. Only an authorized owner should issue device commands; ambiguous or non-idempotent actions must not be replayed automatically. These are design goals, not delivered capabilities.

## Prerequisites

- CMake 3.20 or newer
- A C++17-capable compiler
- Optional: Conan 2 for the future dependency workflow

No third-party packages are required for this bootstrap.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the skeleton executable directly:

```sh
./build/nmos-interlink --help
./build/nmos-interlink --version
```

On multi-configuration generators, add `--config Debug` to the build command and invoke the executable from the Debug configuration directory.

## Non-goals for this bootstrap

- Implementing IS-04, IS-12, MS-05, OSC, OSCQuery, or Ember+
- Shipping HA or split-brain protection
- Depending on KATE-NMOS, Sony nmos-cpp, cpprestsdk, or WebSocket++
- Adding Docker, databases, service discovery, or protocol libraries
