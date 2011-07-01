#! /bin/bash

# Todo:
#     Code10: /var/lib/zypp/db/products/
#     Code11: /etc/products.d
#
# - migrate all product entries (find reasonable names for the .prod file)
# - determine the baseproduct and create the symlink.
# - upon sucess mv /var/lib/zypp/db/products to products.migrated to
#   prevent re-migration id bootstrap.sh is re-run.
#
SCHEMADIR=`dirname $0`
OLD_ZYPP_DIR="/var/lib/zypp/db/products/"
NEW_ZYPP_DIR="/etc/products.d/"

mkdir -p $NEW_ZYPP_DIR
BASE_INSTALL_TIME=2145999599

for p in $OLD_ZYPP_DIR/*; do
    #echo "$p"
    NEW_PROD_NAME=`cat $p | sed -n 's/^.*<name>\(.\+\)<\/name>.*$/\1/p'`
    NEW_PROD_NAME="$NEW_PROD_NAME.prod"
    INSTALL_TIME=`cat $p | sed -n 's/^.*<install-time>\(.\+\)<\/install-time>.*$/\1/p'`
    IS_BASE="no"
    if grep '<product.*type="base' "$p" >/dev/null; then
       IS_BASE="yes"
    fi
    /usr/bin/xsltproc --output $NEW_ZYPP_DIR/$NEW_PROD_NAME "$SCHEMADIR/10-11.migrate.product.xsl" $p
    if [ "$IS_BASE" == "yes" ]; then
       if [ -L "$NEW_ZYPP_DIR/baseproduct" -a $INSTALL_TIME -lt $BASE_INSTALL_TIME ]; then
           #echo "found different baseproduct: $INSTALL_TIME < $BASE_INSTALL_TIME"
           rm -f "$NEW_ZYPP_DIR/baseproduct"
           ln -sf "$NEW_ZYPP_DIR/$NEW_PROD_NAME" "$NEW_ZYPP_DIR/baseproduct"
           BASE_INSTALL_TIME=$INSTALL_TIME
       elif [ ! -L "$NEW_ZYPP_DIR/baseproduct" ]; then
           ln -sf "$NEW_ZYPP_DIR/$NEW_PROD_NAME" "$NEW_ZYPP_DIR/baseproduct"
           BASE_INSTALL_TIME=$INSTALL_TIME
       fi
    fi
done


