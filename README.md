# Photo-Realistic-Rendering-Competition-
Photo realistic rendering competition for COMP4610/8610. 

## Compile Instructions

### Linux / Windows / macOS

```bash
mkdir build
cd build
cmake ..
make
```

### macOS (Apple Silicon)
Install dependencies
```
brew install opencv libomp
```

Then compile
```
mkdir build
cd build
cmake ..
make
```

## Run Instructions

Run the renderer:

```
./RayTracing
```

The renderer launches an interactive setup mode.

You may press Enter to keep defaults or enter custom values.

Example prompts:

```
Interactive render setup
(Press Enter to keep default. For 3D values, enter: x y z)

Samples per pixel [default 256]:
Diamond position x y z [default 350 0 250]:
Diamond rotation x y z [default -12 25 18]:
Number of lights [default 2]:
Main light position x y z [default 0 -5 0]:
Camera position x y z [default 278 273 -800]:
Camera look-at x y z [default 278 120 280]:
Camera FOV [default 40]:
```

Example input:

```
256
350 0 250
-12 25 18
2
0 -5 0
278 273 -800
278 120 280
40
```

Pressing Enter uses the default values.

## Output
Rendered images are saved automatically as ``binary_task2.ppm`` in the `build` folder.
