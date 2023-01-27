#client
FROM registry.opensuse.org/opensuse/leap:15.1

# Add needed repositories
RUN zypper -n --gpg-auto-import-keys ref

#Install our needed packages
RUN zypper -n in catatonit\
                 bash \
                 vim \
                 squid

COPY /proxy/entrypoint.sh /usr/bin/entrypoint.sh
ENTRYPOINT ["entrypoint.sh"]
CMD ["-f", "/etc/squid/squid.conf", "-NYC"]
