# NMOS Interlink agent guide

## Purpose and architecture

NMOS Interlink is an independent, open-source protocol gateway intended to connect non-NMOS broadcast/control devices with NMOS infrastructures. Keep the architecture layered and explicit: transport, protocol, domain model, device adapters, and HA must remain separate modules with narrow interfaces.

The project uses C++17, CMake, and Conan 2. Prefer the standard library and small, justified dependencies. Future networking/data needs should prefer Boost.Asio, Boost.Beast, Boost.JSON, Boost.UUID, and OpenSSL for TLS, but do not add them until a concrete feature requires them.

This project is independent of KATE-NMOS and Sony nmos-cpp. Do not depend on them. Do not use cpprestsdk or WebSocket++. IS-04, IS-12, and MS-05 are normative specifications; nmos-cpp may only be consulted as an implementation reference. Do not add Docker unless it is explicitly approved.

## Model and command safety

NMOS identities must be persistent and stable across restarts. Model state quality explicitly: requested, observed, unknown, and stale are distinct states. Device commands require clear intent, validation, timeouts, and observable outcomes; never silently treat an unconfirmed command as applied.

HA invariants are non-negotiable: only the currently authorized owner may issue device commands; ambiguous or non-idempotent actions must never be replayed automatically; and do not claim split-brain safety unless effective fencing is implemented and verified. Ownership, reconciliation, priority, and fencing are future design work, not existing capabilities.

## Working conventions

- Inspect existing files and conventions before editing. Make the smallest change that serves the approved scope.
- Keep implementations maintainable and dependency-light. Avoid speculative dependencies, placeholder implementations, and unnecessary refactors.
- Add tests for every significant behavior, including negative paths where relevant. Run the applicable tests and report evidence.
- Do not claim NMOS conformance, HA, TLS, or multi-platform support without specific verified implementation and tests.
- Do not commit, push, merge, open a PR, switch branches, use destructive Git commands, or modify source outside the approved scope. Preserve tracked and untracked user files.
