FROM ubuntu:24.04

#install UHD
RUN apt-get update
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:ettusresearch/uhd
RUN apt-get update
RUN apt-get install -y libuhd-dev=4.6.0.0+ds1-5.1ubuntu0.24.04.1 uhd-host=4.6.0.0+ds1-5.1ubuntu0.24.04.1

#srsRAN_4G requirements
RUN apt-get install -y build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev

#yaml-cpp
RUN apt-get install -y git
WORKDIR /
ADD https://github.com/jbeder/yaml-cpp.git /yaml-cpp
WORKDIR /yaml-cpp/build
RUN cmake ..
RUN make
RUN make install

#liquid-dsp
WORKDIR /
RUN apt-get install -y automake autoconf
ADD https://github.com/jgaeddert/liquid-dsp.git /liquid-dsp
WORKDIR /liquid-dsp
RUN ./bootstrap.sh
RUN ./configure
RUN make
RUN make install
RUN ldconfig

#WORKDIR /usr/share/uhd/images
#RUN uhd_images_downloader
#RUN uhd_image_loader --args="type=x300,addr=192.168.40.2"

WORKDIR /NR-Scope
COPY . .
WORKDIR build
RUN cmake ../
RUN make clean
RUN make all -j ${nof_proc}
WORKDIR nrscope/src
RUN install nrscope /usr/local/bin
