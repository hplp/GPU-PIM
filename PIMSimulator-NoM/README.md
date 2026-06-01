# Distributed Convolution and Network-on-Memory Extensions (Work in Progress)

## Acknowledgement

This work is built on top of the [Aquabolt-XL PIM simulator](https://github.com/SAITPublic/PIMSimulator), which was originally developed by Samsung and its collaborators. The original simulator, ISA, benchmark implementations, and documentation are the work of the Aquabolt-XL authors.

This repository contains a research copy of the simulator that has been modified for experimentation with distributed convolution and Network-on-Memory (NoM) support.

The contents described in this document represent ongoing research extensions developed at the High-Performance Low-Power (HPLP) lab at UVA and are not part of the original Aquabolt-XL simulator release.

## Project Context

This work was developed as part of the GPU-PIM Co-design project funded by the Laboratory for Physical Sciences (LPS) and Booz Allen Hamilton (BAH).

## Disclaimer

⚠️ **Research Prototype – Work in Progress**

The code associated with these extensions is currently under active development. It is being provided to document the ongoing research effort and the architectural modifications being explored.

The implementation is incomplete and is not expected to compile, execute correctly, or maintain compatibility with the existing benchmark suite.

## Objective

The goal of this work is to evaluate distributed convolution execution on HBM-based Processing-in-Memory (PIM) architectures and investigate the benefits of a proposed Network-on-Memory (NoM) for supporting communication between banks.

The long-term target application is a VGG13-style convolution block:

```text
Conv -> ReLU -> Conv -> ReLU -> MaxPool
```

The implementation is designed such that data placement and computation patterns can be reused across multiple convolution layers with minimal data movement.

## Current Progress

### Distributed Convolution Support

The following functionality has been partially implemented:

- Input placement across banks aligned with PIM units.
- Convolution execution using PIM compute kernels.
- Generation of local partial results within PIM units.
- Data layouts intended to support future multi-layer convolution execution.

### Architectural Extensions

The following architectural modifications are currently being integrated:

- Expansion of GRF_A and GRF_B register files (target size: 16 entries) to support distributed convolution workloads.
- Addition of a GRF_C register file.
- Extensions to the PIM ISA.
- Support for reduction operations.
- Support for multiple jumps within CRF programs.
- Modifications to control logic required by the larger register files.
- Experimental changes to the mapping of PIM units and banks, allowing banks associated with a PIM unit to reside in different bank groups.

## Incomplete Functionality

The following features are still under development:

### Network-on-Memory (NoM)

- Halo exchanges between neighboring bank partitions.
- Transfer of partial results between PIM units.
- Inter-bank communication mechanisms.
- Final reduction of partial results.

### Application Integration

- End-to-end distributed convolution execution.
- Validation against software reference implementations.
- Integration of multiple convolution layers.
- Execution of complete VGG13 blocks.

## Compatibility Notes

Because of the ongoing ISA, register-file, and control-path modifications, existing Aquabolt benchmarks may not execute correctly.

Compatibility with the original benchmark suite has not yet been re-established.

One objective of the future work is to demonstrate that the modified PIM-bank organization preserves the behavior and performance of existing workloads while enabling efficient NoM-assisted distributed convolution execution.

## Planned Future Work

- Complete halo-exchange support.
- Complete partial-result reduction support.
- Integrate the proposed Network-on-Memory architecture.
- Evaluate distributed convolution workloads.
- Extend support toward complete VGG13 blocks.
- Explore Transformer workloads utilizing NoM-assisted distributed execution.

## Authors

Kavish Ranawella 
Ersin Cukurtas 

High-Performance Low-Power (HPLP) Laboratory  
Department of Electrical and Computer Engineering  
University of Virginia