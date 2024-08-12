NG-Scope-5G (for X310 TwinRX)
===========

Compared with the \*BX daughterboard which has a 184.32MHz clock rate, TwinRX daughterboard has a 200MHz clock rate. Previous srsRAN processing
is based on the "conventional" 184.32MHz. In other words, the sampling rates of \*BX and sampling rates of TwinRX can only be integer divisions
of 184.32MHz and 200MHz respectively. 

Therefore, we need to down-resample signals from 200MHz integer division to 184.32MHz integer division.


### Specifically, modifications done for TwinRX:

* Create two sampling config parameters: `srsran_srate` and `srate`. `srsran_srate` is what the srsRAN signal processing saw, which should be in the 184.32MHz family. `srate` is what fed to the USRP RF, which should be in the 200MHz family. Therefore, the down-resampling ratio will be `r = srsran_srate/srate` (see `config.yaml`).
* Down-resample the raw signal with ratio `r`, using the [liquid-dsp](https://liquidsdr.org/) library. Please install following their Github page.
* As resampling takes non-trivial portion of time (based on our test machine), we optimize the "producer-consumer" coordination pattern in the time-critical baseband processing scenario. There are two versions A and B. See their details respectively below in `twinrx_ver_A` and `twinrx_ver_B`.


### producer-consumer coordination pattern (TLDR)

Producer is codes fetching samples from USRP. Consumer is codes processing (e.g., fft and decoding). Previously, the pattern (a loop) looks like the following:

```
    start --> producer (subframe sample fetching) --> consumer (subframe processing) --> back to start
```

The constraint is you should finish an iteration within 1ms, otherwise you don't fetch the samples timely and overflow occurs. This means sync drift and monitoring no longer works. Without resampling, the whole iteration finish within 1ms robustly in our test machine. With resampling, it exceeds 1ms (on our test machine).

Therefore, we have the variant pattern B:

```
    initialize some semaphore S (resource produced/consumed indicator)
    (thread 1) start --> producer (subframe sample fetching, resampling, and buffering) --> S signal up --> back to start
    (thread 2) start --> S signal down --> consumer (subframe processing) --> back to start
```
As you can see:
> Producer and consumer execute at different rates <br/>
> – No serialization of one behind the other <br/>
> – Tasks are independent (easier to think about) <br/>
> – The buffer set allows each to run without explicit handoff

Version B processes agilely just like the main version with non-TwinRX USRP. Observed issues with version B:
* Sometimes after running for a while overflow still occurs, as resampling is moved to the time-critical producer thread. Investigating if we can assign a dedicated CPU to just the producer thread (I guess by default you can not control whether the CPU will be shared with other processes/threads).

A working example stdout is `stdout_example.txt`. 

Personally I prefer version B. See `twinrx_ver_B`. Only difference is version B moves resampling into producer thread.