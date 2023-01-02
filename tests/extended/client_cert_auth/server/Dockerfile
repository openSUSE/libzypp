#client
FROM registry.opensuse.org/opensuse/leap:15.1

COPY /client_cert_auth/server/bootstrap.sh /root
COPY /client_cert_auth/server/launch.sh /root
COPY /client_cert_auth/server/basic.repo /root
COPY /client_cert_auth/server/vhost.conf /etc/apache2/vhosts.d/

# Add needed repositories and install required packages
RUN zypper -n --gpg-auto-import-keys ref  \
             && zypper -n in bash \
             apache2 \
             vim \
             createrepo_c \
             python3 \
             rsync \
             patterns-devel-base-devel_rpm_build \
             && sysconf_addword /etc/sysconfig/apache2 APACHE_SERVER_FLAGS SSL

RUN --mount=target=/Local/Common,source=/common_files /root/bootstrap.sh

# Launch the container
CMD ["/root/launch.sh"]
