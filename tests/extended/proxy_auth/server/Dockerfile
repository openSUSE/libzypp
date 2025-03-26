#client
FROM registry.opensuse.org/opensuse/leap:15.1

# Add needed repositories
RUN zypper -n --gpg-auto-import-keys ref

#Install our needed packages
RUN zypper -n in bash \
                 apache2 \
                 vim \
                 createrepo_c \
                 python3 \
                 rsync \
                 patterns-devel-base-devel_rpm_build

COPY /server/launch.sh /root
COPY /server/basic.repo /root

# Launch the container
CMD ["/root/launch.sh"]

