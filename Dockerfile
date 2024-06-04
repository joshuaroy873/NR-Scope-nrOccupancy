FROM ubuntu:22.04

RUN add-apt-repository ppa:ettusresearch/uhd
RUN apt-get update
RUN apt-get install -y libuhd-dev uhd-host

RUN apt-get install -y build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev

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