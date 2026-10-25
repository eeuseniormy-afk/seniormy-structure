#!/bin/bash
# Instalador único para Mac: app + AU + VST3 + biblioteca orquestral (universal x86_64+arm64, macOS 10.13+).
# Termos de Uso: o Instalador mostra a licença (pt-BR ou en conforme o idioma do sistema) e exige o aceite.
set -euo pipefail
VERSION="${VERSION:-$(sed -n 's/^project(SeniorMyStructure VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)}"
NAME="SeniorMy Structure"
ART="build/SeniorMyStructure_artefacts/Release"
M=scripts/mac
LIB="${LIBRARY_DIR:-library}"

rm -rf pkgroot pkgs dist $M/res
mkdir -p pkgroot/au pkgroot/vst3 pkgroot/app "pkgroot/library/Structure Library" pkgs dist
cp -R "$ART/AU/$NAME.component"   pkgroot/au/
cp -R "$ART/VST3/$NAME.vst3"      pkgroot/vst3/
cp -R "$ART/Standalone/$NAME.app" pkgroot/app/
cp -R "$LIB"/. "pkgroot/library/Structure Library/"
cp Resources/legal/termos-*.txt Resources/legal/THIRD-PARTY-NOTICES.txt "pkgroot/library/Structure Library/"

# recursos do instalador: base (pt-BR + en) e uma pasta .lproj por idioma
mkdir -p $M/res
cp $M/resources/welcome.html $M/res/
{ cat Resources/legal/termos-pt-BR.txt; printf '\n\n==============================\n\n'; cat Resources/legal/termos-en.txt; } > $M/res/license.txt
for pair in "pt_BR:pt-BR" "pt:pt-BR" "en:en" "English:en"; do
  d="${pair%%:*}"; l="${pair##*:}"
  mkdir -p "$M/res/$d.lproj"
  cp "Resources/legal/termos-$l.txt" "$M/res/$d.lproj/license.txt"
  cp $M/resources/welcome.html "$M/res/$d.lproj/welcome.html"
done

# assinatura ad-hoc (necessária no Apple Silicon)
for b in "pkgroot/au/$NAME.component" "pkgroot/vst3/$NAME.vst3" "pkgroot/app/$NAME.app"; do
  codesign --force --deep --sign - "$b"
done

make_plist () {
  pkgbuild --analyze --root "$1" "$2"
  python3 - "$2" <<'PY'
import plistlib, sys
p = sys.argv[1]
with open(p, "rb") as f: d = plistlib.load(f)
for e in d:
    e["BundleIsRelocatable"] = False
    e["BundleOverwriteAction"] = "upgrade"
with open(p, "wb") as f: plistlib.dump(d, f)
PY
}
chmod 755 $M/pkgscripts/au/postinstall
make_plist pkgroot/app  pkgs/app.plist
make_plist pkgroot/au   pkgs/au.plist
make_plist pkgroot/vst3 pkgs/vst3.plist

pkgbuild --root pkgroot/app --component-plist pkgs/app.plist --install-location "/Applications" \
  --identifier com.seniormy.structure.app --version "$VERSION" pkgs/app.pkg
pkgbuild --root pkgroot/au --component-plist pkgs/au.plist --install-location "/Library/Audio/Plug-Ins/Components" \
  --scripts $M/pkgscripts/au --identifier com.seniormy.structure.au --version "$VERSION" pkgs/au.pkg
pkgbuild --root pkgroot/vst3 --component-plist pkgs/vst3.plist --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier com.seniormy.structure.vst3 --version "$VERSION" pkgs/vst3.pkg
pkgbuild --root pkgroot/library --install-location "/Library/Application Support/SeniorMy" \
  --identifier com.seniormy.structure.library --version "$VERSION" pkgs/library.pkg

sed "s/@VERSION@/$VERSION/g" $M/distribution.xml > pkgs/distribution.xml
productbuild --distribution pkgs/distribution.xml --package-path pkgs --resources $M/res \
  "dist/SeniorMy-Structure-$VERSION-macOS.pkg"
ls -lh dist
shasum -a 256 dist/*.pkg
