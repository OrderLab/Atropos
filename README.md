# Artifact Evaluation Guide

Welcome to the artifact evaluation guide for Atropos (SOSP'25). This document outlines the procedures needed to reproduce our results and guides you through the key experiments presented in the paper.

## Overview

Atropos is an overload control system designed to manage application resource overload by proactively canceling tasks responsible for monopolizing critical resources. It achieves this by continuously monitoring resource usage, identifying tasks causing significant resource contention, and selectively canceling them to maintain performance targets. Atropos provides unified abstractions to manage diverse application resources and leverages existing cancellation mechanisms in applications to safely kill problematic requests. 



## Getting Started

### Cloning the Repository

#### Option 1: Clone with submodules in one command
```bash
git clone --recurse-submodules git@github.com:OrderLab/Atropos.git
```

#### Option 2: Clone first, then initialize submodules
```bash
git clone git@github.com:OrderLab/Atropos.git
cd Atropos
git submodule update --init --recursive
```

### Updating Submodules

If you already have the repository cloned but need to update the submodules:

```bash
git submodule update --init --recursive
```

To pull the latest changes from all submodules:

```bash
git submodule update --remote
```

## Project Structure

The artifact is organized into the following main components:

### Core Implementation
- `project/atropos-c/` - C/C++ Atropos library and system integrations
  - `atropos/` - Core Atropos library implementation
  - `atropos-apache/` - Modified Apache HTTP Server with Atropos support
  - `atropos-mysql/` - Modified MySQL database with Atropos integration
  - `atropos-postgresql/` - Modified PostgreSQL database with Atropos support
  - `sysbench-atropos/` - Sysbench for performance evaluation

- `project/atropos-java/` - Java-based implementation and benchmarks
  - `atropos_java_code/` - Java Atropos library
  - `atropos_exp/` - Experimental evaluation code
  - `elasticsearch/` - Elasticsearch integration and benchmarks
  - `solr/` - Apache Solr integration and benchmarks
  - `scripts/` - Utility scripts for experiments and evaluation

Each component includes its own build instructions and documentation. Refer to the individual README files in each directory for specific setup and usage instructions.

## Prerequisites

Before proceeding with the experiments, ensure you have the following installed:

- **Operating System**: Linux (Ubuntu 18.04+ recommended) or macOS
- **Languages & Runtimes**:
  - GCC/Clang (for C/C++ components)
  - Java 8+ (for Java components)
  - Python 3.6+ (for scripts and utilities)
- **Build Tools**:
  - CMake 3.10+
  - Make
  - Maven or Gradle (for Java builds)
- **Dependencies**:
  - MySQL development libraries
  - PostgreSQL development libraries
  - Apache HTTP Server development headers

## Experiments

**[Java Experiments Setup & Execution Guide](https://github.com/easonycliu/AutoCancelProject/blob/master/README.md)**


## Citation

If you use this artifact in your research, please cite our paper:

```bibtex
@inproceedings{atropos2025,
  title={Atropos: A Library for Mitigating Application Resource Overload},
  author={[Authors]},
  booktitle={Proceedings of the 30th ACM Symposium on Operating Systems Principles},
  series={SOSP '25},
  year={2025},
  publisher={ACM}
}
```

## Contact

For questions about this artifact or the paper, please contact:
- [Author names and emails to be added]
- Open an issue in this repository for technical problems

## License

This project is licensed under [License to be specified]. See individual component directories for specific licensing information.
