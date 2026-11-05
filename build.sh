#!/bin/bash
# Build Projet and open it.
#   ./build.sh          build (Release) and launch
#   ./build.sh debug    build a Debug version and launch
#   ./build.sh xcode    generate an Xcode project in build-xcode/ and open it
#   ./build.sh tests    build and run the measurement program (report in Measurements/)
#                       add --quick for a short run
set -e
cd "$(dirname "$0")"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ "$1" = "xcode" ]; then
    cmake -B build-xcode -G Xcode
    open build-xcode/Projet.xcodeproj
    exit 0
fi

if [ "$1" = "tests" ]; then
    shift
    JUCE_ARG=""
    [ -d build/_deps/juce-src ] && JUCE_ARG="-DPROJET_JUCE_PATH=$(pwd)/build/_deps/juce-src"
    cmake -B build-tests -DCMAKE_BUILD_TYPE=Release -DPROJET_BUILD_TESTS=ON $JUCE_ARG
    cmake --build build-tests --config Release --target ProjetTests -j "$JOBS"
    ./build-tests/ProjetTests_artefacts/Release/ProjetTests --out Measurements "$@"
    echo "Report: Measurements/report.md"
    exit 0
fi

CONFIG=Release
[ "$1" = "debug" ] && CONFIG=Debug

cmake -B build -DCMAKE_BUILD_TYPE=$CONFIG
cmake --build build --config $CONFIG -j "$JOBS"

APP="build/Projet_artefacts/$CONFIG/Projet.app"
if [ -d "$APP" ]; then
    echo "Built: $APP"
    open "$APP"
fi
