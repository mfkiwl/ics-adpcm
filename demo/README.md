## Demo

This demo uses 3 preloaded samples to play audio under user control. Samples are loaded into ECP5 block RAM so preloading onto flash isn't necessary. The samples are about 30kbyte total so the ECP5-12 should be capable of running this demo too.

It includes 2 modes:

* Manual control using user buttons to play different notes and samples.
* Automatic control using a simple integrated tracker

### Prerequisites

* [yosys](https://github.com/YosysHQ/yosys)
* [nextpnr-ecp5](https://github.com/YosysHQ/nextpnr)
* [fujprog](https://github.com/kost/fujprog)

### Build and run on ULX3S

```
make ulx3s_prog
```

## Usage

These buttons marked on the board are used for control:

* 1: Start tracker playback (loops automatically)
* 2: Stop tracker playback
* 3: Change selected sample (piano, steel drum, square wave)
* 6 / 4 / 5: Play keys C, D, E respectively
* 0: Change current octave

Output is currently sent to the headphone jack. It's pretty noisy so in the future SPDIF / HDMI output may be added for cleaner output. The `VOLUME` parameter can be used to reduce volume if needed.

## Output

Output is sent to the headphone jack in both analog / digital form (SPDIF).

* Headphones can be plugged in directly to hear the analog output but it's a bit noisy. Output volume can be set at build time using the `VOLUME` parameter.
* SPDIF receiver can be used to hear the digital output. Using a 3.5mm-to-RCA cable will output SPDIF audio using what would be the composite video cable. This can be plugged directly into the coaxial input of the receiver.

## TODO

* Add iCEBreaker target. This can serve as a use case for streaming large audio tracks from flash rather than block RAM.