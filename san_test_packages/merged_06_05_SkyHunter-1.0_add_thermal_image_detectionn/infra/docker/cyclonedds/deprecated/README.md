# Deprecated Middleware Configurations (DCN-2026-014 D-053)

Retained for emergency rollback. **Active middleware: FastDDS**
(`rmw_fastrtps_cpp`, see `docs/05_Supplementary/ADR-007-rmw-fastrtps-migration.md`).

These XMLs are the CycloneDDS profiles introduced under DCN-2026-004
D-010 to restrict SPDP multicast on the LTE interface (`ppp0`/`wwan0`)
and to seed an explicit `<Peers>` unicast fallback list.

| File | Used by (pre-2026-05) |
| --- | --- |
| `cyclonedds_legacy_sbc1.xml` | Hub SBC #1 (primary, DCN-2026-011 D-032 slot 1) |
| `cyclonedds_legacy_sbc2.xml` | Hub SBC #2 (secondary, expanded `<Peers>`) |

## Rollback

If FastDDS regresses in field testing and a hot rollback is needed:

```sh
# 1. Stop all robots
sudo systemctl stop 'skyautonet-*.service'

# 2. Edit /etc/skyautonet/<robot>.env on each board:
#    Replace the FastDDS block with the CycloneDDS block below.
#    Choose the matching SBC slot file for Hub robots.
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
CYCLONEDDS_URI=file:///opt/san/infra/docker/cyclonedds/deprecated/cyclonedds_legacy_sbc1.xml
# or _sbc2.xml on the secondary Hub SBC.
# Unset the FastDDS pointer so it can't fight the CycloneDDS one.
unset FASTRTPS_DEFAULT_PROFILES_FILE

# 3. Restart and verify
sudo systemctl daemon-reload
sudo systemctl start 'skyautonet-*.service'
ros2 doctor --report | grep -i 'middleware'   # expect: rmw_cyclonedds_cpp
sudo tcpdump -i wlan0 udp port 7400           # SPDP traffic visible
```

(System imaging copies the source tree to `/opt/san/`. Adjust the path
if your deployment uses a different prefix.)

## Why these files are kept (instead of `git rm`)

A FastDDS regression severe enough to need rollback is also a regression
severe enough that you do not want to be `git checkout`-ing the old
files from history under pressure. Keep them present-but-deprecated for
one full release cycle past the migration.

## Removal criteria

Drop this directory in a `chore(rmw): remove deprecated cyclonedds
fallback` commit when ALL of the following hold for one full sprint
beyond the v1.5.3 GA tag:

* No `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` invocations in production
  systemd, infra/docker, or field-deployment runbooks.
* KPP-1 latency baseline stable on FastDDS (no rollback discussion in
  the prior sprint review).
* `ros2 doctor --report` clean on all 5 robots after a full power cycle.

Until then leave it as a recovery option.

## See also

* `docs/05_Supplementary/ADR-007-rmw-fastrtps-migration.md` — decision record
  (accepted 2026-05-26).
* commit `48b0cef` — the Gong fix that switched `combat_nav2` first.
* DCN-2026-014 — sprint ticket for the full migration.
* DCN-2026-004 D-010 — the original CycloneDDS LTE-restriction work.
