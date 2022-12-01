# RPiTSN
A low-cost and easy-to-get alternative of RaspberryPi Real-Time HAT for deterministic communication.

## 1. Introduction

## 2. Testbed Setup

### 2.1 Equipments

- 2 * [Network Adapter(NIC) with Intel I210 Controller](https://www.amazon.com/ipolex-Single-Port-Gigabit-Ethernet-Converged/dp/B0728289M7/ref=sr_1_4?crid=1RXZWJCF4YJG0&keywords=i210&qid=1663721874&sprefix=i210%2Caps%2C138&sr=8-4) 
- 2 * Dell Linux Station 
- 1 * Ethernet CAT8 Cable

There are several reasons that we choose i210 NIC that you can find in the following links:

- [LaunchTime function](https://en.wikipedia.org/wiki/Launch_Time)
- [Faster with PCIe than DMA](https://forums.evga.com/Intel-NICs-i219-and-i210-on-X299-Dark-m2820935.aspx)
- [Receive Side Scaling (RSS)](https://blog.kylemanna.com/hardware/intel-nic-igb-i211-vs-e1000e-i219/)

### 2.2 Environment

### 2.2.1 Install igb driver for i210 NIC

Step 1: Download igb-5.11.4.tar.gz [here](https://www.intel.com/content/www/us/en/download/14098/intel-network-adapter-driver-for-82575-6-82580-i350-and-i210-211-based-gigabit-network-connections-for-linux.html).

Step 2: Install the igb driver following the link [here](https://downloadmirror.intel.com/738737/readme.txt).

Some potential issues:

- igb driver only supports for specific Linux kernels. During my test that Linux 5.4.0 and Linux 5.17.0 worked normally but 5.14.0 and 5.15.0 failed with GPF error in kernel logs and system got totally freezen. Some related issues can be found at [here](https://bugs.archlinux.org/task/65113).
- igb driver may occur some error when you install both e1000e driver and igb on same kernel. The reason I guess is e1000e also drives for I210 as a general driver but lacks of several functions.

### 2.2.2 Stop Linux synchronization service

Stop NTP service:

```
sudo systemctl stop systemd-timesyncd
```

```

# 802.1AS example configuration containing those attributes which
# differ from the defaults.  See the file, default.cfg, for the
# complete list of available options.
#
[global]
gmCapable		1
priority1		248
priority2		248
logAnnounceInterval	0
logSyncInterval		-3
syncReceiptTimeout	3
neighborPropDelayThresh	800
min_neighbor_prop_delay	-20000000
assume_two_step		1
path_trace_enabled	1
follow_up_info		1
transportSpecific	0x1
ptp_dst_mac		01:80:C2:00:00:0E
network_transport	L2
delay_mechanism		P2P

```

```
sudo ptp4l -i enp1s0 -f gptp.cfg -m 
```

### 2.3 Set up Linux Qdisc ETF Scheduler

The ETF (Earliest TxTime First) qdisc allows applications to control the instant when a packet should be dequeued from the traffic control layer into the netdevice, which makes it possible to schedule packets in a periodic manner.

Step 1: Configuring the ETF Qdisc following the link [here](https://tsn.readthedocs.io/qdiscs.html#configuring-the-etf-qdisc).

```
sudo tc qdisc del dev eth1 root
```


```
sudo tc qdisc add dev enp4s0 parent root handle 6666 mqprio \
        num_tc 3 \
        map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 \
        queues 1@0 1@1 2@2 \
        hw 0
```

```
sudo tc qdisc add dev enp4s0 parent 6666:1 etf \
        clockid CLOCK_TAI \
        delta 10000 \
        offload
```

Some potential issues:

- The wrong configuration for delta parameter might crush down the socket program. It must bound the delay from user space and should be estimated for different machines. This `delta` parameter must be account into the socket program for scheduling periodic traffic.

Several materials might be helpful:

- [tc-etf man page](https://man7.org/linux/man-pages/man8/tc-etf.8.html)
- [Qdisc ETF source code](https://lkml.org/lkml/2017/9/18/76)
- [Qdisc TSN offload]( https://github.com/xdp-project/xdp-project/blob/master/areas/tsn/code01_follow_qdisc_TSN_offload.org)

### 2.4 Periodic Socket Programming

We use Qdisc ETF scheduler to schedule traffic into a periodic manner and use linux HARDWARE timestamp to record the delay in MAC layer.

Step 1: Run sche.c on talker and recv.c on listener. The sending time and receiving time in HW and SW timestamp will output through terminal on both talker and listener.

Step 2: Record those output data and calculate the round-trip delay.

Note:

- 2 different functions defined in `utils.h`. 
  - `sche_single` is scheduled with ETF scheduler.
  - `send_single` is scheduled in C program (application level).

- Parameters:
  - `TIME_DELTA` that has to be adapted with the delay you estimated in Sec 2.3.
  - `DEFAULT_PRIORITY` that has to be adapted with the qdisc configuration you have done in Sec 2.3.
  - `BUFFER_LEN` is the length of UDP payload.
  - `HW_FLAG` should be set as 1 to record HW timestamp.
  - `USE_TXTIME` should be set as 1 to achieve time-triggered UDP sending.

> Configuration leads to lowest sending jitter on OptiPlex 7090: `qdisc-delta = 100_000`, \
`TIME_DELTA = 1_000_000`

Materials can be found at:

- [Source code](https://github.com/ChuanyuXue/TSN-EndSystem/tree/main/src).
- [Linux Kernal Archives: HW timestamp](https://www.kernel.org/doc/Documentation/networking/timestamping.txt).

## 3. Experiment Result

**Synchronization**


![local](test/cm4/sync_local.png)
![out](test/cm4/sync_utral.png)


**Jitter on different End-Station**

|        | Device | Packet Loss | Jitter (L7) | Jitter (L2) |
| ------ | ------ | ----------- | ----------- | ----------- |
| 1 s    | PC     | 0           | 1.54e-5     | 1.16e-8     |
|        | CM4    | 0           | 6.58e-6     | 1.10e-8     |
| 100 ms | PC     | 0.00%       | 1.71e-5     | 1.16e-8     |
|        | CM4    | 1.02%       | 3.50e-5     | 8.62e-9     |
| 10 ms  | PC     | 0.03%       | 2.19e-5     | 2.39e-5     |
|        | CM4    | 1.68%       | 4.14e-5     | 1.27e-8     |
| 1 ms   | PC     | 5.75%       | 2.13e-5     | 2.24e-5     |
|        | CM4    | 6.55%       | 1.22e-5     | 1.36e-8     |
| 100 us | PC     | 50.00%      | 2.22e-5     | 7.03e-9     |
|        | CM4    | 99.99%      | 7.14e-5     | 3.00e-9     |
| 50 us  | PC     | 66.70%      | 1.56e-4     | 1.50e-4     |
|        | CM4    | 99.99%      | N/A         | N/A         |

![1hz](test/cm4/1hz_jitter.png)
![10hz](test/cm4/10hz_jitter.png)
![100hz](test/cm4/100hz_jitter.png)
![1000hz](test/cm4/1000hz_jitter.png)



**E2E delay on different NIC**

![delay](test/tsn_multihop/06_compare_delay.png)

**Jitter on different NIC**
![delay](test/tsn_multihop/06_compare_jitter.png)

## 4. Future work
- Determining the cause of the ETF scheduler's inability to support high bandwidth. Its inability to enable out-of-order scheduling could be one of the causes. For instance, packet $f 1$ is planned to transmit at time $t 3$ at time $t 1$ while packet $f 2$ is scheduled to send at time $t_4$ at time $t_2$. When $t_2$ > $t_1$ and $t_4$ < $t_3$, the system crashes.

- Discover a method to support a single traffic with a frequency of 200000Hz that satisfies the following conditions: :
  - No packet loss.
  - Delay is bounded within 1us.
  - Jitter (the time gap between two neighbor packet) is constrained within 1us.
  
  Instead of purely using the ETF scheduler, you are allowed to experiment with other strategies like DPDK or OpenFlow.

- Find a means to accommodate more traffic in multiple periods simultaneously, such as three traffics with periods of 50000Hz, 30000Hz, and 70000Hz; ideally, the overall frequency should be greater than 100000 Hz.


## TODO
1. Wrap up the code as a normal cpp application
2. Finish the synchronization test result
3. Improve this read me and make it better
4. Do more comprehensive experiment on e2e delay



