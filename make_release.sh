#!/bin/sh
# vim:et:ai:ts=4:sw=4:filetype=sh:tw=0:

set -x

cur_dir=$(cd $(dirname $0); pwd)
cd $cur_dir/

umask 002

[ -n "$RELEASE_TOPDIR" ] ||
    RELEASE_TOPDIR=/var/ftp/pub/linux.dell.com/srv/www/vhosts/linux.dell.com/html/files/biosdevname/
[ -n "$SIGNING_KEY" ] ||
    SIGNING_KEY=libsmbios

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
git tag -u $SIGNING_KEY -m "tag for official release: $PACKAGE_STRING" v${PACKAGE_VERSION}
pushd _builddir

DEST=$RELEASE_TOPDIR/$PACKAGE_NAME-$PACKAGE_VERSION/
mkdir -p $DEST
for i in *.tar.{gz,bz2} *.zip *.src.rpm; do
    [ -e $i ] || continue
    [ ! -e $DEST/$(basename $i) ] || continue
    cp $i $DEST
done

#/var/ftp/pub/yum/dell-repo/software/_tools/upload_rpm.sh ./_builddir/dist/SRPMS/${PACKAGE_NAME}-${PACKAGE_VERSION}-*.src.rpm
echo /var/ftp/pub/yum/dell-repo/software/_tools/upload_rpm.sh ./_builddir/dist/SRPMS/${PACKAGE_NAME}-${PACKAGE_VERSION}-*.src.rpm

#git push --tags origin master:master
