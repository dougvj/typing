# Simple Ncurses Typing Test

This is a simply utility to measure typing performance based on text file
input.

![GIF Demo](https://github.com/dougvj/typing/blob/main/demo.gif?raw=true)

## Requirements

Expects a POSIX environment with ncurses support

## Building

Just run `make`

## Usage

`typing <text file name>`

For example, `typing quick_brown_fox.txt`

You can press ctrl+c at any time to quit and see stats. The program
automatically quits to show stats when the entire input is typed

