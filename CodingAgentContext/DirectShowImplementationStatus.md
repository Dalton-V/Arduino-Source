# DirectShow implementation status

DirectShow video is integrated into the Windows application's video selector. Start OBS Virtual Camera, run the rebuilt executable, and select **OBS Virtual Camera (DirectShow)** under **Video Input**. Discovery refreshes automatically; allow a few seconds for the entry to appear. No backend setting needs to be changed.

The local executable is `build/RelWithDebInfo/SerialPrograms.exe`. Restart an older running copy to use this build. The existing Qt camera choices remain available.

## Implemented

- Versioned device identities independent of enumeration order and friendly names, with DevicePath identity and moniker fallback.
- Background cached discovery without binding capture filters on the UI thread. Native entries carry a DirectShow label and use the existing alias mechanism.
- A native graph owner, scoped COM resources, explicit stream configuration and validated connected types. Native startup, capture and graph teardown run on an application global-pool task.
- Progressive uncompressed RGB24, RGB32, YUY2 and NV12 video. Driver-advertised media types supply the resolution and exact frame interval; the integer FPS label does not replace the native interval.
- A custom in-process sample sink and bounded preallocated handoff. The sink owns shared sample storage, so delayed driver references cannot leave a dangling queue reference.
- A Qt frame bridge with actual pixels, correct padded RGB rows and bottom-up orientation, and native NV12 planes. Retained frames keep their storage alive and bound the number of allocations.
- Asynchronous negotiated metadata publication to VideoSession, including saved resolution/format/FPS and selector updates.
- The existing accelerated QQuickWidget display path, with coalesced updates and real frames delivered to frame listeners and recording consumers.
- Headless snapshots through SnapshotManager, with a freshness check and no preview requirement. Startup failures and device-loss events are logged. Reset Video reconstructs the graph; automatic reconnect is not enabled.
- Five registered unit test groups and an explicit opt-in `DirectShowProbe` build target.
- Explicit shutdown of the catalog before the application stops its global pools.

## Validation on November 4 2026

The Windows MSVC RelWithDebInfo application built and deployed successfully with the installed Qt 6.8.3. All five DirectShow unit test groups passed: endpoint identity, media validation, bounded sample ownership, sink COM ownership and Qt frame pixels/ownership. The queue and COM ownership groups each exercise 500 cycles.

A headless integration test exercised the same camera catalog, descriptor factory and VideoSession used by the UI. It confirmed the published label `OBS Virtual Camera (DirectShow)`, then completed three source open/close cycles with at least 60 valid 1920 by 1080 frames each. Snapshots, rejection of future freshness cutoffs, saved negotiated settings and switching to no source all passed. No empty listener frames were observed.

The native OBS probe independently captured approximately 172–174 frames in three seconds at the driver's advertised interval of 166666 in 100 ns units. Explicit IAMStreamConfig::SetFormat was necessary before connection; otherwise OBS proposed a different subtype from the requested media type.

Graphical UI verification was stopped with the physical Escape key. The actual dropdown and on-screen preview were therefore not visually confirmed. The earlier full application test-suite attempt failed before test output with Windows exception 0xc0000409 in Qt6Core.dll 6.8.3. Its cause remains unresolved; these targeted passes do not establish a full regression pass.

## Native diagnostic

Start OBS Virtual Camera and explicitly build and run:

```text
cmake --build build/RelWithDebInfo --target DirectShowProbe
build/RelWithDebInfo/DirectShowProbe.exe
```

This diagnostic opens OBS only, captures for three seconds, stops and disconnects the graph, and fails if no frames arrive. It is excluded from the default build and unattended unit tests. It does not display a window or capture audio.

The five unit test groups are registered through add_tests_DirectShow in the existing UnitTestDatabase and require no physical card.

## Remaining from the broader capture plan

Embedded HDMI audio and shared A/V device sessions are not implemented. OBS virtual-camera video cannot qualify those features. Standalone DirectShow audio, automatic reconnection, complete graph/lifecycle fault injection, long-duration physical-card qualification and Qt 6.10.2 validation also remain outstanding. Linux/macOS compilation has not been run locally; Windows-native files and integrations are guarded by _WIN32.

The current work makes OBS video selectable for live testing. It does not claim completion of the broader DirectShow audio/video qualification plan.
