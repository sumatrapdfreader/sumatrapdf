# Simple script to revert the changes made by destatic.sh

# Allow for the fact that mujs might not be present
MUJS_SRC=
test -d thirdparty/mujs && MUJS_SRC=thirdparty/mujs

# Convert it all back
sed -i 's!/\*static \*/!static !' $(find source platform/android/viewer platform/java $MUJS_SRC -name '*.c')
