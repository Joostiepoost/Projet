# Projet engine: measurements

Machine: Intel(R) Xeon(R) Processor @ 2.10GHz, Linux. Date: 6 Nov 2026 11:18:05am.

## 1. Import: placement, length and level

A 10 s test file (quiet noise, single-sample clicks at 1.000, 2.500 and 7.250 s, a 1 kHz sine from 8 to 9 s) imported into a 48 kHz project. **Click error**: where each click lands in the placeholder, compared with where it should be, in samples at 48 kHz (largest of the three; parabolic interpolation, so fractions are meaningful). **Length**: placeholder length minus the expected length. **Level**: change of the sine's level.

| Source rate | Placeholder | Click error (samples) | Length (samples) | Level (dB) |
| --- | --- | --- | --- | --- |
| 44.1 kHz | Ogg Vorbis q0.7 | 0.002 | 0 | 0.076 |
| 44.1 kHz | FLAC 24-bit | 0.001 | 0 | 0.000 |
| 48.0 kHz | Ogg Vorbis q0.7 | 0.000 | 0 | 0.076 |
| 48.0 kHz | FLAC 24-bit | 0.001 | 0 | -0.000 |
| 96.0 kHz | Ogg Vorbis q0.7 | 0.001 | 0 | 0.076 |
| 96.0 kHz | FLAC 24-bit | 0.001 | 0 | -0.000 |

## 2. Playback timing

Boxes holding a single click, placed at 1.000, 2.500, 3.333 333 and 5.000 021 s in a 48 kHz project, played with the device at 48 kHz. **Error**: largest distance between where a click sounds and the sample its box starts on (round(start x 48000)).

| Block size | Error (samples) |
| --- | --- |
| 32 | 0 |
| 64 | 0 |
| 333 | 0 |
| 512 | 0 |
| 4096 | 0 |

## 3. Locating inside a box

A box holding a ramp (each sample's value is its index) starts at 1.000 s. Playback starts at a position inside it; the first output sample must be the ramp's sample at that position.

| Start position (s) | Expected sample | Played sample | Match |
| --- | --- | --- | --- |
| 1.00000 | 0 | 0 | yes |
| 1.23450 | 11256 | 11256 | yes |
| 2.50000 | 72000 | 72000 | yes |
| 4.99997 | 191999 | 191999 | yes |

## 4. Fades and crossfades

A box of constant level 1.0 with a 100 ms fade in and out. Gain halfway through the fade (expected: linear 0.5, equal power 0.707, S-curve 0.5), at the last sample of the fade in (expected 1 within a sample's step) and at the last sample of the box (expected 0).

| Shape | Halfway | End of fade in | Last sample |
| --- | --- | --- | --- |
| Linear | 0.5000 | 0.9998 | 0.0000 |
| Equal power | 0.7071 | 1.0000 | 0.0000 |
| S-curve | 0.5000 | 1.0000 | 0.0000 |

Two overlapping boxes with a 500 ms overlap, the first fading out while the second fades in over the overlap. Equal power keeps the summed *power* constant (right for unrelated sounds); linear and S-curve keep the summed *level* constant (right for identical sounds). Largest deviation inside the overlap:

| Shape | Summed level (min / max) | Summed power (min / max) |
| --- | --- | --- |
| Linear | 1.0000 / 1.0000 | 0.5000 / 0.9999 |
| Equal power | 1.0000 / 1.4142 | 0.9999 / 1.0000 |
| S-curve | 0.9999 / 1.0000 | 0.4999 / 1.0000 |

## 5. Device at another rate than the project

A 48 kHz project played on a device at 44.1 or 96 kHz goes through the sinc kernel ("High"). A click box at 1.000 s must sound at 1.000 s of device time; a 1 kHz sine must keep its level.

| Device rate | Click error (device samples) | Sine level change (dB) |
| --- | --- | --- |
| 44.1 kHz | 0.000 | 0.000 |
| 96.0 kHz | 0.000 | -0.000 |

## 6. CPU

Time to produce 1 s of stereo output, as a share of one core of this machine.

| Case | Load |
| --- | --- |
| 8 overlapping boxes, device = project rate | 0.02% |
| 32 overlapping boxes, device = project rate | 0.05% |
| 32 overlapping boxes in long fades, device = project rate | 2.38% |
| 8 overlapping boxes, device 44.1 kHz (resampled) | 4.47% |
| 32 overlapping boxes, device 44.1 kHz (resampled) | 4.17% |

## 7. Import speed and size

A 2.0-minute stereo file of coloured noise, imported into a 48 kHz project. Times for one file (rate conversion uses all cores); two files import at a time.

| Source | Placeholder | Read | Resample | Encode | Total | x real time | Size per minute |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 44.1 kHz WAV 24-bit (15.9 MB/min) | Ogg Vorbis q0.7 | 0.12 s | 0.22 s | 1.47 s | 1.81 s | 66x | 1.60 MB |
| 44.1 kHz WAV 24-bit (15.9 MB/min) | FLAC 24-bit | 0.10 s | 0.20 s | 0.35 s | 0.65 s | 184x | 14.31 MB |
| 48.0 kHz WAV 24-bit (17.3 MB/min) | Ogg Vorbis q0.7 | 0.11 s | 0.00 s | 1.47 s | 1.58 s | 76x | 1.66 MB |
| 48.0 kHz WAV 24-bit (17.3 MB/min) | FLAC 24-bit | 0.11 s | 0.00 s | 0.33 s | 0.44 s | 275x | 14.46 MB |
| 96.0 kHz WAV 24-bit (34.6 MB/min) | Ogg Vorbis q0.7 | 0.28 s | 0.38 s | 1.44 s | 2.10 s | 57x | 1.64 MB |
| 96.0 kHz WAV 24-bit (34.6 MB/min) | FLAC 24-bit | 0.26 s | 0.38 s | 0.34 s | 0.98 s | 123x | 14.71 MB |

## 8. Fast sample-rate conversion

Imports convert the rate with a precomputed polyphase table built from the same sinc kernel as SincReader, spread over all cores. Checked against SincReader evaluated at every position: largest difference on 10 s of noise (away from the edges), and the time each takes.

| Conversion | Largest difference (dB below full scale) | Polyphase | Reference |
| --- | --- | --- | --- |
| 44.1 to 48.0 kHz | 130.5 dB | 0.02 s | 0.12 s |
| 96.0 to 48.0 kHz | 131.4 dB | 0.03 s | 0.44 s |
| 88.2 to 48.0 kHz | 130.5 dB | 0.03 s | 0.38 s |
| 48.0 to 96.0 kHz | 130.5 dB | 0.03 s | 0.14 s |


All checks passed.
