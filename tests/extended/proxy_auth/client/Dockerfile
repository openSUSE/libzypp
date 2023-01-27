#client
FROM registry.opensuse.org/opensuse/tumbleweed

# Add needed repositories
RUN zypper -n --gpg-auto-import-keys ref

#Install our needed packages
RUN zypper -n in catatonit\
                 bash \
                 vim

COPY /client/bootstrap.sh /root
COPY /client/launch.sh /root

# Launch the container, catatonit is just a simple init process that keeps the container running for us
CMD ["/usr/bin/catatonit", "-P"]
