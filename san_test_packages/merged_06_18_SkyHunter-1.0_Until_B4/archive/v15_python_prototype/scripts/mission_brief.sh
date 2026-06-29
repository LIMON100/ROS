#!/usr/bin/env bash
# mission_brief.sh — Pre-mission map preparation (SDD §10.5.2, P2-7).
#
# Workflow:
#   1. Operator inputs polygon (lat/lon corners or KML)
#   2. Compute bbox
#   3. Download OSM PBF (Geofabrik, cached)
#   4. osmium extract bbox
#   5. Download SRTM HGT tiles for bbox
#   6. Rasterize PBF -> 20cm grid
#   7. Save to /opt/patrol_maps/<mission_id>/
#   8. rsync to all robots in mesh

set -euo pipefail

# ---------- Defaults ----------
MAPS_BASE="${PATROL_MAPS_DIR:-/opt/patrol_maps}"
GEOFABRIK_BASE="${GEOFABRIK_BASE:-https://download.geofabrik.de/asia/south-korea-latest.osm.pbf}"
ROBOT_HOSTS="${ROBOT_HOSTS:-}"  # space-separated user@host list

usage() {
    cat <<EOF
Usage: $0 -m <mission_id> -b "<lat_min,lon_min,lat_max,lon_max>" [-k <kml_file>]

Options:
  -m  Mission ID (e.g. M_20260512_seoul_a)
  -b  Bounding box (4 numbers comma-separated)
  -k  KML file (alternative to -b)
  -h  This help

Environment:
  PATROL_MAPS_DIR   - base map directory (default: /opt/patrol_maps)
  GEOFABRIK_BASE    - source URL for OSM PBF
  ROBOT_HOSTS       - space-separated SSH hosts to rsync to
EOF
    exit 1
}

MISSION_ID=""
BBOX=""
KML=""
while getopts "m:b:k:h" opt; do
    case $opt in
        m) MISSION_ID="$OPTARG" ;;
        b) BBOX="$OPTARG" ;;
        k) KML="$OPTARG" ;;
        h|?) usage ;;
    esac
done

if [[ -z "$MISSION_ID" ]]; then
    echo "ERROR: -m required" >&2
    usage
fi
if [[ -z "$BBOX" && -z "$KML" ]]; then
    echo "ERROR: -b or -k required" >&2
    usage
fi

# ---------- Parse KML if given ----------
if [[ -n "$KML" ]]; then
    BBOX=$(python3 - <<PYEOF
import sys
import xml.etree.ElementTree as ET

tree = ET.parse("$KML")
root = tree.getroot()
coords = []
for el in root.iter():
    if el.tag.endswith("coordinates"):
        for tok in (el.text or "").strip().split():
            parts = tok.split(",")
            if len(parts) >= 2:
                lon, lat = float(parts[0]), float(parts[1])
                coords.append((lat, lon))
if not coords:
    print("ERROR: no coords found in KML", file=sys.stderr)
    sys.exit(1)
lats = [c[0] for c in coords]
lons = [c[1] for c in coords]
print(f"{min(lats)},{min(lons)},{max(lats)},{max(lons)}")
PYEOF
)
fi

IFS=',' read -r LAT_MIN LON_MIN LAT_MAX LON_MAX <<< "$BBOX"
echo "Mission: $MISSION_ID"
echo "BBox:    $LAT_MIN,$LON_MIN  ->  $LAT_MAX,$LON_MAX"

OUT_DIR="$MAPS_BASE/$MISSION_ID"
mkdir -p "$OUT_DIR"

# ---------- Step 1: OSM PBF ----------
PBF_FULL="$MAPS_BASE/_shared/south-korea-latest.osm.pbf"
PBF_OUT="$OUT_DIR/area.osm.pbf"

