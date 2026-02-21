#!/usr/bin/env bash
set -euo pipefail

# ===== 0) Build tipi =====
echo "Build tipi seçin:"
echo "1) First Build (ilk defa derleme)"
echo "2) Rebuild (sadece out klasörleri silinir)"
echo "3) Hard Build (depot_tools + v8 tamamen silinir)"
read -rp "Seçiminizi girin (1/2/3): " BUILD_TYPE

case "$BUILD_TYPE" in
  1) CLEAN_BUILD=false; FULL_RESET=false ;;
  2) CLEAN_BUILD=true; FULL_RESET=false ;;
  3) CLEAN_BUILD=true; FULL_RESET=true ;;
  *) echo "Geçersiz seçim."; exit 1 ;;
esac

case "$(uname -m)" in
  arm64|aarch64) TARGET_CPU="arm64" ;;
  x86_64) TARGET_CPU="x64" ;;
  *) echo "Desteklenmeyen mimari: $(uname -m)"; exit 1 ;;
esac

ARGNSDIR="$PWD/gns"
WORKDIR="$PWD/../third_party/v8"
DEPO="$WORKDIR/depot_tools"
OUTDIR="$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample"
VENDOR_DIR="$WORKDIR/vendor/v8"

mkdir -p "$WORKDIR" "$VENDOR_DIR"

# ===== Full Reset =====
if [ "$FULL_RESET" = true ]; then
  echo "Full Reset yapılıyor..."
  rm -rf "$WORKDIR"
  mkdir -p "$WORKDIR" "$VENDOR_DIR"
fi

# ===== depot_tools =====
if [ ! -d "$DEPO" ]; then
  echo "Depot Tools klonlanıyor..."
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPO"
fi
export PATH="$DEPO:$PATH"

# ===== V8 kaynaklarını çek =====
cd "$WORKDIR"
if [ ! -d "v8" ]; then
  echo "V8 kaynakları fetch ediliyor..."
  fetch --no-history v8
fi

cd v8
gclient sync

# ===== Rebuild gerekiyorsa =====
if [ "$CLEAN_BUILD" = true ]; then
  echo "Rebuild seçildi: sadece out.gn dizini temizleniyor..."
  rm -rf "$WORKDIR/v8/out.gn"
fi

# ===== v8gen.py =====
echo "Otomatik GN konfigürasyonu başlatılıyor..."
cd "$WORKDIR/v8"
python3 tools/dev/v8gen.py "${TARGET_CPU}.release.sample"

# ===== args.gn güncelleme =====
ARGS_FILE="$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample/args.gn"


# OS tespiti
case "$(uname -s)" in
  Darwin*)  OS_NAME="mac" ;;
  Linux*)   OS_NAME="linux" ;;
  CYGWIN*|MINGW*|MSYS*) OS_NAME="win" ;;
  *)        OS_NAME="unknown" ;;
esac

SRC_ARGS_FILE="$ARGNSDIR/args.${OS_NAME}.${TARGET_CPU}.gn"

if [ -f "$SRC_ARGS_FILE" ]; then
  echo "args.gn içeriği $SRC_ARGS_FILE dosyasından dolduruluyor..."
  mkdir -p "$(dirname "$ARGS_FILE")"
  # args.gn dosyasını temizle, sonra kaynak dosyanın içeriğini içine bas
  : > "$ARGS_FILE"
  cat "$SRC_ARGS_FILE" >> "$ARGS_FILE"
  echo "✅ args.gn içeriği güncellendi → $ARGS_FILE"
else
  echo "❌ GNS dosyası bulunamadı: $SRC_ARGS_FILE"
  echo "Varsayılan args.gn bırakılıyor."
fi



# ===== Ninja build =====
echo "v8_monolith build ediliyor..."
ninja -C "$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample" v8_monolith

# ===== Vendor kopyalama =====
echo "Vendor dizinine kopyalanıyor..."
mkdir -p "$VENDOR_DIR/include" "$VENDOR_DIR/lib"

rsync -a --delete include/ "$VENDOR_DIR/include/"
rsync -a "$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample/obj/" "$VENDOR_DIR/lib/"

if [ -f "$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample/obj/libv8_monolith.a" ]; then
  cp "$WORKDIR/v8/out.gn/${TARGET_CPU}.release.sample/obj/libv8_monolith.a" "$VENDOR_DIR/lib/"
fi

echo "✅ Build tamamlandı!"
echo "Vendor dizini: $VENDOR_DIR"