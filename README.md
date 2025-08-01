**Artifact for the Atropos Work**


## About This Artifact

This artifact includes:
- **C/C++ Implementation**: Core Atropos library and system integrations (`project/atropos-c/`)
- **Java Implementation**: Java-based simulation and benchmarks (`project/atropos-java/`)
- **Database Integration**: Modified versions of MySQL and PostgreSQL with Atropos support
- **Web Server Integration**: Modified Apache HTTP Server with Atropos support
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

### Troubleshooting Submodules

If you encounter issues with empty submodule directories (especially nested submodules in the Java component):

1. **Check submodule status**:
   ```bash
   git submodule status --recursive
   ```

2. **Force reinitialize all submodules**:
   ```bash
   git submodule deinit --all -f
   git submodule update --init --recursive
   ```

3. **If nested submodules are still empty**, navigate to the submodule and initialize manually:
   ```bash
   cd project/atropos-java
   git submodule update --init --recursive
   ```

**Note**: The Java submodule (`project/atropos-java/`) contains multiple nested submodules. Ensure all are properly initialized using the `--recursive` flag.

## Project Structure

The artifact is organized into the following main components:

### Core Implementation
- `project/atropos-c/` - C/C++ Atropos library and system integrations
  - `atropos/` - Core Atropos library implementation
  - `atropos-apache/` - Modified Apache HTTP Server with Atropos support
  - `atropos-mysql/` - Modified MySQL database with Atropos integration
  - `atropos-postgresql/` - Modified PostgreSQL database with Atropos support
  - `sysbench-atropos/` - Sysbench for performance evaluation

- `project/atropos-java/` - Java-based implementation and benchmarks (submodule with nested submodules)
  - `atropos_java_code/` - Java Atropos library (nested submodule)
  - `atropos_exp/` - Experimental evaluation code (nested submodule)
  - `elasticsearch/` - Elasticsearch integration and benchmarks (nested submodule)
  - `solr/` - Apache Solr integration and benchmarks (nested submodule)
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
