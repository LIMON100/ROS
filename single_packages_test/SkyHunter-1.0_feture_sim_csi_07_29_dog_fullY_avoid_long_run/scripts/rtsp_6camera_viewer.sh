gst-launch-1.0 -e videomixer name=mix background=1 \
        sink_0::xpos=0   sink_0::ypos=0 \
        sink_1::xpos=1280   sink_1::ypos=0 \
        sink_2::xpos=1280 sink_2::ypos=360 \
        sink_3::xpos=0   sink_3::ypos=720 \
        sink_4::xpos=640 sink_4::ypos=720 \
        sink_5::xpos=1280 sink_5::ypos=720 \
    ! fpsdisplaysink sync=false \
rtspsrc location=rtsp://192.168.100.10:11000/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=1280,height=720 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    !  mix.sink_0 \
rtspsrc location=rtsp://192.168.100.10:11001/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=640,height=360 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    !  mix.sink_1 \
rtspsrc location=rtsp://192.168.100.10:11002/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=640,height=360 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    !  mix.sink_2 \
rtspsrc location=rtsp://192.168.100.10:11003/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=640,height=360 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    !  mix.sink_5 \
rtspsrc location=rtsp://192.168.100.10:11004/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=640,height=360 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    !  mix.sink_3 \
rtspsrc location=rtsp://192.168.100.10:11005/test latency=0 \
    ! decodebin \
    ! videoscale   ! video/x-raw,width=640,height=360 \
    ! videobox  top=-1 bottom=-1 left=-1 right=-1 fill=5 border-alpha=0 \
    ! queue max-size-buffers=3 ! mix.sink_4