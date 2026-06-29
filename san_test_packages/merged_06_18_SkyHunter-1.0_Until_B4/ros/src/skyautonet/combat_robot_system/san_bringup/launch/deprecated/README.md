# Deprecated Python Launch Files (DCN-2026-014)

These `.launch.py` files were superseded by the matching `.launch.xml`
files in the parent directory on 2026-05-23. Retained for one release
cycle as reference / fallback during the XML conversion rollout.

| Python file | XML replacement | Notes |
| --- | --- | --- |
| `squadron.launch.py` | `../squadron.launch.xml` | Top-level composition. |
| `leader_go2.launch.py` | `../leader_go2.launch.xml` | Pinned wrapper. |
| `hub_sbc1.launch.py` | `../hub_sbc1.launch.xml` | Pinned wrapper (slot 1). |
| `hub_sbc2.launch.py` | `../hub_sbc2.launch.xml` | Pinned wrapper (slot 2). |
| `deputy.launch.py` | `../deputy.launch.xml` | Pinned wrapper. |
| `follower.launch.py` | `../follower.launch.xml` | Pinned wrapper (variable id). |
| `pdr_demo.launch.py` | (no XML replacement yet) | Demo / dev — Gazebo+swarm+operator. Convert when next exercised; not on the v1.5.3 critical path. |

## Why kept (instead of `git rm`)

* Identity-arg pinning is non-trivial; the Python files document the
  decision rationale (`Pin reasoning:` blocks) more thoroughly than
  the XML files can. Keep them readable for one cycle.
* `test_launch_wrappers.py` (also in `test/deprecated/` per this same
  DCN) imported these — keeping the originals lets future archaeology
  reconcile what the pytest used to assert.

## Removal criteria

Delete this directory in a `chore(launch): drop deprecated python
wrappers` commit once ALL of:

* The XML wrappers have passed one full sprint of bench + production use.
* No SOP / runbook still references the `.launch.py` invocation form.
* `pdr_demo.launch.py` has been ported to XML (if still needed) or
  formally retired.

## Refs

* DCN-2026-014 D-050 — RMW unification on FastDDS (forcing function
  for the XML-only launch policy).
* [[ADR-007]] — FastDDS migration ADR.

## ⚠️ Rebuild caveat (post-review HIGH #9)

`colcon build --symlink-install` (which the project uses) writes
symlinks from `install/<pkg>/share/<pkg>/launch/<name>.launch.py`
into the source tree. When this DCN moved the .py files into
`deprecated/`, the OLD symlinks (pointing at the original locations)
remain in `install/.../launch/` until the install tree is cleaned;
the NEW symlinks land under `install/.../launch/deprecated/`. The
result is two entries with the same basename under `share/.../launch/`
and `ros2 launch` errors:

```
file 'squadron.launch.py' was found more than once in the share
directory of package 'san_bringup': [
  '.../launch/squadron.launch.py',
  '.../launch/deprecated/squadron.launch.py'
]
```

If you hit this, do a one-time clean rebuild:

```sh
rm -rf ros/install/san_bringup ros/build/san_bringup
colcon build --packages-select san_bringup --symlink-install
```

A full `rm -rf ros/install ros/build ros/log` is also fine and avoids
having to remember the package name.
