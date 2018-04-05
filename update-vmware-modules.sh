#!/bin/bash
MODULE_ROOT="/usr/lib/vmware/modules/source/"
REPOSITORY_URL="https://github.com/mkubecek/vmware-host-modules"
VMWARE_PRODUCT="player-12.5.7"

############
## update-vmware-modules.sh
## 
## This script expects be run as root or equivalent. (Ain't nobody got time for issuing "sudo" on each line)
##
## This script will completely replace the source code of your VMWare Product's Modules with patched
## versions from github:mkubecek/vmware-host-modules. Hack this script up if you want to back up the original
## broken code before they're replaced.
##
## Just set the VMWARE_PRODUCT variable above to the workstation- or player-version you are
## using, and run this script. In extreme cases, a reboot may be necessary to get the
## updated modules loaded in.
##
## This script does not follow or use the TAGs naming format. It is expected the BRANCH zipfile from this 
## repository will include all known patches to date, with backwards-compatibility to keep it working
## on earlier Linux Kernel releases.
############

rm -rf $MODULE_ROOT/{vmmon-only,vmnet-only}
rm -rf $MODULE_ROOT/{vmmon-only.tar,vmnet-only.tar}

## Fetch and Unpack the Upstream Patched Files ##
rm -rf ./$VMWARE_PRODUCT.zip
wget $REPOSITORY_URL/archive/$VMWARE_PRODUCT.zip

unzip $VMWARE_PRODUCT.zip -d.

## Repack (tar) and Install the Upstream Patched Files to the VMWare Modules Folder ##
pushd vmware-host-modules-$VMWARE_PRODUCT
  tar cf vmmon.tar vmmon-only
  tar cf vmnet.tar vmnet-only

  mv vmmon.tar $MODULE_ROOT/
  mv vmnet.tar $MODULE_ROOT/
popd

## Invoke the VMWare Module Configuration tool
vmware-modconfig --console --install-all
