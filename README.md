# Grotto

A VST3 for Ableton Live, recreating the EarthQuaker Devices Afterneath otherworldly reverberation machine (V3) in the Flip Fella look. All DSP here is original code written from public descriptions of the pedal's behaviour.

The reverb isn't a hall or a plate. It's a swarm of short delay lines. Drag pulls them apart into a scattered ping-pong and bends their pitch as it moves, and Reflect feeds the swarm back into itself until the cave sings on its own. Those quirks are the point.

## Install

1. Open the **builds** branch of this repo and download `Grotto-Windows.zip`.
2. Unzip it and copy the `Grotto.vst3` folder to `C:\Program Files\Common Files\VST3`.
3. In Ableton, rescan plugins (Preferences, Plug-Ins, Rescan). Grotto appears under Audio Effects.

The same branch has `Grotto-Linux-ARM64.zip`, built for the future Raspberry Pi rig.

## The panel

| Control | What it does |
| --- | --- |
| **Drag** (the big dial) | Clockwise, the short delays sit together as a reverb. Anticlockwise, they stretch out (up to 12 times longer) and pull apart into scattered ping-pong echoes, gritty and aliased below noon. Turning it while the cave rings bends the pitch of the tail: down as you turn anticlockwise, up as you turn back |
| **Mode** (the chip under Drag) | V3's nine Drag modes. Smooth moves the delay time evenly, like the original. Glide does the same but slides slowly between settings. Even gives equal pitch change for equal travel. Chromatic, Major, Minor, Pentatonic, 8ve + 5th and Octaves snap Drag to those intervals, shown as teal dots on the dial. Click to step through, right-click for the list |
| **Length** | Decay, about 0.4 s to 15 s |
| **Diffuse** | Anticlockwise you hear each short delay as a distinct, metallic tap with your pick attack. Clockwise it smears into a wash |
| **Dampen** | Clockwise darker: every pass round the cave loses a little more top |
| **Reflect** (red hand) | Regeneration. Up high the cave stops dying away and sings on its own, following the note you fed it. With Length at noon that's about 80% of the knob, and sooner with Length up, as on the pedal. Drag then bends the scream |
| **Mix** | The dry stays at full level to noon, then dips. It never goes fully wet on its own, like the pedal. Dry Kill does that |
| **Low cut** | Thins the reverb only, 20 to 300 Hz, default 90 Hz. Your dry signal is never filtered |

The **Deep** page has **Bass** (reverb low cut at least 120 Hz, and Dry Kill keeps your fundamental), **Mono** (mono in and out, as the pedal), **Trails** (the cave rings out when bypassed, on by default as V3 ships), **Tame**, the Drag modes with notes, and Live or Studio for Keeper.

**Pebble** lives in the cave. Every note echoes round the walls as fading Pebbles, more of them with Length and Reflect. Low Drag stretches Pebble wide and scatters the echoes left and right, and moving Drag slides them up or down with the pitch. In a runaway Pebble shakes and screams and the cave glows red. It holds its breath for Freeze, goes see-through for Dry Kill, dozes when bypassed, taps its foot on a footswitch press, leans with the expression pedal, flinches at Tame and nods when you keep a moment. Click the cave to ping it.

## Playing bass through it

Your bass passes through untouched: the dry path has no filtering and no delay. The reverb has a 24 dB per octave low cut at its input (90 Hz by default) and a low shelf inside the feedback, so a long tail or a runaway can't fill up with sub. Turn on **Bass** on the Deep page to raise the low cut to at least 120 Hz and keep everything under 120 Hz of your dry signal when Dry Kill is on, so the fundamental stays solid under a wet-only wash.

## Footswitches and expression (optional)

Everything works from the screen, so you don't need any pedals. The four squares along the bottom act exactly like footswitches, taps, holds and pairs included. If you have a MIDI footswitch:

1. Set its first four switches to send **momentary CC 20, 21, 22 and 23 on MIDI channel 16** (127 when pressed, 0 when released). Or use **Learn** on the Pedal page to use whatever yours sends.
2. Set an expression pedal input to **CC 11 on channel 16**, or click the expression bar and choose Learn.
3. Click **Calibrate heel and toe** under the expression bar, then rock the pedal from heel to toe once.

These settings are shared by every Flip Fella plugin on your computer, so you set them up once.

The pedal has one footswitch: tap to latch, hold for momentary. Grotto gives all four that rule:

| Footswitch | Tap | Hold |
| --- | --- | --- |
| A | Bypass on or off, the cave rings out | On while held |
| B | **Runaway** on or off | Runaway while held: Reflect climbs from the knob into self-oscillation over about 1.5 seconds, then drops back when you let go while the scream rings out |
| C | **Freeze** on or off: the cave holds as a pad you play over | Freeze while held |
| D | **Dry Kill** on or off: a wet-only wash | Dry Kill while held |
| A+B | **Tame**: kill Reflect and the tail now | |
| C+D | Hand the footswitches to the next Grotto | Held 1 second: **Keeper** |

Every switch acts on the press, so it's instant. If a press turns out to be half of a pair (A+B or C+D within 0.12 seconds), it is undone, so pairs never leave a stray job behind. Holding C to freeze and then pressing D a moment later isn't a pair, so you get a frozen, wet-only bed.

