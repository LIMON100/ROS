# `models/` — accelerator model weights

NPU / accelerator weight files for the perception stack (`human_detector`).

## Provisioning convention

These files are tracked **raw in git** and reach the robot via the standard
deploy step: system imaging copies the repo tree to `/opt/san/`, so a file
committed at `models/<name>` resolves at runtime as **`/opt/san/models/<name>`**.

That `/opt/san/models/` path is what the configs and launch files reference:

| Reference | Path |
|---|---|
| `human_detector/config/human_detector.yaml` → `model_path` / `rk3588_fallback_model_path` | `/opt/san/models/…` |
| `san_bringup/launch/squadron.launch.xml` → `hailo_model_path` | `/opt/san/models/y5s_person_drone.hef` |
| `san_bringup/launch/squadron.launch.xml` → `rk3588_model_path` | `/opt/san/models/yolov5s-640-640_rk3588_251205_640.rknn` |
| `san_bringup/launch/squadron.launch.xml` → `detector_model_path` (sim ONNX) | `/opt/san/models/yolov8m.onnx` *(provision separately — see below)* |

## Tracked files

| File | Backend | Notes |
|---|---|---|
| `yolov5s-640-640_rk3588_251205_640.rknn` | `rk3588` (RKNN, RK3588 NPU) | Real-HW default when `hw_backend=rk3588`. |
| `y5s_person_drone.hef` | `hailo8` (HailoRT, Hailo-8) | Real-HW default when `hw_backend=hailo8`. |

The **simulation ONNX model** (`yolov8m.onnx`, used when `use_sim_time=true`) is
**not** committed here — provide it under `/opt/san/models/` or override
`detector_model_path:=<path>` on the launch CLI. If it is absent the ONNX
backend init fails gracefully and `human_detector_node` falls back to
`rk3588` → `stub` (detection disabled, the rest of the stack still boots).

## Known discrepancy (to reconcile)

`human_detector/config/human_detector.yaml` still names the RKNN model
`yolov8n_640.rknn`, but the file actually shipped here is
`yolov5s-640-640_rk3588_251205_640.rknn`. The squadron launch's
`rk3588_model_path` points at the real shipped file; the package-local
`human_detector.launch.xml` / yaml path should be reconciled (rename the
weight, or point the yaml at the shipped name) under a follow-up DCN.

## git size policy

Model weights exceed the `check-added-large-files --maxkb=500` pre-commit
limit, so `models/*.{rknn,hef,onnx,pt,engine}` are excluded from that hook
(see `.pre-commit-config.yaml`). The exclusion is deliberate: it keeps the
raw-tracking convention working **without** forcing `git commit --no-verify`,
which would silently skip *every* hygiene hook. If the project later moves
these to Git LFS, add the matching `filter=lfs` entries to `.gitattributes`
and drop the exclude.
