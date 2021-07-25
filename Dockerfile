# Only tested on Ubuntu 18.04, other versions *may* work
FROM ubuntu:18.04

# Install Gem5 dependencies
RUN apt-get update
RUN apt-get -y install curl build-essential \
    less swig m4 libprotobuf-dev libgoogle-perftools-dev \
    gcc-5 g++-5 python2.7 protobuf-compiler python2.7-dev \
    libboost-all-dev gcc-8 g++-8 task-spooler
RUN curl https://bootstrap.pypa.io/pip/2.7/get-pip.py --output get-pip.py
RUN python2.7 get-pip.py
RUN update-alternatives --install /usr/bin/python python /usr/bin/python2.7 1
RUN pip2 install scons six python-config

# Install PMDK dependencies
RUN apt-get -y install autoconf pkg-config libndctl-dev \
    libdaxctl-dev pandoc git libjemalloc1 libjemalloc-dev \
    htop wget

RUN wget -nv 'https://repo.anaconda.com/archive/Anaconda3-2021.05-Linux-x86_64.sh'
RUN chmod +x Anaconda3-2021.05-Linux-x86_64.sh
RUN ./Anaconda3-2021.05-Linux-x86_64.sh -b

RUN /root/anaconda3/bin/conda install -y -c conda-forge jupyterlab

WORKDIR pmweaver_ae
COPY . ./

# Copy images
RUN mkdir -p /mnt/
COPY pm_images /mnt/pmem0/

# Build Janus' workloads
WORKDIR janus_workload
RUN make -j1 CFLAGS+="-DGEM5 -I/pmweaver_ae/gem5/include/" CXX=g++-8 CC=gcc-8

# Build PMDK
WORKDIR ../pmdk
RUN make -j$(nproc) EXTRA_CFLAGS+="-DGEM5 -I/pmweaver_ae/gem5/include/" CXX=g++-8 CC=gcc-8
RUN make -j$(nproc) install

# Build Gem5
WORKDIR ../gem5
RUN scons ./build/X86/gem5.opt -j$(nproc) CXX='g++-5' CC='gcc-5'

WORKDIR ../scripts

# Setup environment
ENV LD_LIBRARY_PATH="/usr/local/lib/"
RUN /root/anaconda3/bin/conda init bash
