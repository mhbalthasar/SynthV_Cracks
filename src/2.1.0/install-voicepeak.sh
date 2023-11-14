#!/bin/sh
KERNEL="$(uname -s)"
case "$KERNEL" in
  ?inux)
    PLATFORM="linux"
    ;;
  *)
    PLATFORM="macos"
    ;;
esac

RAW="$(curl -s "https://tts-auth.dreamtonics.com/api/update/fetch?product=VoicepeakEditor&platform=$PLATFORM")"
URL="$(echo "$RAW" | cut -d'"' -f4)"
BASE="$(basename "$URL")"

curl -# -o "$BASE" "$URL"

echo "Saved Voicepeak to: $BASE"
