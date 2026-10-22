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

## License

This project is licensed under the MIT License. See `LICENSE`. You may modify it and use it in your own projects.
