FROM ubuntu:22.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -y software-properties-common

RUN add-apt-repository ppa:ettusresearch/uhd
RUN apt-get update
RUN apt-get install -y libuhd-dev=4.1.0.5-3
RUN apt-get install -y uhd-host=4.1.0.5-3

RUN apt-get install -y build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
RUN apt-get install -y git

RUN uhd_images_downloader

WORKDIR /
RUN git clone https://github.com/jbeder/yaml-cpp.git
WORKDIR yaml-cpp/build
RUN cmake ..
RUN make
RUN make install
RUN ldconfig

WORKDIR /NG-Scope-5G
COPY . .
WORKDIR build
RUN cmake ../
RUN make -j $(nproc --ignore 1)
WORKDIR nrscope/src/
RUN install nrscope /usr/local/bin
