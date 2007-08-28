#!/bin/sh
# vim:et:ai:ts=4:sw=4:filetype=sh:tw=0:

set -x

cur_dir=$(cd $(dirname $0); pwd)
cd $cur_dir/

umask 002

# if anything hiccups, halt:
set -e

chmod -R +w _builddir ||:
rm -rf _builddir

mkdir _builddir
pushd _builddir
../configure
make -e distcheck
make -e srpm

. version

popd
/var/ftp/pub/yum/dell-repo/testing/_tools/upload_rpm.sh     \
        -c /var/ftp/pub/yum/dell-repo/testing/_tools/repo-biosdevname.cfg   \
        ./_builddir/dist/SRPMS/${PACKAGE_NAME}-${PACKAGE_VERSION}-*.src.rpm