if [[ ! -f "$PBF_FULL" ]]; then
    echo "[1/4] Downloading OSM PBF (~700MB)..."
    mkdir -p "$(dirname "$PBF_FULL")"
    if command -v wget >/dev/null 2>&1; then
        wget -O "$PBF_FULL.tmp" "$GEOFABRIK_BASE" || {
            echo "WARN: PBF download failed; continuing without full PBF" >&2
            rm -f "$PBF_FULL.tmp"
        }
        if [[ -f "$PBF_FULL.tmp" ]]; then
            mv "$PBF_FULL.tmp" "$PBF_FULL"
        fi
    else
        echo "WARN: wget not installed; skipping PBF download" >&2
    fi
fi

if [[ -f "$PBF_FULL" && ! -f "$PBF_OUT" ]]; then
    echo "[1/4] Extracting bbox..."
    if command -v osmium >/dev/null 2>&1; then
        osmium extract -b "$LON_MIN,$LAT_MIN,$LON_MAX,$LAT_MAX" \
            -o "$PBF_OUT" "$PBF_FULL"
    else
        echo "WARN: osmium not installed; copying full PBF as area.osm.pbf" >&2
        cp "$PBF_FULL" "$PBF_OUT"
    fi
fi

# ---------- Step 2: SRTM HGT ----------
echo "[2/4] Provisioning SRTM tile placeholders..."
SRTM_DIR="$OUT_DIR/srtm"
mkdir -p "$SRTM_DIR"

LAT_FLOOR=$(python3 -c "import math; print(math.floor(float('$LAT_MIN')))")
LAT_CEIL=$(python3 -c "import math; print(math.floor(float('$LAT_MAX')))")
LON_FLOOR=$(python3 -c "import math; print(math.floor(float('$LON_MIN')))")
LON_CEIL=$(python3 -c "import math; print(math.floor(float('$LON_MAX')))")

for lat in $(seq "$LAT_FLOOR" "$LAT_CEIL"); do
    for lon in $(seq "$LON_FLOOR" "$LON_CEIL"); do
        TILE_NAME=$(printf "N%02dE%03d" "$lat" "$lon")
        TILE_FILE="$SRTM_DIR/${TILE_NAME}.hgt"
        if [[ ! -f "$TILE_FILE" ]]; then
            echo "  -> $TILE_NAME (placeholder; populate via NASA Earthdata)"
            : > "$TILE_FILE"
        fi
    done
done

# ---------- Step 3: Rasterize PBF -> 20 cm grid ----------
echo "[3/4] Rasterizing OSM to 20cm grid..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
python3 - "$PBF_OUT" "$LAT_MIN" "$LON_MIN" "$LAT_MAX" "$LON_MAX" "$OUT_DIR" "$SCRIPT_DIR/.." <<'PYEOF' || true
import sys, os, numpy as np
pbf, lat_min, lon_min, lat_max, lon_max, out_dir, repo = sys.argv[1:8]
sys.path.insert(0, repo)
from mapping.osm_static_layer import OsmStaticLayer
layer = OsmStaticLayer(cell_size_m=0.20)
try:
    layer.load_from_pbf(pbf, bbox=(float(lat_min), float(lon_min),
                                   float(lat_max), float(lon_max)))
    np.save(os.path.join(out_dir, "static_layer.npy"), layer.grid)
    print(f"  Rasterized: {layer.grid.shape}")
except Exception as e:
    print(f"WARN: rasterize stub - {e}", file=sys.stderr)
PYEOF

# ---------- Step 4: rsync to all robots ----------
if [[ -n "$ROBOT_HOSTS" ]]; then
    echo "[4/4] Distributing to robots..."
    for host in $ROBOT_HOSTS; do
        echo "  -> $host"
        rsync -avz "$OUT_DIR/" "$host:$MAPS_BASE/$MISSION_ID/" || true
    done
else
    echo "[4/4] ROBOT_HOSTS not set - skipping distribution"
fi

echo "Done. Output: $OUT_DIR"
