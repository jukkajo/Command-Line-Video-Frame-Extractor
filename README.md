# Frame Extractor

A command-line utility for extracting frames as JPEG images from a video file using FFmpeg libraries. It saves every Nth frame within a specified time range.

## Prerequisites

To compile and run this program, you need the FFmpeg development libraries installed:

- **Ubuntu/Debian**: `sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev`

## Compilation

To compile the program:

$ make

## Usage

./frame_extractor <input_file> <start_time> <end_time> <nth-frame>

e.g ./frame_extractor video.mp4 1 10 5

