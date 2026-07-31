# Goal
Implement the listed LV2 features in the loader in a sequence that preserves the current core loading path and keeps each step testable.

# Implementation Plan

## 1. Feature plumbing [implemented]
- Added a feature registry in `Plugin` that builds `LV2_Feature` entries before `instantiate()`.
- Kept feature construction isolated from port loading so the existing load path still works when a feature is unavailable.
- Added basic ownership handling for feature-backed allocations so cleanup is paired with plugin shutdown.

## 2. LV2 Map [implemented]
- Implemented `LV2_URID_Map` support through the host feature list so plugins can request URIDs during instantiation.
- Added a stable URI-to-URID mapping table per plugin instance that can be reused by later atom/state work.
- Exposed a host-side `mapUri()` helper so the loader can resolve URIs consistently without ad-hoc string handling.

## 3. LV2 Buffer Size [implemented]
- Exposed the host buffer size to plugins through the extension data passed at instantiate time.
- Used the JACK buffer size as the source of truth for the host-side block size.
- Made audio processing and atom/event buffering use the same frame limit.

## 4. LV2 Worker [implemented]
- Added worker scheduling hooks so plugins can enqueue non-realtime work from the audio thread.
- Provided a host callback path that drains worker requests outside the realtime callback.
- Set up a queue for deferred file loading or other slow operations that should not run in `process()`.

## 5. LV2 State [implemented]
- Added save/restore support for plugin state using URIDs from the Map implementation.
- Persisted control values into a host-side state object.
- Restored state before activation when a saved session is available.

## 6. LV2 Patch [implemented]
- Represented patch objects as structured messages instead of raw strings.
- Routed patch writes through the same host-side message queue used by worker/state handling.
- Added support for the patch-style message path needed by the loader first, then widened coverage later.

## 7. Validation
- Test plugin discovery and instantiation with features enabled and disabled.
- Confirm control ports still connect and process correctly after feature plumbing is added.
- Add focused fixtures for atom:Path and any plugin that requires worker/state behavior.

## Order of delivery
1. Map
2. Buffer Size
3. Worker
4. State
5. Patch

## Open questions
- Feature support only for loaded plugins
- state only kept in memory

## Current status
- Step 1: implemented
- Step 2: implemented
- Step 3: implemented and verified in the loader core
- Host-side UI build remains blocked by missing GTK development headers in this environment