**Expression** moves **Drag** by default, as V3's jack does. Heel is full anticlockwise (scattered, long, the tail bent furthest down), toe a tight reverb. The sweep is even in pitch whatever the Mode, so a slow rock back on the heel is a smooth dive; in a stepped mode the dive lands on the mode's intervals. Click the bar to move Reflect, Length, Mix, Dampen or Diffuse instead. On Reflect most of the travel sits near the edge where the cave starts to sing. The Pedal page sets what the heel and toe reach, smoothing and the curve.

### Getting footswitch MIDI to the plugin in Ableton

Grotto reads your footswitch straight from its USB cable, so every copy you load works with no MIDI track. The dot next to **FOOTSWITCHES** turns green when it's connected, and Pebble says FOOTSWITCHES READY.

1. In Ableton's Preferences, Link Tempo MIDI, turn **Track**, **Sync** and **Remote** off for your footswitch's input. Windows lets only one program use a MIDI input at a time, so Ableton has to let go of it.
2. Load Grotto. That's it.

If the dot stays grey, click **FOOTSWITCHES**. The menu says what's wrong, and you can pick your footswitch from the list if it wasn't found automatically.

With several copies loaded, the footswitches drive the one you opened or clicked last. Tap C+D to hand them to the next copy.

Prefer routing through Ableton? Choose **Only from a MIDI track in Ableton** in that menu, then turn **Track** back on for the footswitch, make a MIDI track with **MIDI From** set to the footswitch and **MIDI To** set to Grotto, and arm it.

The pedal itself has no MIDI, so there is no original pedal MIDI map to clash with the footswitches. Anything else on the cable, the sustain pedal on CC 64 included, is ignored.

### Kevin's setup (M-Vave Chocolate Plus and M-Audio EX-P)

In CubeSuite: Custom mode, footswitches A to D as momentary CC 20 to 23, the pedal jack as Expression Pedal on CC 11. It always sends on channel 1 whatever CubeSuite says; read directly from its cable, Grotto accepts any channel. Put the Chocolate Plus switch on **U** and connect with the USB cable, not Bluetooth. The EX-P needs a 3.5 mm to 6.35 mm TRS adapter and reads backwards on the Chocolate Plus, which Calibrate fixes.

## Keeper

Press **KEEP** (or hold C+D for a second) to log the moment: every setting, the expression pedal, the tempo, and the last 8 seconds of input and output as WAV files, into `Documents\Flip Fella Keepers`. The small arrow next to KEEP adds a note, exports a zip for Claude, opens the folder, sets Live or Studio, and learns a dedicated Keeper switch.

## Remote control

Every control, every footswitch job and Keeper are in one parameter list that a host or a remote controller can read and set. `footA` to `footD` act as the footswitches (1 to press, 0 to release, so taps, holds and pairs all work), `keeper` and `tame` fire once and reset, and `runaway`, `freeze` and `dryKill` can be set directly.

## CPU

Per instance, one core, scattered Drag with the grit clock and Reflect high (the heaviest case). Every build writes fresh figures for Windows x64 and Linux ARM64 to `cpu-windows-x64.txt` and `cpu-linux-arm64.txt` on the builds branch.

## Building

GitHub Actions builds the Windows VST3 and the Linux ARM64 build on every push and publishes them to the `builds` branch. To build locally: `cmake -S . -B build && cmake --build build --config Release`. JUCE downloads automatically on the first configure.

## Tests

- `grotto_render_test` runs the engine with no host: 34 scenarios at three sample rates (every control, all nine Drag modes, Drag sweeps, Reflect singing and staying just under the edge, the runaway taking off and settling, Tame, Freeze, bypass with and without trails, Dry Kill, a bass low E staying clean, a bass runaway with no sub, Bass mode keeping the fundamental under Dry Kill, Mono), checks for NaNs, levels above full scale and dead output, then measures CPU. Pass a folder to write a WAV of each scenario.
- `grotto_foot_test` drives the real plugin with plain MIDI press and release messages: tap and hold on all four switches, A+B and C+D in both orders, C held then D, the 1 second Keeper hold, the expression curves, the remote parameters, and a clash test (sustain pedal, mod wheel and the footswitch numbers on other channels change nothing). Run it with `--snapshot <folder>` under a display to render the panel in eight states.

## Project layout

| Path | What it holds |
| --- | --- |
| `Source/engine/Swarm.h` | The swarm of short delays: plain C++17, nothing Grotto-specific, ready for Vesper to copy |
| `Source/engine/GrottoEngine.h` | Drag and its modes, Reflect and the runaway, bypass, Freeze, Dry Kill, Bass, Mono |
| `Source/Params.h` | Every parameter, its range and default, and the expression curves |
| `Source/PluginProcessor.*` | Host wrapper, footswitch jobs, expression, Keeper, state |
| `Source/PluginEditor.*` | The Main, Deep and Pedal pages, the cave and Pebble |
| `Source/PedalControl.*`, `Source/Keeper.*` | Shared with every Flip Fella plugin, copied from Prism Patch |
| `docs/` | The research, feature map and footswitch plan |
