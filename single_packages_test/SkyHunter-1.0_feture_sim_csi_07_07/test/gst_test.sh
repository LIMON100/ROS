#!/bin/bash
gst-launch-1.0 v4l2src device=/dev/video3 ! video/x-raw, format=NV12, width=1280, height=720, framerate=30/1 ! autovideosink