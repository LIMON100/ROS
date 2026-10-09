# camera 0
/home/nvidia/gst-rtsp-server/examples/test-launch  "( 
    v4l2src device=/dev/video0 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11000 & > /dev/null 2>&1

# camera 1
/home/nvidia/gst-rtsp-server/examples/test-launch  "( v4l2src device=/dev/video1 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11001 & > /dev/null 2>&1

# camera 2
/home/nvidia/gst-rtsp-server/examples/test-launch  "( v4l2src device=/dev/video2 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11002 & > /dev/null 2>&1

# camera 3
/home/nvidia/gst-rtsp-server/examples/test-launch  "( v4l2src device=/dev/video3 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11003 & > /dev/null 2>&1

# camera 4
/home/nvidia/gst-rtsp-server/examples/test-launch  "( v4l2src device=/dev/video4 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11004 & > /dev/null 2>&1

# camera 5
/home/nvidia/gst-rtsp-server/examples/test-launch  "( v4l2src device=/dev/video5 io-mode=auto ! video/x-raw, width=1920, heigth=1080 ! nvvidconv ! nvv4l2h265enc bitrate=2000000 iframeinterval=300 vbv-size=33333 insert-sps-pps=true control-rate=constant_bitrate profile=Main num-B-Frames=0 ratecontrol-enable=true preset-level=UltraFastPreset EnableTwopassCBR=false maxperf-enable=true ! h265parse ! rtph265pay name=pay0 )" -p 11005 & > /dev/null 2>&1
