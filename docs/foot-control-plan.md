# Grotto foot control plan

Written 4 November 2026. Proposed, following the house rules and Prism Patch's `PedalControl`. Works on the M-Vave Chocolate Plus and M-Audio EX-P with no mapping.

## Footswitch defaults

Momentary CC 20, 21, 22, 23 on channel 16 (any channel when read straight from the footswitch's USB cable). Learn remaps any of them.

| Footswitch | Tap | Hold |
| --- | --- | --- |
| A | Bypass on or off, the cave rings out (Trails on) | Momentary: effect on while held |
| B | Runaway on or off | **Runaway while held**: Reflect climbs from the knob into self-oscillation over about 1.5 seconds, then drops back to the knob when you let go, and the scream rings out |
| C | Freeze on or off | **Freeze while held**: the cave holds as a pad, then fades on release |
| D | Dry Kill on or off | Dry Kill while held: wet-only wash |
| A+B | **Tame**: kill Reflect and the tail now | |
| C+D | Hand the footswitches to the next Grotto | Held 1 second: **Keeper** |

Why:

- **One rule for all four.** The pedal's Flexi-Switch is "tap latches, hold is momentary". Grotto uses that rule on every switch, so A behaves exactly like the pedal, and B, C and D work the same way your foot already expects. Your starting idea is each switch's hold. The tap is the latched version of the same job.
- **A** is the pedal's own switch, with V3's tails.
- **B** is the runaway people get by reaching down to crank Reflect. Holding it is the safe version, because letting go ends it. Tapping it latches the runaway for hands-free drones, with A+B as the way out.
- **C** turns a good moment into a pad to play over, which is especially useful on bass.
- **D** is the Eurorack module's Dry Kill. With Freeze on C, C then D gives a frozen, wet-only bed with nothing else.
- **A+B Tame** is the safety net any self-oscillating pedal needs live.

**Keeper on hold C+D doesn't clash.** A pair only counts when C and D go down within 0.12 seconds of each other. Then their single jobs are undone, so no stray freeze or dry kill is left behind, as in OilSlick. Holding C for Freeze and pressing D a moment later is not a pair, so the frozen wet-only wash still works.

## Expression default

CC 11 on channel 16. Moves **Drag** through the current Mode, as V3's jack does. Heel is full anticlockwise (scattered, long, gritty, pitch down), toe is full clockwise (tight reverb). The curve is exponential, so equal travel gives equal pitch change and the sweep feels even across the whole pedal rather than all happening at one end. The pedal's party trick, the pitch dive while the cave rings, is a slow rock back on the heel.

The PITCH QUANTIZE chip on the expression bar picks the Mode, and the menu reaches Length, Reflect, Mix and Dampen as alternative targets. Heel and toe calibration (the EX-P reads backwards on the Chocolate Plus, which Calibrate fixes), smoothing and curve come from `PedalControl`.

## Original pedal MIDI map

None. The pedal has no MIDI, so nothing can clash with the footswitch and expression defaults.

## Tests

`Tests/FootTest.cpp` will cover tap, hold, A+B and C+D in both orders, C held then D (Freeze plus Dry Kill, not Keeper), the 1 second Keeper hold, the Drag expression curve in each Mode, the remote parameters, and a clash test: sustain (CC 64), mod wheel (CC 1), and CC 20 to 23 and 11 on other channels change nothing.
