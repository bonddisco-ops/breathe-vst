#!/bin/bash
# Breathe installer for macOS: copies the plugins and removes the download quarantine.
cd "$(dirname "$0")"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
echo "Installing Breathe..."
for P in "Breathe" "Breathe Analog Color"; do
  rm -rf "$VST3/$P.vst3" "$AU/$P.component"
  cp -R "$P.vst3" "$VST3/" && echo "  $P VST3 -> $VST3"
  cp -R "$P.component" "$AU/" && echo "  $P AU   -> $AU"
  xattr -cr "$VST3/$P.vst3" "$AU/$P.component" 2>/dev/null
  xattr -cr "$P.app" 2>/dev/null
done
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "Done! Rescan plugins in your DAW. / Готово! Пересканируй плагины в DAW."
echo "You can close this window. / Это окно можно закрыть."
