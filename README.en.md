# Rasterizer from Scratch in C++

This repository contains the code for the video series **Rasterizer from Scratch in C++** on the Rewheel Lab YouTube channel. We build a software rasterizer without a GPU, starting with drawing points and working up to a 3D world with shadows, transparency, and realistic materials.

## Requirements

- Visual Studio Community with the **Desktop development with C++** workload installed

No other tools or libraries are needed.

## How to run the code

1. Click the green **Code** button on this page and choose **Download ZIP**.
2. Right-click the downloaded ZIP file and choose **Extract All**.
3. Open the folder for the episode you want to watch, then double-click its `.sln` file.
4. When Visual Studio opens, press **F5**.

## Episode folders

Each episode has two folders:

- `epXX_start` contains the code from the beginning of the episode. Follow along with the video and build the program from here.
- `epXX_done` contains the code from the end of the episode. Compare your code with this folder if something does not work.

If you get stuck in one episode, you can continue from the `_start` folder of a later episode.

## Experiment programs for the extra episodes

The `extra/` folder contains the experiment programs used in the extra episodes. Each folder is one Visual Studio project: double-click its `.sln` file and press F5.

`extra/fill_rule` (the extra episode "The Fill Rule") adds two things to the triangle filling of Rasterizer Episode 4: a switch for the rule that decides which triangle paints a pixel that lies exactly on an edge (the fill rule), and a zoomed view. Choose a picture with the 1 to 5 keys, and switch the rule with T (top-left rule), B (bottom-right rule), G (paint only "> 0"), and E (paint ">= 0"). V switches the view: one picture, two pictures side by side, or the differences in red. Z and X zoom in and out, and the arrow keys move the view. C shows the pixel borders and the marks at the pixel centers, W draws the edges of the triangles, and A shows how many times each pixel was painted (pixels painted twice look brighter). Esc quits.

`extra/bresenham` (the extra episode "Bresenham: Drawing Lines with Integers Only") places the line drawing of Rasterizer episode #02 (DDA) next to Bresenham's line algorithm, which draws lines with integers only, and highlights the pixels that differ in red. Keys 1 to 4 switch scenes (1: the #02 fireworks side by side, 2: the same fireworks in one image with the differences in red, 3 and 4: one enlarged line that shows the accumulated error as numbers). V switches the view, and Z, X, and the arrow keys zoom and move. F counts the lines where DDA and Bresenham differ. The full list of keys is in the comment at the top of `main.cpp`.

`extra/matrix_conventions` (the extra episode "OpenGL and DirectX") swaps matrices with different conventions into the Sun, Earth, and Moon of Rasterizer episode #07 to see how the image changes. The matrices are copied from online examples: a right-handed perspective matrix, a translation matrix for row vectors, and a perspective matrix with depth from -1 to 1. Key 1 uses this channel's conventions (the same image as #07), and keys 2 to 7 switch the experiments (2 and 3: the right-handed perspective matrix, 4 and 5: the row-vector matrix and the multiplication order, 6 and 7: the depth of an approaching cube, with depth from 0 to 1 and from -1 to 1). V splits the screen, A shows the world axes, and T prints a table of depth by distance to the console window. Space pauses or resumes, R resets the time to 0, and Esc exits. The full list of keys is in the comment at the top of `main.cpp`.

## License

This project is licensed under the MIT License. See `LICENSE`. You may modify it and use it in your own projects.
