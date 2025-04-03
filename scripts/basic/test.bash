
set -eE 
trap 'echo Error: in $0 on line $LINENO' ERR

for i in ../overlay-fydetab_duo-openfyde/sys-boot/rk-uboot/files/*.patch; do
git apply $i
git add .
git commit -m f
