#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  APP_PATH=<dir> scripts/manage_app.sh add <app_name> [desc]
  APP_PATH=<dir> scripts/manage_app.sh del <app_name>
USAGE
  exit 1
}

[[ $# -ge 2 ]] || usage
CMD="$1"
APP="$2"
DESC="${3:-example}"
BASE="${APP_PATH:-.}"

if [[ ! "$APP" =~ ^[a-zA-Z_][a-zA-Z0-9_]*$ ]]; then
  echo "ERROR: invalid app name: $APP (use [a-zA-Z_][a-zA-Z0-9_]*)"
  exit 2
fi

mapfile -t APPH_CANDIDATES < <(find "$BASE" -type f -name 'app.h' 2>/dev/null | sort)
if [[ ${#APPH_CANDIDATES[@]} -eq 0 ]]; then
  echo "ERROR: app.h not found under: $BASE (set APP_PATH correctly)"
  exit 3
fi

choose_app_h() {
  local shortest="" bestlen=0 len
  for p in "${APPH_CANDIDATES[@]}"; do
    len=${#p}
    if [[ -z "$shortest" || $len -lt $bestlen ]]; then
      shortest="$p"; bestlen=$len
    fi
  done
  printf "%s\n" "$shortest"
}

APP_H="$(choose_app_h)"
APP_DIR="$(dirname "$APP_H")"
SRC="$APP_DIR/${APP}.c"

echo "[INFO] using app.h: $APP_H"

sedi() { sed -i "$@"; }

exists_in_header() {
  grep -Fq "\"$APP\"" "$APP_H" || grep -Fq "${APP}_entry" "$APP_H"
}

insert_prototype() {
  local pat='^[[:space:]]*static[[:space:]]+struct[[:space:]]+shell_app[[:space:]]+app_table\['
  if grep -Eq "$pat" "$APP_H"; then
    sedi "/$pat/i void ${APP}_entry(void *arg);" "$APP_H"
  else
    echo "ERROR: cannot find 'static struct shell_app app_table[]' in $APP_H"
    exit 6
  fi
}

insert_table_item() {
  awk -v app="$APP" -v desc="$DESC" '
    BEGIN{ inserted=0 }
    {
      if (!inserted && $0 ~ /\{[[:space:]]*NULL[[:space:]]*,[[:space:]]*NULL[[:space:]]*,[[:space:]]*NULL[[:space:]]*\}/) {
        print "    {\"" app "\", " app "_entry, \"" desc "\"},"
        inserted=1
      }
      print
    }
    END{
      if (inserted==0) {
        exit 42
      }
    }
  ' "$APP_H" > "$APP_H.tmp" || {
    rc=$?
    rm -f "$APP_H.tmp"
    if [[ $rc -eq 42 ]]; then
      echo "ERROR: sentinel '{NULL,NULL,NULL}' not found in $APP_H"
      exit 7
    else
      echo "ERROR: awk failed while inserting table item"
      exit 8
    fi
  }
  mv "$APP_H.tmp" "$APP_H"
}

remove_prototype() {
  sedi "/void[[:space:]]\+${APP}_entry[[:space:]]*(void[[:space:]]*\*arg)[[:space:]]*;/d" "$APP_H" \
    || true
}

remove_table_item() {
  sedi "/{[[:space:]]*\"${APP}\"[[:space:]]*,[[:space:]]*${APP}_entry[[:space:]]*,/d" "$APP_H" \
    || true
}

create_source() {
  if [[ -e "$SRC" ]]; then
    echo "ERROR: source already exists: $SRC"
    exit 5
  fi
  cat > "$SRC" <<EOF
#include "app.h"
void ${APP}_entry(void *arg) {
    SHELL_PRINTF("${APP} running!\\r\\n");
    return;
}
EOF
}

delete_source() {
  if [[ -f "$SRC" ]]; then
    rm -f "$SRC"
    echo "[INFO] deleted: $SRC"
  else
    echo "[INFO] source not found (skip): $SRC"
  fi
}

case "$CMD" in
add)
  if exists_in_header; then
    echo "ERROR: app '$APP' already exists in $APP_H"
    exit 4
  fi
  insert_prototype
  insert_table_item
  create_source
  echo "OK: added app '${APP}'"
  echo " - prototype & table entry updated in: $APP_H"
  echo " - source created: $SRC"
  ;;

del)
  remove_table_item
  remove_prototype
  delete_source
  echo "OK: deleted app '${APP}' from: $APP_H"
  ;;

*)
  usage
  ;;
esac
