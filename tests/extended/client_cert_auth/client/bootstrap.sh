#!/bin/bash
set -e

cp /Local/Common/certs/ca/ca.crt /etc/pki/trust/anchors/
update-ca-certificates

exit 0
