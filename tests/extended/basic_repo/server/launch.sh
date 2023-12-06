#!/bin/bash

set -e

make_repo () {
  pushd /root

  /Local/Common/ptest-repo "$1".repo
  mkdir -p /srv/www/htdocs/"$1"
  mv /Local/Packages/RPMS /srv/www/htdocs/"$1"
  rm -rf /Local/Packages/RPMS

  find /srv/www/htdocs/"$1"/ -iname "*.rpm" | xargs rpm --addsign

  createrepo /srv/www/htdocs/"$1"

  cp RPM-GPG-KEY-pmanager /srv/www/htdocs/"$1"/repodata/repomd.xml.key
  gpg -ab -utest-sig@example.com /srv/www/htdocs/"$1"/repodata/repomd.xml

  popd
}

#set up rpm sig structure
rsync -aI --delete --checksum /Local/Common/gnupg/ /root/.gnupg
chown -R root:root /root/.gnupg

pushd /root
gpg --export -a 'Pkg Manager' > RPM-GPG-KEY-pmanager
rpm --import RPM-GPG-KEY-pmanager
popd

cp /Local/Common/rpmmacros /root/.rpmmacros
make_repo "basic"

/usr/sbin/apache2ctl -DFOREGROUND

