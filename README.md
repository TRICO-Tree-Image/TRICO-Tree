<h1 align="center">
  <br>
  Revisiting the Hierarchical PIFO Model: Enhancing Accuracy and Efficiency in Programmable Scheduling
  <br>
</h1>
<p align="center">
  <a href="#-key-features">Key Features</a> •
  <a href="#-get-started">Get Started</a> •
  <a href="#-license">License</a> •
  <a href="#-links">Links</a>
</p>

## Key Features

* A resource-driven programmable hierarchical scheduling model TRICO Tree, enables accurate programmable hierarchical scheduling, including implementations in NS3 and a Python-based logical model.
* The hardware design and FPGA prototype of the TRICO Tree Scheduler, delivers accurate and efficient programmable hierarchical scheduling.
* A hierarchical scheduling requirements dataset. we collect over 20 diverse hierarchical scheduling requirements from both industry and academia. By employing scaling methods, we generated a dataset comprising over 200 hierarchical scheduling requirements.
* A Scheduling Configuration Generator, processes user-defined scheduling requirements to generate a placement plan for PIFO nodes across PIFO Cores.
* All implementation code presented in the paper has been open-sourced.

## Get Started

### Overview

The complexity and diversity of hierarchical scheduling algorithms necessitate programmable hierarchical schedulers to enable customized traffic management policies. Among various programmable hierarchical scheduling models, the Push-In First-Out (PIFO) Tree and its variants, collectively referred to as Hierarchical PIFO models, have been the most widely studied. However, the PIFO Tree inherently relies on packets as the fundamental scheduling unit. This packet-driven scheduling logic leads to scheduling errors and operational dependencies,  severely undermining both accuracy and efficiency of Hierarchical PIFO models.

Recognizing that hierarchical scheduling fundamentally revolves around link resource allocation across multiple levels,  we propose the resource-driven TRICO (Top-Rank-In Credit-Out) Tree model. By introducing  the novel mechanisms of Top-Rank Commitment and Credit-Based Invoking, the TRICO Tree ensures that link resource consumption  aligns with the intended allocation policies. This approach achieves near-ideal packet scheduling with minor yet significant modifications to the PIFO Tree model. Building on TRICO Tree model,  we design the TRICO Tree Scheduler with multiple heterogeneous physical PIFO queues, which operate in parallel to deliver large-scale, high-throughput programmable hierarchical scheduling. Prototypes implemented on FPGA and ASIC demonstrate the scheduler's effectiveness, achieving multi-fold improvements in throughput and two orders of magnitude enhancements in scheduling accuracy compared with the state-of-the-art PIFO Tree-based schedulers. These results establish the TRICO Tree model as a highly promising solution for programmable hierarchical scheduling.

### Requirements

This repository has hardware and software requirements.

**Hardware Requirements**

* Our testbed evaluation is conducted on the Xilinx Alveo U200 Data Center Accelerator Card.

**Software Requirements**

* ns-3: ns-3.26
* Python: 3.8.10 (the same is best)
* [Vivado Design Suite](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/archive.html) >= 2018.3 (We used the 2018.3 version in our experiments. Newer versions should be compatible with 2018.3, but we have not tested with older versions.)

## License

The project is released under the Apache-2.0 License.

## Links

Below are some links that may also be helpful to you:

- [vPIFO](https://github.com/vPIFO-image/vPIFO)
- [ns-3](https://www.nsnam.org/)
- [Vivado Design Suite](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/archive.html)


