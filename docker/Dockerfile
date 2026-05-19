FROM ubuntu:24.04

ARG user_id
ARG root_psw="12345678"
ARG user_psw="ok"
ARG user_name=user

# Install system packages and create user (runs as root).
RUN \
    echo "**** packages installation ****" \
        && apt-get update && apt-get install -y --no-install-recommends \
            build-essential \
            cmake \
            pkg-config \
            libopencv-dev \
            libjpeg-dev \
            libpng-dev \
            libzstd-dev \
            libglew-dev \
            libpthread-stubs0-dev \
            libaravis-dev \
            git \
            time \
            sudo \
            wget \
            zip \
            nano \
            rsync \
            python3-pip \
        && apt-get clean && rm -rf /var/lib/apt/lists/* \
    && echo "**** l / ll aliases ****" \
        && printf 'ls --color=auto "$@"\n'    > /usr/bin/l  \
        && printf 'ls -lah --color=auto "$@"\n' > /usr/bin/ll \
        && chmod +x /usr/bin/l /usr/bin/ll \
    && echo "**** history-search-backward on F8 ****" \
        && sed -i 's/# "\\e\[5~": history-search-backward/"\\e\[19~": history-search-backward/' /etc/inputrc \
    && echo "**** root password ****" \
        && echo "root:${root_psw}" | chpasswd \
    && echo "**** user creation ****" \
        && useradd -m -s /bin/bash -u ${user_id} -G sudo ${user_name} \
        && echo "${user_name}:${user_psw}" | chpasswd \
        && chown -R ${user_name}:${user_name} /home/${user_name}/ \
        && mkdir -p /home/${user_name}/workspace \
        && chown -R ${user_name}:${user_name} /home/${user_name}/workspace

# Install ROS2 Rolling and colcon (runs as root)
COPY docker/setup_ros.sh /tmp/setup_ros.sh
RUN bash /tmp/setup_ros.sh && rm /tmp/setup_ros.sh

COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

USER ${user_name}
WORKDIR /home/${user_name}/

# Interactive shell: source ROS and the colcon overlay (built at runtime)
RUN echo "source /opt/ros/rolling/setup.bash" >> /home/${user_name}/.bashrc \
    && echo '[ -f ~/ros_ws/install/setup.bash ] && source ~/ros_ws/install/setup.bash' >> /home/${user_name}/.bashrc

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
