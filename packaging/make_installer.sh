#!/bin/bash
# Crea Remacdent-FX-Instalador.pkg con Lazo y Hueco (VST3 + AU).
set -euo pipefail

VERSION="${1:-1.0.0}"
WORK="build/pkg"
OUT="dist"

rm -rf "$WORK" "$OUT"
mkdir -p "$WORK/scripts" "$OUT"

cat > "$WORK/scripts/postinstall" <<'EOS'
#!/bin/bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOS
chmod +x "$WORK/scripts/postinstall"

make_component () {
    local name="$1" src="$2" dest="$3" extra="${4:-}"
    mkdir -p "$WORK/root-$name"
    cp -R "$src" "$WORK/root-$name/"
    pkgbuild --analyze --root "$WORK/root-$name" "$WORK/$name.plist"
    local i=0
    while plutil -extract "$i" xml1 -o /dev/null "$WORK/$name.plist" 2>/dev/null; do
        plutil -replace "$i.BundleIsRelocatable" -bool NO "$WORK/$name.plist"
        i=$((i + 1))
    done
    pkgbuild --root "$WORK/root-$name" \
             --component-plist "$WORK/$name.plist" \
             --identifier "com.remacdent.fx.$name" \
             --version "$VERSION" \
             --install-location "$dest" \
             $extra \
             "$WORK/$name.pkg"
}

for P in Lazo Hueco; do
    lower=$(echo "$P" | tr '[:upper:]' '[:lower:]')
    make_component "$lower-vst3" "build/${P}_artefacts/Release/VST3/$P.vst3"     "/Library/Audio/Plug-Ins/VST3"
    make_component "$lower-au"   "build/${P}_artefacts/Release/AU/$P.component"  "/Library/Audio/Plug-Ins/Components" "--scripts $WORK/scripts"
done

cat > "$WORK/distribution.xml" <<EOX
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Remacdent FX ${VERSION}</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="lazo"/>
        <line choice="hueco"/>
    </choices-outline>
    <choice id="lazo" title="Lazo (compresor de bus)" description="VST3 y Audio Unit">
        <pkg-ref id="com.remacdent.fx.lazo-vst3"/>
        <pkg-ref id="com.remacdent.fx.lazo-au"/>
    </choice>
    <choice id="hueco" title="Hueco (espacio por sidechain)" description="VST3 y Audio Unit">
        <pkg-ref id="com.remacdent.fx.hueco-vst3"/>
        <pkg-ref id="com.remacdent.fx.hueco-au"/>
    </choice>
    <pkg-ref id="com.remacdent.fx.lazo-vst3"  version="${VERSION}">lazo-vst3.pkg</pkg-ref>
    <pkg-ref id="com.remacdent.fx.lazo-au"    version="${VERSION}">lazo-au.pkg</pkg-ref>
    <pkg-ref id="com.remacdent.fx.hueco-vst3" version="${VERSION}">hueco-vst3.pkg</pkg-ref>
    <pkg-ref id="com.remacdent.fx.hueco-au"   version="${VERSION}">hueco-au.pkg</pkg-ref>
</installer-gui-script>
EOX

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$OUT/Remacdent-FX-Instalador-${VERSION}.pkg"
echo "Listo: $OUT/Remacdent-FX-Instalador-${VERSION}.pkg"
