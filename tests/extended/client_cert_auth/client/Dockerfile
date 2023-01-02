#client
FROM registry.opensuse.org/opensuse/tumbleweed

# Add needed repositories
RUN zypper -n --gpg-auto-import-keys ref

#Install our needed packages
RUN zypper -n in catatonit\
                 bash \
                 vim

COPY /client_cert_auth/client/bootstrap.sh /root
COPY /client_cert_auth/client/launch.sh /root
COPY /client_cert_auth/client/basic.repo /etc/zypp/repos.d

RUN --mount=target=/Local/Common,source=/common_files /root/bootstrap.sh

# Launch the container, catatonit is just a simple init process that keeps the container running for us
CMD ["/usr/bin/catatonit", "-P"]
