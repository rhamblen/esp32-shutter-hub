# Vendored library patches

`apply_patches.py` is a PlatformIO **pre-build** hook (`extra_scripts = pre:patches/apply_patches.py`
in `platformio.ini`) that overwrites files inside downloaded libraries with our patched copies before
each build. This keeps the patch in version control and independent of the `.pio/libdeps` cache (which
is git-ignored and wiped on library reinstall).

## HomeSpan.cpp — newlib-nano `%m` sscanf hang (the v0.7.0 HomeKit fix)

**Library:** `homespan/HomeSpan @ ~1.9.1` (pinned; see the HomeKit note in `docs/` / memory).

**Symptom:** with the HomeKit bridge enabled, the device was never discoverable in Apple Home and
manual codes dropped instantly. HomeSpan's poll task reached "Device not yet Paired" and then did
nothing — no `_hap._tcp`, port 1201 closed, connect callback never fired. No crash/reboot.

**Root cause:** `Span::checkConnect()` validates the mDNS hostname with

```c
char *d;
sscanf(hostName, "%m[A-Za-z0-9-]", &d);
if (... || strlen(hostName) != strlen(d)) { LOG0("PROGRAM HALTED!"); while(1); }
```

The `%m` allocating conversion is a POSIX/glibc extension that **newlib-nano does not implement**, so
`d` is never populated, `strlen(hostName) != strlen(d)` is falsely true, and HomeSpan hits its
`while(1)`. (`shutter-hub` is a perfectly valid hostname — the check itself was broken.)

**Patch:** `patches/HomeSpan.cpp` replaces the `%m` sscanf + validation with an equivalent nano-safe
character loop (search for `[PATCH]`). Everything else is upstream 1.9.1 verbatim.

### Refreshing this patch after a HomeSpan version bump
1. Build once so PlatformIO downloads the new HomeSpan into `.pio/libdeps/<env>/HomeSpan/`.
2. Diff `patches/HomeSpan.cpp` against the new upstream `src/HomeSpan.cpp`.
3. Re-apply the `[PATCH]` hunk (and remove any leftover `hsCrumb` diagnostic breadcrumbs — those are
   temporary), copy the result back to `patches/HomeSpan.cpp`.
