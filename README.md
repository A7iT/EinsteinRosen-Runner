<div align="center">

# 🌌 Einstein-Rosen Runner

**Fly through five sectors, collect coins, light the beacons, and dive into a wormhole.**

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![OpenGL](https://img.shields.io/badge/OpenGL-FreeGLUT-5586A4?logo=opengl&logoColor=white)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

<!-- Replace with a real screenshot or GIF:
<img src="screenshots/gameplay.png" alt="Einstein-Rosen Runner gameplay" width="800">
-->

</div>

---

A 3D space game written in C++ with FreeGLUT and fixed-function OpenGL. Pilot a spaceship through five procedurally laid-out sectors. Complete each sector's goals to unseal an **Einstein-Rosen bridge** (a wormhole) that sucks you in and spits you out in the next sector.

## 📑 Table of Contents

- [Gameplay](#-gameplay)
- [Controls](#-controls)
- [Getting Started](#-getting-started)
- [Troubleshooting](#-troubleshooting)
- [Customizing the Game](#-customizing-the-game)
- [Technical Notes](#-technical-notes)
- [License](#-license)

## 🎮 Gameplay

Each sector has three goals:

- [ ] **Collect coins** (about 70% of the coins in the sector)
- [ ] **Activate every beacon** by flying close to it
- [ ] **Enter the bridge**, which stays sealed until the first two goals are done

While sealed, the bridge glows red and pushes you away. Once it opens, its gravity field pulls you in, you spiral into the mouth, travel through the wormhole tunnel, and emerge in the next sector. Clear all **5 sectors** to win.

### ❤️ Health and invincibility

| Rule | Detail |
|------|--------|
| Starting health | 3 hull points |
| Obstacle hit | Costs 1 hull point and knocks the ship back |
| After a hit | **3 seconds of invincibility**: the ship blinks, a shield bubble appears, and you fly straight through obstacles |
| After 3 seconds | The ship is vulnerable again |
| 0 hull points | Ship destroyed, game over |

### ☄️ Obstacles

| Obstacle | Behaviour |
|----------|-----------|
| **Asteroids** | Drifting, spinning rocks |
| **Mines** | Spiked spheres with a pulsing red core that explode when hit |
| **Orbiters** | Glowing rocks circling a point, with a faint ring showing their path |

The number of obstacles increases every sector.

### 🏆 Scoring

| Action | Points |
|--------|--------|
| Coin | +100 |
| Beacon | +500 |
| Sector complete | +1000 × sector number |
| Hull remaining at the end of the game | +500 per hull point |

### 🪐 Sectors

| # | Sector | Centerpiece |
|---|--------|-------------|
| 1 | Azure Drift | Ringed planet |
| 2 | Ember Reach | Star |
| 3 | Verdant Veil | Ringed planet |
| 4 | Violet Abyss | Star |
| 5 | Event Horizon | Black hole with accretion disk |

### 🖥️ HUD

- Objectives panel with checkboxes and distance to the next target
- Hull hearts and an invincibility timer bar
- Radar showing coins, beacons, obstacles, and the bridge
- Speed bar, floating score popups, and warnings (gravity field, sealed bridge, leaving the sector)

## ⌨️ Controls

| Key | Action |
|-----|--------|
| <kbd>W</kbd> / <kbd>S</kbd> | Thrust forward / brake |
| <kbd>A</kbd> / <kbd>D</kbd> or <kbd>←</kbd> / <kbd>→</kbd> | Turn |
| <kbd>R</kbd> / <kbd>F</kbd> or <kbd>↑</kbd> / <kbd>↓</kbd> | Pitch up / down |
| <kbd>Q</kbd> / <kbd>E</kbd> | Roll |
| <kbd>Space</kbd> | Boost |
| <kbd>Enter</kbd> | Start, or restart from the end screens |
| <kbd>P</kbd> | Pause |
| <kbd>X</kbd> | Restart the game |
| <kbd>Esc</kbd> | Quit |

## 🚀 Getting Started

The game is a single source file, `er_runner.cpp`. You need a C++17 compiler, OpenGL, GLU, and FreeGLUT.

```bash
git clone https://github.com/<your-username>/<your-repo>.git
cd <your-repo>
```

### Windows (MSYS2 MinGW64)

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 MINGW64** terminal and install the toolchain:

   ```bash
   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut
   ```

3. Build:

   ```bash
   g++ -std=c++17 -O2 er_runner.cpp -o er_runner -lfreeglut -lopengl32 -lglu32
   ```

4. Run `./er_runner.exe`.

> [!IMPORTANT]
> To run the game from PowerShell or VS Code, add `C:\msys64\mingw64\bin` to your Windows `PATH` so `freeglut.dll` can be found, then restart the terminal.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential freeglut3-dev
g++ -std=c++17 -O2 er_runner.cpp -o er_runner -lglut -lGLU -lGL
./er_runner
```

### VS Code

<details>
<summary>Click to expand the <code>.vscode/tasks.json</code></summary>

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build ER Runner",
      "type": "shell",
      "command": "g++",
      "args": [
        "-std=c++17", "-O2",
        "${workspaceFolder}/er_runner.cpp",
        "-o", "${workspaceFolder}/er_runner.exe",
        "-lfreeglut", "-lopengl32", "-lglu32"
      ],
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": ["$gcc"]
    }
  ]
}
```

Press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> to build.

</details>

## 🛠️ Troubleshooting

<details>
<summary><code>g++</code> is not recognized</summary>

The MinGW `bin` folder is not on your `PATH`. Add `C:\msys64\mingw64\bin`, then restart your terminal and VS Code.

</details>

<details>
<summary><code>cannot find -lfreeglut</code></summary>

FreeGLUT is not installed. Re-run the `pacman` command above in the **MSYS2 MINGW64** terminal (not PowerShell).

</details>

<details>
<summary>Missing <code>freeglut.dll</code> when launching</summary>

The MinGW `bin` folder is not on your `PATH`.

</details>

<details>
<summary>Low frame rate</summary>

The game uses immediate-mode OpenGL, so it benefits from a dedicated GPU. Make sure it is not running on a software renderer.

</details>

## 🎛️ Customizing the Game

Most gameplay values are easy to change in `er_runner.cpp`:

| Where | What you can change |
|-------|---------------------|
| Constants at the top | `INVULN_TIME`, `START_HEALTH`, `GRAV_RADIUS`, `LOCK_DIST`, `SEG_LEN`, `MAX_LEVEL` |
| `buildLevel()` | Coin patterns, beacon count, obstacle counts per sector |
| `themes[]` | Colours and centerpiece for each sector |

## 🔧 Technical Notes

- C++17 with FreeGLUT and fixed-function OpenGL (no shaders)
- Sector layouts are generated procedurally from a fixed seed per sector, so each sector is identical every run
- Movement uses delta time, so gameplay speed stays consistent across frame rates
- Effects include billboarded glows, additive blending, fog, a nebula sky, and a custom wormhole tunnel scene

## 📄 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
