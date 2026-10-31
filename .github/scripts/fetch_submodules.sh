#!/bin/bash
# Fetch the submodules this build needs.
#
# A recursive checkout is not used: edk2 carries a host-based unit test
# framework whose SubhookLib submodule points at github.com/Zeex/subhook, a
# repository that no longer exists, so recursion aborts:
#
#   fatal: repository 'https://github.com/Zeex/subhook.git/' not found
#   Failed to recurse into submodule path 'edk2'
#
# Nothing in a firmware build touches UnitTestFrameworkPkg, so it is excluded
# instead of being allowed to take the whole checkout down with it.
set -euo pipefail

git submodule update --init --depth 1 \
    Common/edk2 \
    Common/edk2-platforms \
    Platform/EFI_Binaries \
    GPLDrivers/Library/SimpleInit

echo
echo "=== edk2 nested submodules, excluding the unit test framework ==="
git -C Common/edk2 submodule init
mapfile -t paths < <(
    git -C Common/edk2 config --file .gitmodules --get-regexp 'submodule\..*\.path$' \
        | awk '{print $2}' \
        | grep -v '^UnitTestFrameworkPkg/' \
        || true
)
printf '  %s\n' "${paths[@]}"
git -C Common/edk2 submodule update --jobs 4 "${paths[@]}"

echo
echo "=== edk2-platforms nested submodules ==="
mapfile -t ppaths < <(
    git -C Common/edk2-platforms config --file .gitmodules --get-regexp 'submodule\..*\.path$' 2>/dev/null \
        | awk '{print $2}' \
        || true
)
if [ ${#ppaths[@]} -gt 0 ]; then
    git -C Common/edk2-platforms submodule init
    git -C Common/edk2-platforms submodule update --jobs 4 "${ppaths[@]}"
else
    echo "  (none)"
fi

echo
echo "=== SimpleInit nested submodules ==="
git -C GPLDrivers/Library/SimpleInit submodule update --init --depth 1 --recursive

echo
echo "=== the pieces the build links against ==="
missing=0
for path in \
    Common/edk2/BaseTools/Source/C/BrotliCompress/brotli \
    Common/edk2/CryptoPkg/Library/OpensslLib/openssl \
    Platform/EFI_Binaries/Drivers/sm7150 \
    GPLDrivers/Library/SimpleInit/SimpleInit.inc \
    GPLDrivers/Library/SimpleInit/libs/freetype/include
do
    if [ -e "$path" ]; then
        echo "  ok      $path"
    else
        echo "  MISSING $path"
        missing=1
    fi
done

if [ "$missing" -ne 0 ]; then
    echo
    echo "a required submodule is missing; the build would fail later anyway"
    exit 1
fi

echo
echo "all required submodules are in place"
