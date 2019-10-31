# Simple script to make all static functions in the main source
# unstatic. This allows backtracing functions (such as that used
# in Memento, in particular on Android) to pick up symbol names
# nicely.
#
# This script can be reversed using restatic.sh

# Allow for the fact that mujs might not be present
MUJS_SRC=
test -d thirdparty/mujs && MUJS_SRC=thirdparty/mujs

# Convert everything
sed -i '/^static inline/b;/^static const/b;s!^static !/\*static \*/!' $(find source platform/android/viewer platform/java $MUJS_SRC -name '*.c')

# Convert source/tools back again.
sed -i 's!/\*static \*/!static !' $(find source/tools -name '*.c')
