"""Camera intrinsic calibration — chessboard pattern (P2-12).

Procedure:
1. Show a chessboard (9×6 inner corners) at multiple angles/distances
2. Capture 20+ images
3. Compute K matrix + distortion coefficients
4. Output /etc/patrol/calibration/camera.yaml
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import yaml

try:
    import cv2
    import numpy as np
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False


CHESSBOARD_INNER_CORNERS = (9, 6)
SQUARE_SIZE_MM = 25.0


def _stub_result(image_count: int) -> dict:
    return {
        "calibration_ts": time.time(),
        "image_count": image_count,
        "K": [[600.0, 0.0, 320.0],
              [0.0, 600.0, 240.0],
              [0.0, 0.0, 1.0]],
        "distortion": [0.0, 0.0, 0.0, 0.0, 0.0],
        "rms_error_px": 0.5,
        "stub_mode": True,
    }


def calibrate_from_images(image_paths) -> dict:
    if not CV2_AVAILABLE:
        return _stub_result(len(image_paths))

    objp = np.zeros((CHESSBOARD_INNER_CORNERS[0]
                     * CHESSBOARD_INNER_CORNERS[1], 3), np.float32)
    objp[:, :2] = np.mgrid[0:CHESSBOARD_INNER_CORNERS[0],
                           0:CHESSBOARD_INNER_CORNERS[1]].T.reshape(-1, 2)
    objp *= SQUARE_SIZE_MM

    objpoints, imgpoints = [], []
    gray = None
    for path in image_paths:
        img = cv2.imread(str(path))
        if img is None:
            continue
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        ret, corners = cv2.findChessboardCorners(
            gray, CHESSBOARD_INNER_CORNERS, None)
        if ret:
            objpoints.append(objp)
            imgpoints.append(corners)

    if len(objpoints) < 5 or gray is None:
        # Not enough chessboard images for a real calibration — emit stub.
        return _stub_result(len(image_paths))

    ret, K, dist, _, _ = cv2.calibrateCamera(
        objpoints, imgpoints, gray.shape[::-1], None, None)
    return {
        "calibration_ts": time.time(),
        "image_count": len(image_paths),
        "K": K.tolist(),
        "distortion": dist.flatten().tolist(),
        "rms_error_px": float(ret),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--images-dir", default="/tmp/calib_images")
    parser.add_argument("--output",
                        default="/etc/patrol/calibration/camera.yaml")
    args = parser.parse_args()

    img_dir = Path(args.images_dir)
    img_paths = sorted(img_dir.glob("*.jpg")) + sorted(img_dir.glob("*.png"))
    print(f"[1/2] Found {len(img_paths)} images in {img_dir}")
    if not img_paths:
        # Emit stub even when no images — useful for CI / first-boot dry run.
        img_paths = []

    cal = calibrate_from_images(img_paths)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w") as f:
        yaml.dump(cal, f, default_flow_style=False)
    print(f"[2/2] Wrote {out_path}")
    print(f"  RMS error: {cal['rms_error_px']:.3f} px"
          f"{' (stub)' if cal.get('stub_mode') else ''}")


if __name__ == "__main__":
    main()
