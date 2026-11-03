#!/bin/sh
set -eu
# Check the exact PPSSPP format supplied by the user. No Python needed.
FILE="TTT_CAMERA_FOV05/plugin.ini"
PRX="ttt_camera_fov05.prx"
[ -f "$FILE" ]
grep -Fx '[options]' "$FILE" >/dev/null
grep -Fx 'version = 1' "$FILE" >/dev/null
grep -Fx 'type = prx' "$FILE" >/dev/null
grep -Fx "filename = $PRX" "$FILE" >/dev/null
grep -Fx '[games]' "$FILE" >/dev/null
grep -Fx 'ULUS10537 = true' "$FILE" >/dev/null
if [ "${1:-}" = '--check-prx' ]; then
  [ -s "$PRX" ]
fi
printf '%s\n' '[OK] plugin.ini corresponde ao nome do PRX e ao jogo ULUS10537.'
