# PAL Timing Calibration

## Observed reference

- Emulator: SDL2VICE 3.10, with the C64 model forced to PAL through the `-pal` option.
- Disk image: `Boulder Dash.d64`.
- SHA-256: `69a5fec66a8e88ef2750118dd4ce932da7691fc0bb5aeb6844b293a7cc5c62d9`.
- Protocol: VICE binary monitor with checkpoints on `ProcessCave`, `Animate`, and writes to `SubSecondCounter` at `$ae`.
- The C64 reference corpus was not modified.

The raw observations and their protocol traces are stored in `docs/porting/fixtures/vice-pal/`. Each primary file records the VICE version, command line, D64 hash, and structured results.

## Verified facts

`SubSecondTick` at `$7111-$712c` resets its counter after `$3c`, or 60 calls. Ten consecutive windows of 60 calls were observed for each of the five sublevels.

A 60-sub-tick window normally consumes 1,159,704 CPU cycles. Rare 23-cycle differences occur at sample boundaries; totals observed over 600 sub-ticks remain within 23 cycles of the central value. The retained rational reference is therefore:

- 600 sub-ticks: 11,597,040 CPU cycles;
- PAL CPU frequency used by the port: 985,248 cycles per second;
- average cadence: 50.974110635 sub-ticks per second;
- average sub-tick duration: 19.617801812 ms;
- duration of a 60-sub-tick game "second": 1.177068109 s.

The observed `ProcessCave` call counts over 600 sub-ticks are exactly:

| Sublevel | `ProcessCave` calls | Average period |
|---:|---:|---:|
| 1 | 79 | 148.996 ms |
| 2 | 90 | 130.785 ms |
| 3 | 99 | 118.896 ms |
| 4 | 104 | 113.180 ms |
| 5 | 106 | 111.044 ms |

The animation trace confirms that `Animate` advances by one phase every two sub-ticks. An eight-phase cycle therefore lasts 16 sub-ticks, or approximately 313.885 ms at the calibrated average cadence. Cycle intervals vary according to interrupts occurring while the main code runs; the deterministic reference is the ordering and number of sub-ticks.

## Porting decisions

- The engine fixed step now represents one `SubSecondTick` call instead of an arbitrary modern frame at 60 Hz.
- The runner produces those steps from the exact ratio `600 * 985248 / 11597040`.
- A cave second, Rockford's appearance countdown, and the magic wall advance after 60 sub-ticks.
- The five cave-scan cadences are distributed directly as 79, 90, 99, 104, and 106 calls over 600 sub-ticks.
- Delays inherited from C64 loops, including time-bonus conversion and the transition between caves, remain expressed in CPU cycles.
- The modern cave-transition presentation retains approximately its previous 0.8 s cover and 2.05 s reveal phases. They become 41 and 105 sub-ticks respectively; the 146-sub-tick total remains aligned with the cycle-timed engine transition.

## Remaining uncertainty

The title-screen cadence is driven by `TitleIRQActions`, not by `SubSecondTick`. It was not included in this measurement campaign. Its current behavior is preserved until a dedicated observation is available; no PAL cadence is extrapolated from the gameplay measurements.
