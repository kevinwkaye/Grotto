# Grotto: research and feature map

Written 4 November 2026. Grotto recreates the behaviour of a cavernous digital reverb pedal (the EarthQuaker Devices Afterneath) in the Flip Fella family look. All DSP is original code written from public descriptions of the pedal. The plugin UI names no brands.

The pedal's reverb isn't a hall or plate model. It's a swarm of short delay lines. Drag pulls them apart into scattered ping-pong echoes and bends their pitch as it moves, and Reflect feeds the swarm back into itself until it rings on its own. Those quirks are the point, so Grotto keeps them.

## Versions

| Version | Year | What changed |
| --- | --- | --- |
| V1 | 2014 | Six knobs: Length, Diffuse, Dampen, Drag, Reflect, Mix. One bypass footswitch, mono, 9 V. Turning Drag gives "an even and linear change in the pitch effect" (forum owner) |
| V2 | about 2016, with Dispatch Master, Acapulco Gold and Speaker Cranker | Flexi-Switch relay soft switching (tap latches, hold is momentary), sturdier enclosure, top-mounted jacks. The maker said the V2s are "not sonically any different" from the originals. One retailer says V2 added an expression jack. Every other source puts that in V3, so treat it as a mistake |
| V3 | NAMM, January 2020 | Same six knobs and sound, plus three features: an **expression and CV jack for Drag** (0 to 5 V); a small **Mode** knob under Drag with **nine Drag modes**, shown by LED colour; **Tails or no tails** bypass (hold the footswitch while plugging in power to toggle, ships with tails on). Buffered bypass, Flexi-Switch kept |
| Eurorack module | 2020 | A cousin, not a pedal version. Same engine, CV over Drag, and a **Dry Kill** switch because Mix never goes fully wet. The manual notes self-oscillation is normal with Length and Reflect both past noon, and that with CV on Drag "you have an interesting voltage controlled oscillator" |

The nine V3 Drag modes: unquantized, unquantized with slew, unquantized volt per octave, chromatic, major, minor, pentatonic, octaves and fifths, octaves.

**Recommendation: recreate V3.** The sound is the same in all three, so nothing is lost. V3 is the only one with expression on Drag, which is exactly your default. Its quantized modes turn the pitch warp into something you can play in key, and its tails setting matches the bypass with trails you want. V1's smooth Drag is V3's first mode, so the original feel is one click away. Dry Kill is borrowed from the Eurorack module for footswitch D.

## Controls (from the manuals)

| Control | What the pedal does | How Grotto does it |
| --- | --- | --- |
| **Length** | Decay length of the reverb | Feedback inside each swarm line, worked out for a decay time from about 0.4 s to 15 s on an exponential taper |
| **Diffuse** | Spread of the reverb. Anticlockwise sharper with more attack, clockwise more ambient and washy | Allpass smearing on every swarm line, from none (you hear each short delay as a distinct tap) to four stages at high gain (a smooth wash). It also crossfades early taps into the smeared ones, so anticlockwise keeps the pick attack |
| **Dampen** | Tone of the reverb. One manual says clockwise brighter, another says clockwise darker | Clockwise darker, as the name suggests: a low-pass inside the feedback loops from 16 kHz down to about 700 Hz, so every repeat gets darker than the last, like a real cave. Proposed default: 2 o'clock |
| **Drag** | "This digital reverb is made up of a bunch of short delays, this separates the reverb." Clockwise: delays pulled together, a reverb. Anticlockwise: they pull apart into scattered ping-pong delays. An owner reports Drag behaves like a sample-rate control: clean and shortest at full, grungier and longer as it comes down, aliasing below noon. Turning it while sound is in the lines warps the pitch | The swarm (16 delay lines with unrelated lengths, about 3 to 45 ms at full clockwise) runs on its own clock. Drag sets that clock: down to about 1/12 of the rate at full anticlockwise, which stretches every line out to around 0.5 s and splits the left and right sides into ping-pong echoes. Because the clock changes the speed of sound already in the lines, turning Drag bends the pitch of the tail, as on the pedal. Below noon the slower clock is not filtered, so it aliases and gets gritty, the same idea as Murmur's Clock |
| **Mode** (V3) | How Drag responds, nine modes | The same nine modes, on the knob and the expression pedal. Quantized modes step the clock so pitch jumps land on intervals of the original: chromatic, major, minor, pentatonic, octaves and fifths, octaves. "With slew" glides between settings. The volt per octave mode becomes "even pitch": equal travel gives equal semitones |
| **Reflect** | Regeneration of the reverb. Clockwise more wash and echoes. Self-oscillates when turned up high, especially with Length past noon. The oscillation pitch is mostly set by the note you play into it | A second loop that feeds the whole swarm's output back into its input, with a soft clipper. Loop gain reaches 1 at about 80% of the knob (sooner with Length past noon) and goes beyond it after that, so the clipper holds a steady roar. The swarm's resonances are excited by what you play, so the scream follows your note, and Drag then bends it |
| **Mix** | Wet and dry balance. Never completely wet | Dry stays at unity to noon, then dips to about −10 dB at full while the wet rises, so it never goes completely wet on its own. Dry Kill (footswitch D) gives the fully wet wash |
| Footswitch | V2 and V3 Flexi-Switch: tap latches, hold is momentary. V3 tails setting | Footswitch A. Trails on by default, as V3 ships. See the foot control plan for B to D |
| Expression jack (V3) | Drag, through the chosen Mode | Expression moves Drag by default (see below) |

