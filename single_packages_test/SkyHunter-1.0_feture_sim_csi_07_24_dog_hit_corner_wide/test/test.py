#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Dual camera recorder (GStreamer, Rockchip-friendly)
- cam1: /dev/video3 (NV12 1280x720@30)
- cam0: /dev/video22 (YUY2 640x512@30)
- 미리보기 + 동시 저장(MP4)
- 터미널에서 'q' → EOS 전송 후 안전 종료

예)
  mkdir -p Normal
  python3 test.py --outdir Normal --bps 2000000 --dev1 /dev/video3 --dev0 /dev/video22
  # 미리보기 없이 저장만
  python3 test.py --outdir Normal --bps 2000000 --dev1 /dev/video3 --dev0 /dev/video22 --no-preview
  # (성능 모드) DMABuf zero-copy 시도
  python3 test.py --outdir Normal --bps 2000000 --dev1 /dev/video3 --dev0 /dev/video22 --fast-dmabuf
"""

import argparse
import os
import sys
import time
import threading
from datetime import datetime

import gi
gi.require_version("Gst", "1.0")
from gi.repository import Gst, GLib


def ts() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def plugin_exists(name: str) -> bool:
    return Gst.ElementFactory.find(name) is not None


def build_pipeline_str(
    dev1: str,
    dev0: str,
    bps: int,
    outdir: str,
    no_preview: bool,
    fast_dmabuf: bool,
) -> str:
    out1 = os.path.join(outdir, f"cam1_{ts()}.mp4")  # /dev/video3
    out0 = os.path.join(outdir, f"cam0_{ts()}.mp4")  # /dev/video22

    # 프리뷰 싱크 (FPS 오버레이 제거: 모두 autovideosink로 통일)
    preview1 = "fakesink sync=false" if no_preview else "videoconvert ! videoscale ! autovideosink sync=false"
    preview0 = "fakesink sync=false" if no_preview else "videoconvert ! videoscale ! autovideosink sync=false"

    # cam 포맷/스케일 변환: rkvideoconvert 선호
    if plugin_exists("rkvideoconvert"):
        conv_main = "rkvideoconvert"
    else:
        conv_main = "videoconvert ! videoscale"

    cam1 = ""
    cam0 = ""

    if dev1.lower() != "none":
        # 소스 캡에서 format과 framerate를 제거하여 카메라가 지원하는 형식을 스스로 찾게 합니다.
        # 1920x1280은 비표준일 수 있어 1920x1080으로 우선 시도합니다.
        if fast_dmabuf:
            cam1 = (
                f"v4l2src device={dev1} io-mode=dmabuf do-timestamp=true ! "
                "video/x-raw,width=1920,height=1080 ! "
                f"queue ! {conv_main} ! video/x-raw,width=1280,height=720 ! "
                "queue ! tee name=t1 "
                f"t1. ! queue leaky=downstream max-size-buffers=1 ! videoconvert ! videoscale ! autovideosink sync=false "
                "t1. ! queue ! videoconvert ! video/x-raw,format=NV12,width=1280,height=720 ! "
                f"mpph264enc bps={bps} ! h264parse config-interval=1 ! mp4mux ! "
                f"filesink location={out1} "
            )
        else:
            cam1 = (
                f"v4l2src device={dev1} io-mode=mmap do-timestamp=true ! "
                "video/x-raw,width=1920,height=1080 ! "
                f"queue ! {conv_main} ! video/x-raw,width=1280,height=720 ! "
                "queue ! tee name=t1 "
                f"t1. ! queue leaky=downstream max-size-buffers=1 ! videoconvert ! videoscale ! autovideosink sync=false "
                "t1. ! queue ! videoconvert ! "
                "video/x-raw,format=NV12,width=1280,height=720 ! "
                f"mpph264enc bps={bps} ! h264parse config-interval=1 ! mp4mux ! "
                f"filesink location={out1} "
            )

    if dev0.lower() != "none":
        if fast_dmabuf:
            cam0 = (
                f"v4l2src device={dev0} io-mode=dmabuf do-timestamp=true ! "
                "video/x-raw,format=YUY2,width=640,height=512,framerate=30/1 ! "
                f"queue ! {conv_main} ! "
                "video/x-raw,format=NV12,width=640,height=512,framerate=30/1 ! "
                "queue ! tee name=t0 "
                f"t0. ! queue leaky=downstream max-size-buffers=1 ! videoconvert ! videoscale ! autovideosink sync=false "
                f"t0. ! queue ! mpph264enc bps={bps} ! h264parse config-interval=1 ! mp4mux ! "
                f"filesink location={out0} "
            )
        else:
            cam0 = (
                f"v4l2src device={dev0} io-mode=mmap do-timestamp=true ! "
                "video/x-raw,format=YUY2,width=640,height=512,framerate=30/1 ! "
                f"queue ! {conv_main} ! video/x-raw,format=NV12,width=640,height=512,framerate=30/1 ! "
                "queue ! tee name=t0 "
                f"t0. ! queue leaky=downstream max-size-buffers=1 ! videoconvert ! videoscale ! autovideosink sync=false "
                f"t0. ! queue ! mpph264enc bps={bps} ! h264parse config-interval=1 ! mp4mux ! "
                f"filesink location={out0} "
            )

    return " ".join([cam1, cam0]).strip()


def main() -> int:
    ap = argparse.ArgumentParser(description="Dual camera preview & MP4 record (GStreamer, mpph264enc)")
    ap.add_argument("--dev1", default="/dev/video3", help="cam1 디바이스(기본: /dev/video3, NV12 1280x720@30)")
    ap.add_argument("--dev0", default="none", help="cam0 디바이스(기본: /dev/video22, YUY2 640x512@30)")
    ap.add_argument("--bps", type=int, default=2_000_000, help="mpph264enc bps (기본 2,000,000)")
    ap.add_argument("--outdir", default="Normal", help="출력 폴더 (기본: Normal)")
    ap.add_argument("--no-preview", action="store_true", help="미리보기 끄기(fakesink)")
    ap.add_argument("--fast-dmabuf", action="store_true", help="DMABuf zero-copy 경로 시도(환경에 따라 실패 가능)")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    Gst.init(None)

    pipeline_str = build_pipeline_str(
        args.dev1, args.dev0, args.bps, args.outdir, args.no_preview, args.fast_dmabuf
    )
    print("[PIPELINE]", pipeline_str)

    try:
        pipeline = Gst.parse_launch(pipeline_str)
    except Exception as e:
        print("[ERROR] 파이프라인 생성 실패:", e)
        return 2

    loop = GLib.MainLoop()

    bus = pipeline.get_bus()
    bus.add_signal_watch()

    def on_message(bus, msg):
        t = msg.type
        if t == Gst.MessageType.ERROR:
            err, dbg = msg.parse_error()
            print("[ERROR]", err, dbg)
            try:
                pipeline.set_state(Gst.State.NULL)
            except Exception:
                pass
            loop.quit()
        elif t == Gst.MessageType.EOS:
            print("[INFO] EOS 수신: 파일 마감 완료.")
            try:
                pipeline.set_state(Gst.State.NULL)
            except Exception:
                pass
            loop.quit()
        return True

    bus.connect("message", on_message)

    def wait_q_then_eos():
        print("[INFO] 녹화 시작. 터미널에서 q 키를 누르면 저장 후 종료합니다.")
        try:
            if os.name == "nt":
                import msvcrt
                while True:
                    if msvcrt.kbhit():
                        ch = msvcrt.getch()
                        if ch in (b"q", b"Q"):
                            break
                    time.sleep(0.05)
            else:
                import termios, tty, select
                fd = sys.stdin.fileno()
                old = termios.tcgetattr(fd)
                try:
                    tty.setcbreak(fd)
                    while True:
                        r, _, _ = select.select([sys.stdin], [], [], 0.1)
                        if r:
                            ch = sys.stdin.read(1)
                            if ch in ("q", "Q"):
                                break
                finally:
                    termios.tcsetattr(fd, termios.TCSADRAIN, old)
        except Exception as e:
            print("[WARN] 키보드 입력 감시 실패:", e, "대신 Ctrl+C 로 종료하세요.")
            return
        print("[INFO] 'q' 입력: EOS 전송 중...")
        pipeline.send_event(Gst.Event.new_eos())

    th = threading.Thread(target=wait_q_then_eos, daemon=True)
    th.start()

    ret = pipeline.set_state(Gst.State.PLAYING)
    if ret == Gst.StateChangeReturn.FAILURE:
        print("[ERROR] 파이프라인 재생 실패")
        return 3

    try:
        loop.run()
    except KeyboardInterrupt:
        print("\n[INFO] KeyboardInterrupt: EOS 전송")
        pipeline.send_event(Gst.Event.new_eos())
        loop.run()

    print("[INFO] 종료")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())