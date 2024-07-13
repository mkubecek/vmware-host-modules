#!/bin/bash
# dkms-setup.sh - Configure DKMS hooks for the VMware kernel modules
#
# Usage: dkms-setup.sh [OPTIONS]
#   OPTIONS:
#   -u, --update  Remove older versions of the VMware modules from the DKMS tree
#                 before installing the newest. Operations will only be applied
#                 on the currently running kernel.
#   -h, --help    Show this help and exit.
#
# This script handles
#  - VMware version detection
#  - Checking out and applying needed patches from mkubecek/vmware-host-modules
#  - Adding, building, and installing the modules through DKMS so that they
#    will be automatically updated when new kernels are installed on the system
#  - Automatic removal of older VMware module versions from the DKMS tree 
#    *for the currently running kernel ONLY*
#
# This script does *not* handle
#  - Package dependencies. `git' and `dkms' are of course required, as are 
#    your distribution's linux headers, kernel build system, and C compiler.
#    Additional dependencies will vary.
#  - Removal of older modules for other kernel versions. There are simply too
#    many ways to unintentionally screw up someone's setup this way--it's
#    easier to manage more complex cases directly through the DKMS CLI.
#  - Updating the secure boot configuration to allow loading self-signed
#    kernel modules. Having a user reboot into their UEFI firmware to enroll
#    a new MOK certificate is outside the scope of this script. DKMS will 
#    automatically sign newly compiled modules (and generate a signing key, 
#    if needed). Usually, it will place the public/private key pair at
#    /var/lib/dkms/mok.{pub,key}, but can also be configured to use an existing
#    keypair through the $mok_signing_key and $mok_certificate variables in
#    /etc/dkms/framework.conf.d
#

set -e

function usage() {
    echo "Usage: dkms-setup.sh [OPTIONS]"
    echo "OPTIONS:"
    echo "  -u, --update   Remove older versions of the VMware modules from the DKMS tree"
    echo "                 before installing the newest. Operations will only be applied"
    echo "                 on the currently running kernel."
    echo "  -h, --help     Show this help and exit."
}

function die() { # die(err_msg, exitcode=1)
    echo "Fatal error: $1" >&2
    exit ${2-1}
}

function remove_outdated() {
    # Currently, this will remove older versions for the running kernel.
    # More fine-grained maintenance can be done directly through the dkms CLI.

    local OLD_INSTALLS DONE

    DONE=0
    OLD_INSTALLS=($(${DKMS} status "${PKG_NAME}" \
                    | grep "${KVERSION}" \
                    | awk -F, '{ print $1; }'))

    for i in "${OLD_INSTALLS[@]}"; do
        if (echo -n "$i" | grep -q "${PKG_VER}"); then

            echo "Target version '${PKG_VER}' is already added to DKMS." \
                 "Skipping."
            DONE=1

            continue
        fi

        echo "Removing version '$i' for kernel '${KVERSION}'..."
        sudo "${DKMS}" remove "$i" -k "${KVERSION}"
    done

    if [ ${DONE} -eq 1 ]; then 
        exit 0
    fi
}

SCRIPT_PATH=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
SCRIPT_CWD=$(readlink -f "$PWD")

PKG_NAME=vmware_host_modules
MODULES=(vmmon vmnet)
KVERSION=$(uname -r)

# Can be overridden by environment
: ${VMWARE_PRODUCT:=workstation}
: ${VMWARE_CFG_PATH:=/etc/vmware/config}

# Defaults for Fedora & Debian DKMS; some distros use MOK.priv & MOK.der instead.
# In any case, these should be the paths to the MOK key/cert pair DKMS
#  will use to auto-sign new modules when the system has secure boot enabled.
# Their default values can be found in /etc/dkms/framework.conf.
: ${DKMS_PRIVKEY:=/var/lib/dkms/mok.key}
: ${DKMS_CERT:=/var/lib/dkms/mok.pub}

: ${DKMS:=/sbin/dkms}

# Ignore anything past the 1st arg
if [ $# -gt 0 ]; then
    case "$1" in
        (-u | --update)     RM_OUTDATED=1;;
        (-h | --help)       usage; exit 0;;
        (*)                 usage; exit 1;;
    esac
fi

test -f "${DKMS}" || which dkms 2>&1 > /dev/null \
    || die "Unable to locate the DKMS executable."

VMWARE_VERNUM=$(grep -F "${VMWARE_PRODUCT}.product.version" "$VMWARE_CFG_PATH" \
              | awk -F ' = ' '{ print $2; }' \
              | xargs)

([ ${PIPESTATUS[0]} -ne 0 ] || [ "$VMWARE_VERNUM" == "" ]) \
    && die "Unable to find version number for product '$VMWARE_PRODUCT'."

PKG_VER="${VMWARE_PRODUCT}-${VMWARE_VERNUM}"
echo "Detected VMware ${VMWARE_PRODUCT} version ${VMWARE_VERNUM}."

[ ${RM_OUTDATED-0} -eq 1 ] && remove_outdated

pushd "${SCRIPT_PATH}"

echo "Generating dkms.conf..."
sed -e "s/@PKG_VER@/${PKG_VER}/" \
    -e "s/@PKG_NAME@/${PKG_NAME}/" \
        "${SCRIPT_PATH}/dkms.conf.in" \
    > "${SCRIPT_PATH}/dkms.conf"
[ $? -eq 0 ] || die "Unable to generate dkms.conf"

git fetch
git checkout "$PKG_VER"
[ $? -eq 0 ] || die "VMware $VMWARE_PRODUCT $VMWARE_VERNUM is not supported. \
                     Note that for versions >= 17.0, you should set VMWARE_PRODUCT \
                     to 'workstation' regardless of which you're using."
popd

echo "Setting up DKMS build system..."
# `dkms add` will fail with 3 if we're already added for a different kversion
sudo "${DKMS}" add "${SCRIPT_PATH}" || [ $? -eq 3 ]
sudo "${DKMS}" build "${PKG_NAME}/${PKG_VER}"
sudo "${DKMS}" install "${PKG_NAME}/${PKG_VER}"

echo "Loading modules into kernel..."
FAILURES=()
for mod in "${MODULES[@]}"; do
    sudo modprobe $mod || FAILURES+="$mod"
done

if [ "${#FAILURES[@]}" -gt 0 ]; then
    echo "Failed to load the following modules into the kernel: "
    echo "${FAILURES[@]}"
    echo "Tips:"
    echo " * Check the kernel message buffer for errors (e.g. \`dmesg -ewH')."
    echo " * Check your secure boot configuration. You may need to enroll"
    echo "   the DKMS certificate into your motherboard's UEFI firmware."
    echo "   In most distros, the public key should be located at ${DKMS_CERT}."
fi