## New for Grotto

| Control | Why | What it does |
| --- | --- | --- |
| **Low cut** | Bass. The pedal's low end builds up in long tails | High-pass on the reverb input and inside the Reflect loop, 20 to 300 Hz, default 90 Hz. The dry signal is never touched, so your bass keeps every bit of its low end and only the cave above it is thinned |
| **Bass** (Deep page) | Bass through a self-oscillating reverb | Shifts the low cut and Dampen defaults for bass (low cut 120 Hz) and keeps the dry low end under Dry Kill (question for Kevin) |
| **Trails** | V3's tails setting, without the power cycle | Let the cave ring out when bypassed |
| **Tame** | Self-oscillation needs a safety net live | Kills Reflect and the tail instantly |
| **Freeze** | Holds the cave as a pad you play over | The swarm loops at unity gain without growing, input muted into it |
| **Dry Kill** | From the Eurorack module | Fully wet wash |

## The character to keep

- **You hear the short delays.** With Diffuse down, the swarm is a cluster of distinct taps, the metallic, boxy early sound people describe, not a smooth algorithm.
- **Drag warps pitch.** Moving Drag changes the speed of what's already in the lines, so the tail dives as you turn anticlockwise and climbs as you turn clockwise. Fast sweeps give the warped, tape-like glide from the demos. Unquantized modes glide; quantized ones jump to intervals.
- **Grit low down.** Below noon, Drag's slow clock aliases on purpose: grungy, lo-fi and longer, as owners report.
- **Scattered ping-pong.** Full anticlockwise, the left and right sides of the swarm pull apart into separate echoes.
- **Runaway you can play.** Reflect past about 80% and the cave sings on its own, following the note you fed it. Drag then bends the scream, the pedal's best-known trick. At the very top it saturates into a roar instead of clipping digitally.
- **Glitches stay.** Clock changes are not crossfaded, so fast Drag moves click and stutter a little, as the pedal does. Slew mode smooths them.

## Bass

Your bass goes through untouched: the dry path has no filtering and no delay. The reverb gets a low cut (90 Hz by default, 120 Hz in Bass mode) at its input and inside the Reflect loop, so a runaway can't fill up with sub. Dampen darkens the tail but can't add mud. The test checklist includes an open E and B string through full Length and Reflect.

## Footswitches and expression

See `docs/foot-control-plan.md`.

## Code shared with the family

| Code | From | Used for |
| --- | --- | --- |
| `PedalControl.*`, `Keeper.*` | Prism Patch (copied unchanged, as the house rules ask) | Footswitches, expression, Learn, calibration, shared `Flip Fella Pedals.xml`, Keeper on hold C+D |
| `DspUtil.h` (`DelayLine`, `Svf`, `Butter4`, `DcBlock`, `Smoother`, `Rng`, `softClip`) | Speckle and Prism Patch | The swarm's delay lines, Low cut, Dampen, safe clipping in the Reflect loop |
| Tap cluster, allpass diffusion, Householder feedback mix | Murmur's Wet Channel reverb | The shape of the swarm: taps, then diffusion, then a mixed feedback network. Grotto's version has 16 lines and its own clock |
| Clock (engine running on a stretchable rate) | Murmur | Drag: changing the swarm's clock bends pitch and aliases below noon |
| Footswitch job wiring, runaway hold, Tame, Freeze, bypass with trails | OilSlick | B's runaway, Tame on A+B, C's Freeze and A's bypass, with the pair-undo logic |
| Pebble, flip cards, clock-face knobs, panel palette | Murmur, Speckle and Jetstream | The look |

The swarm lives in its own header (`Source/engine/Swarm.h`) with no Grotto-specific code, so it can be copied as the starting point for Vesper (kevinwkaye/Monolith, empty as of 4 November 2026) and later shared with Murmur's Reverb. Grotto ships with its own copy first so nothing in Murmur changes.

## Decided (Kevin, 4 November 2026)

V3, with expression on Drag. Stereo, with a Mono switch for pedal-exact behaviour. In Bass mode, Dry Kill keeps the dry signal below 120 Hz so the fundamental stays solid under the wash. Footswitches as in the foot control plan.

## Family conventions

- Engine in plain C++17 with no JUCE (Portability), plus an offline render test.
- Footswitches CC 20 to 23 and expression CC 11 on channel 16, read straight from the footswitch's USB cable in every copy.
- Keeper on hold C+D for 1 second.
- Pages: Main, Deep, Pedal. Every footswitch job has an on-screen button.
- Builds: Windows VST3 and Linux ARM64 through GitHub Actions, published to the `builds` branch, with CPU figures.

## Sources

- Afterneath manuals (V1, V2, V3 EU R1, Eurorack) and the maker's product page, as quoted in retailer listings and manual mirrors
- Guitar Player, Afterneath V3 review; Premier Guitar and Guitar.com, V3 launch, NAMM 2020
- gearnews.com, the four V2 upgrades
- Guitar World video review (Drag "spits out short, ping-pong delays that can be slowed down or sped up for warped-out sounds")
- Guitar.com V3 review (expression on Drag mid-flight)
- Perfect Circuit and synthtopia (Reflect up to self-oscillation, "the pedal can act as its own sound source")
- Mod Wiggler, "Afterneath with synths?" (Drag as sample rate, aliasing below noon)
- MOD Audio forum, "Reverb with drag function a la EQD Afterneath?"; The Gear Forum Helix thread (V1's linear Drag pitch change)
- The maker's blog, "Afterneath For Noodlers"
