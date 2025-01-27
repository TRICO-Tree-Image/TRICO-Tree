# Project README

## Overview

This project implements a programmable packet scheduler with a credit-based task handling mechanism. The system is composed of several modules that interact to efficiently manage task scheduling. Below is an overview of the files in this folder and their respective roles in the project.

## Files and Their Functions

### 1. `TaskGenerator`
- **Role**: Top-level module responsible for handling the credit mechanism.
- **Functionality**: Converts incoming push/pop requests into tasks that need to be processed. These tasks are then handed off to the `PIFO_SRAM_TOP` module for further processing.

### 2. `PIFO_SRAM_TOP`
- **Role**: Module responsible for organizing connections between different components.
- **Functionality**: Receives tasks from the `TaskGenerator` and forwards them to the `TaskFIFO` for queuing. It ensures proper coordination between the various modules.

### 3. `TaskFIFO`
- **Role**: FIFO buffer for holding tasks.
- **Functionality**: Temporarily stores tasks before they are distributed for execution. This module ensures that tasks are processed in the correct order and are available for the next stage of the pipeline.

### 4. `TaskDistribute`
- **Role**: Task distribution module.
- **Functionality**: Selects tasks from the head of various FIFOs and passes them to the `PIFO` for execution. It ensures that tasks are distributed efficiently based on availability.

### 5. `PIFO`
- **Role**: Main processing unit for task scheduling.
- **Functionality**: Composed of two key components:
  - **`PIFO_SRAM`**: Implements the RPU (Reconfigurable Processing Unit) functionality.
  - **`INFER_SDPRAM`**: Implements the SRAM (Static RAM) functionality.

Together, these components form the core of the scheduling system, managing high-throughput, large-scale, and accurate task processing.