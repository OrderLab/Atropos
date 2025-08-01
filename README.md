**Artifact for the Atropos Work**


## About This Artifact

This artifact includes:
- **C/C++ Implementation**: Core autocancellation library and system integrations (`project/autocancel-c/`)
- **Java Implementation**: Java-based simulation and benchmarks (`project/autocancel-java/`)
- **Database Integration**: Modified versions of MySQL and PostgreSQL with autocancellation support
- **Web Server Integration**: Modified Apache HTTP Server with autocancellation support
- **Benchmarking Tools**: Sysbench integration and search engine benchmarks (Elasticsearch, Solr)

## Getting Started

### Cloning the Repository

This repository contains submodules. To properly clone the repository with all submodules, use one of the following methods:

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
- `project/autocancel-c/` - C/C++ autocancellation library and system integrations
  - `autocancel/` - Core autocancellation library implementation
  - `autocancel-apache/` - Modified Apache HTTP Server with autocancellation support
  - `autocancel-mysql/` - Modified MySQL database with autocancellation integration
  - `autocancel-postgresql/` - Modified PostgreSQL database with autocancellation support
  - `sysbench-autocancel/` - Sysbench for performance evaluation

- `project/autocancel-java/` - Java-based implementation and benchmarks (submodule)
  - `autocancel-java/` - Java autocancellation library
  - `autocancel_exp/` - Experimental evaluation code
  - `elasticsearch/` - Elasticsearch integration and benchmarks
  - `solr/` - Apache Solr integration and benchmarks
  - `scripts/` - Utility scripts for experiments and evaluation

Each component includes its own build instructions and documentation. Refer to the individual README files in each directory for specific setup and usage instructions.

## Citation

If you use this artifact in your research, please cite our paper:

```bibtex
@inproceedings{atropos2025,
  title={Atropos: Autocancellation for Improved System Performance},
  author={[Authors]},
  booktitle={Proceedings of the ACM SIGOPS 30th Symposium on Operating Systems Principles},
  year={2025}
}
```
