# Native Wordle Clone for Mac OS 9
**Software Architecture & Technical Specification**

---

## 1. Executive Summary & Design Paradigm

This specification outlines the architecture for a **native Wordle clone** engineered specifically for Classic Mac OS (Mac OS 9 / Carbon API). Designed to run fluidly on vintage PowerPC hardware, the application implements the classic 6x5 letter grid, color-coded feedback logic, virtual on-screen keyboard, and local dictionary resource management without external web wrappers or heavy runtime dependencies.

Leveraging Mac OS 9’s **QuickDraw graphics engine**, **Event Manager**, and resource fork storage (`STR#` resources), this project delivers a highly responsive, authentic retro gaming experience adhering to classic Platinum interface guidelines.

---

## 2. System Architecture & Workflows

The game loop handles keyboard input and mouse clicks via low-level Event Manager hooks, updating QuickDraw graphic buffers to redraw tiles instantly with color feedback states.

```
┌─────────────────────────────────────────────────────────┐
│ File  Edit  Game  Help                  Wordle OS 9     │
├─────────────────────────────────────────────────────────┤
│                     [ W ] [ O ] [ R ] [ D ] [ E ]       │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]       │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]       │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]       │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]       │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]       │
│                                                         │
│   Q W E R T Y U I O P     (Green: Correct Spot)         │
│    A S D F G H J K L      (Yellow: Wrong Spot)          │
│     Z X C V B N M  [<-]   (Gray: Not in Word)           │
└─────────────────────────────────────────────────────────┘
```

> **Core Subsystems & Integration Highlights**
> * **QuickDraw Grid Renderer:** Draws the 6x5 matrix using custom rectangle coordinate math, filling tile backgrounds with Classic RGB colors (`RGBColor`) for exact matches (Green), misplaced letters (Yellow), and excluded letters (Gray).
> * **Event & Input Manager:** Captures physical hardware keystrokes (`keyDownEvt`) as well as clicks on the virtual Control Manager on-screen keyboard.
> * **Dictionary Resource Engine:** Loads target and valid guess lists from application resource forks (`STR#`) or localized flat files via the File Manager (`FSOpen`).
> * **Game State & Dialog Manager:** Manages win/loss conditions, triggering native alert dialogs (`Alert` / `StopAlert`) with score summaries and replay options.

---

## 3. Core Features & Functional Specification

### A. The 6x5 Letter Matrix & QuickDraw Rendering
The game board is rendered inside a standard Platinum document window. Each cell is structured as a fixed-size square with a solid black border. When a guess is submitted, QuickDraw color fills (`RGBForeColor`) paint the tiles:
* **Correct Position:** Forest Green (`#6AAA64` / RGB equivalent).
* **Incorrect Position:** Golden Yellow (`#C9B458`).
* **Absent Letter:** Dark Slate Gray (`#787C7E`).

### B. Dual Input Handler (Hardware & Virtual Keyboard)
Input is processed seamlessly across two channels. The Event Manager intercepts alphanumeric key presses for typing and Backspace/Return for submission. Simultaneously, a virtual on-screen QWERTY keyboard rendered at the bottom of the window responds to mouse down events (`mouseDown`), dynamically updating key cap colors as letters are verified.

### C. Word Validation & Dictionary Storage
Before a guess is evaluated, it is checked against a local array of valid 5-letter words. To keep memory footprint minimal on vintage G3/G4 systems, words are compiled directly into application resource forks (`STR#` resource type), allowing instant indexed lookups without sluggish disk reads.

---

## 4. Technical Stack & API Mapping

| Component | Technology / API | Description |
| :--- | :--- | :--- |
| **Graphics & Rendering** | QuickDraw (`RGBForeColor`, `PaintRect`, `DrawText`) | Fast off-screen buffer rendering of game board, tiles, and typography. |
| **Event Handling** | Mac OS Event Manager (`WaitNextEvent`, `keyDownEvt`) | Polling loop for keyboard input, mouse clicks, and window activation events. |
| **UI Controls & Dialogs** | Control Manager & Dialog Manager | Window frames, on-screen buttons, and victory/defeat alert dialogs. |
| **Storage & Resources** | Resource Manager (`Get1IndResource`, `STR#`) | Lightweight local storage for word lists and game dictionaries. |
| **Development Environment** | Metrowerks CodeWarrior C/C++ / REALbasic 5.5 | Standard toolchain for building native PowerPC/68k Carbon or Classic binaries. |

---

## 5. Work Effort & Development Estimates

| Development Phase | Core Tasks | Estimated Time |
| :--- | :--- | :--- |
| **1. Project Setup & Window Framework** | Carbon/Classic application skeleton, event loop, and Platinum window initialization. | 2 – 3 Hours |
| **2. QuickDraw Grid & Tile Engine** | 6x5 matrix layout logic, text drawing algorithms, and color-coded state painting. | 3 – 4 Hours |
| **3. Input Handler & Validation Logic** | Hardware keyboard capture, virtual keyboard mapping, and letter-matching evaluation engine. | 4 – 5 Hours |
| **4. Dictionary Integration & Polish** | Resource fork dictionary loading (`STR#`), win/loss dialogs, and stats tracking. | 2 – 3 Hours |
| **TOTAL ESTIMATED EFFORT** | **Complete native, standalone Mac OS 9 Wordle Clone** | **~11 – 15 Hours** |
