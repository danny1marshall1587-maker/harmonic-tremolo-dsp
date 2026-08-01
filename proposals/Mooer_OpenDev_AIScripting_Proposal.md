# Proposal for Mooer Audio Engineering: Open Development Scripting Engine & AI Prompt-to-DSP Architecture

**Author / Contributor**: DSP & AI Audio Developer
**Target Ecosystem**: Mooer GE Series (GE100, GE200, GE300, GE1000) & Mooer Studio Software

---

## Executive Summary

To position Mooer multi-effects units at the forefront of modern guitar pedal innovation, we propose introducing **Open Dev Scripting** and an **AI Prompt-to-DSP Generator** into the Mooer Studio suite. 

By integrating a lightweight, real-time Audio DSL (Domain Specific Language) like **Faust** or **Cmajor**, guitarists and developers can create custom DSP effect blocks using mathematical scripts. These scripts compile down to raw C/assembly math that runs directly inside the pedal's DSP engine at native speed.

Furthermore, leveraging generative AI (LLMs), users can simply type a plain-language prompt (e.g., *"Create a vintage 1960s Fender harmonic tremolo with warm tube saturation and envelope speed control"*), and the software will write, validate, compile, and push the custom DSP block straight into a dedicated user slot on the Mooer pedal.

---

## 1. Technical Architecture: Open Development Scripting Language

### Recommended Scripting Languages
We recommend two industry-standard real-time audio languages suitable for pedal DSP engines:

1. **Faust (Functional Audio Stream)** - *Recommended for Pure Math Simplicity*
   - **Why Faust?**: Faust is a purely functional DSP language designed specifically for real-time audio processing.
   - **Performance**: The Faust compiler generates highly optimized, zero-dependency C/C++ code. It has zero runtime overhead or garbage collection pauses.
   - **Example Harmonic Tremolo Script in Faust**:
```faust
import("stdfaust.lib");

rate = hslider("Rate[unit:Hz]", 3.5, 0.1, 20.0, 0.01);
depth = hslider("Depth", 0.85, 0.0, 1.0, 0.01);
fc = hslider("Crossover[unit:Hz]", 650.0, 150.0, 3500.0, 1.0);
warmth = hslider("Warmth", 0.35, 0.0, 1.0, 0.01);

// Anti-phase LFO
lfoLow = (os.osc(rate) + 1.0) * 0.5;
lfoHigh = 1.0 - lfoLow;

// SVF Crossover Filter
split = fi.svf.lp(fc, 0.707), fi.svf.hp(fc, 0.707);

// Tube Saturation function
tube(x) = ma.tanh(x * (1.0 + warmth * 2.0));

// Process
process(x) = x : split : (tube, tube) : 
             (*(1.0 - depth * lfoLow), *(1.0 - depth * lfoHigh)) :> _;
```

2. **Cmajor (by Sound Stotts / ROLI)** - *Recommended for Modern C-Style Syntax*
   - Modern procedural syntax resembling C/TypeScript built specifically for audio processors and JIT compilation.

---

## 2. Embedded Real-time Execution Pipeline on Mooer Hardware

```
[ User Script / AI Generated Script ]
                 │
                 ▼
     [ Mooer Studio Editor (PC/Mac/iOS) ]
                 │ (Faust/Cmajor Compiler)
                 ▼
    [ Native C Code / Optimized Bytecode ]
                 │
                 ▼ (USB / Bluetooth Firmware Flash)
[ Mooer Pedal Embedded Memory (User Effect Block Slot) ]
                 │
                 ▼
 [ Real-time Audio Loop: 48kHz Single Sample Process ]
```

### Safety & Stability Rules for Embedded Hardware:
1. **Zero Dynamic Allocation**: Heap memory (`malloc`/`new`) is disallowed inside the process call.
2. **Denormal & NaN Shield**: The compiler automatically wraps mathematical outputs with `std::isnan()` and denormal flushing to prevent DSP locks or ear-piercing audio spikes.
3. **CPU Cycle Limit Checker**: Prior to flashing to the pedal, the compiler measures estimated cycle counts. If the script exceeds the DSP block cycle budget (e.g. 500 instructions per sample at 48kHz), the compiler warns the user to reduce filter order or complexity.

---

## 3. AI Script Writer Feature (Prompt-to-Effect)

### System Concept
Guitarists often know what tone they want in words, but don't know DSP filter math. The **Mooer AI Tone Designer** bridges this gap.

### Example User Interaction:

> **User Prompt**: *"I want a warm harmonic tremolo inspired by Fender brownface amps, but I want the tremolo speed to increase dynamically when I pick harder!"*

### AI Processing Flow:
1. **System Prompt**: An LLM agent (equipped with Faust/Cmajor audio primitives and DSP guidelines) receives the user prompt.
2. **Code Generation**: The AI generates a self-contained DSP script implementing an Envelope Follower driving the LFO rate of a Harmonic Tremolo.
3. **Automated Verification & Compilation**:
   - Compiler verifies syntax correctness.
   - DSP tester runs 1,000 synthetic test samples to verify stability (no clipping, no infinite loops).
4. **Deploy**: The user clicks **"Load to FX Block 3"** in Mooer Studio, and the pedal plays the newly created custom effect instantly!

---

## 4. Immediate Contribution: Harmonic Tremolo C/C++ Codebase

To demonstrate this concept, we have authored a production-ready **Harmonic Tremolo** DSP engine:
- **`include/HarmonicTremoloEngine.hpp`**: Pure C++ realtime DSP class.
- **`embedded/mooer_harmonic_tremolo.h` & `.c`**: Zero-dependency C implementation for immediate inclusion into Mooer firmware builds.
- **`au_plugin/`**: Audio Unit wrapper for DAW testing.

We invite the Mooer R&D and engineering team to evaluate this implementation and test the open development scripting pipeline!
