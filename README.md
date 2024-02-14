NG-Scope-5G
===========

Implement on top of srsRAN_4G UE code, decode the DCI and SIB information for 5G SA base station.

## Requirements

We tested this system on Ubuntu 22.04 system and it may support other version of Ubuntu. To build this project and make it run properly, the following libraries are needed.

[UHD libraries](https://files.ettus.com/manual/page_install.html):

```
sudo add-apt-repository ppa:ettusresearch/uhd
sudo apt-get update
sudo apt-get install libuhd-dev uhd-host
```

[srsRAN_4G&#39;s requirements](https://docs.srsran.com/projects/4g/en/latest/general/source/1_installation.html):

```
sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```

[yaml-cpp](https://github.com/jbeder/yaml-cpp):

```
# In a different directory
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build
cd build
cmake ..
make
sudo make install
```

Also, you may need the performance mode:

```
# This script is copied from srsRAN_Project
cd script
sudo ./srsran_performance
```

## Files and functions

```
Entry: /nrscope/src/main.cc
Load config: /nrscope/src/libs/load_config.cc
Radio thread (cell search, mib decoding, coreset decoding, etc.): /nrscope/src/libs/radio_nr.cc
Config file: /nrscope/config/config.yaml
```

## Usage

```
mkdir build
cd build
cmake ../
make -j ${nof_proc}
cd nrscope/src/
sudo ./nrscope
```

## Logs

(Aug-9) Solved some problems in synchronization, see how to use external clock to perform the synchronization.

(Aug-11) DCI 1_0 and PDSCH are decoded for gNB in n41 with `ssb_freq=2523.75 MHz`. SIB 1 payload is verified with  `/srsue/src/stack/rrc_nr/test/ue_rrc_nr_test.cc` and [libasn1](https://github.com/j0lama/libasn).

(Aug-13) DCI decoding is tested with the following settings using srsgNB: `n41, TDD, 20MHz, 30kHz SCS`; `n78, TDD, 20MHz, 30kHz SCS`; `n41, TDD, 20MHz, 15kHz SCS`; `n3, FDD, 20MHz, 15kHz SCS`; `n3, FDD, 10MHz, 15kHz SCS` and `n41, TDD, 10MHz, 15kHz SCS`.

(Aug-21) Used all RA-RNTI to decode DCI 1_0 scrambled with RA-RNTI and decoded the RAR (Msg 2) bytes. Then we should use the TC-RNTI in Msg 2 to decode DCI 1_0 for Msg 4 (RRC ConnectionSetup).

(Aug-26) Verified Msg 2 decoding and get TC-RNTI. Msg 4 successfully decoded and for UE tracked in RACH process, we can easily decode its other DCI in the following data communication.

(Sep-2) Decoding DCIs for 1 UE is finished. But some settings that affects DCI's size setting are still not clear to me. They are set manually and should be set according to RRC messages in the future.

(Sep-4) Downlink DCI 1_1 and grants are decoded, and the inconsistency between srsRAN_4G and srsRAN_Project codes causes the DCI size problem. Now the downlink grants are correctly decoded. Uplink DCI 0_1 decoded, but there are some problems left -- the elements of the DCI 0_1 is not set correctly so the uplink grants may have some problems. This problem can be easily solved by more reading, but left for the testing phase.

(Oct-1) Works for 30kHz SCS with 20MHz bandwidth in TDD bands (n41, n48, n78), both srsgNB and sercomm small cell, some bugs in different SCS and bandwidth settings, such as 15kHz+20MHz, 15kHz+10MHz and 30kHz+10MHz, in TDD band (n41) and FDD bands (n3).

(Oct-7) A very rough fix for `n3, FDD, 20MHz, 15kHz SCS` and `n3, FDD, 10MHz, 15kHz SCS` settings, in `lib/src/phy/dft/ofdm.c` function `srsran_ofdm_set_phase_compensation_nrscope(..)`, I added some fixed phase compensation for such settings. Now the code can decode SIB 1 for such settings, but we cannot verify them because the real phone cannot attach. Will move on to work with small cell (`n48, TDD, 20MHz, 30kHz SCS`) and gaming platform.

(Oct-30) Code works for small cell with known UEs (listened in RACH), as in srsgNB. Now try to make it work for unknow UEs, such as UEs that are already existing in the base station.

(Feb-8) Use a new repo for code release and next phase development.

(Feb-14) Decoding other SIBs (2, 3 transmitted by the small cell, and potentially other SIBs in other cells) is finished. Now we use three concurrent threads to decode SIB, RACH and DCI for the same TTI, not in a producer-consumer style but in a non-blocking function way. Because we want the processing happens in the same time, which would be helpful for future carrier aggregation that requires synchronization between cells. However, after a while, NG-Scope can't decode any new DCIs for SIB, RACH and exisiting UEs, the behavior looks like the synchronization failure when the GPS clock is not used. The next step is to figure it out what makes the function unable to decode any new DCIs. One guess is that the processing time for SIB and RACH RRC decoding takes too long (~1000 us) in some TTIs, which drags the buffer and destroys the synchronization, however according to some observation, the last few DCIs' processing time before the "stop" is not so long.

## TODOs

There are some on-going plans for the near future:

* Try to figure out what causes the "unable to decode DCI" problem.
* Get a better logging functions, and add APIs (in logging class maybe) to send data to a Python server.
* Try to decode RRC reconfiguration message.
* Test the tool with different bandwidth, SCS, and different duplexing modes (TDD and FDD), with the help of Amarisoft.
