# README

## About The Project

Etcd3.3 doesn't support fully concurrent read. An expensive read would block cheap reads and writes. It's known that the read request of clients is applied as KV ```range``` requests in the server. The price of read is determined by the number of keys and value size for a single range request. Generally, the more keys it involves, and the larger the value size, the greater pressure it would apply to the database.  

This repo reproduces the [benchmark](https://github.com/etcd-io/etcd/pull/9384#issuecomment-462566915) for the PR [*allow fully concurrent read*](https://github.com/etcd-io/etcd/pull/9384), and explores the influence of cancellation expensive request.  

## Analysis

The expensive read request holds the read lock, which avoids the write request to acquire the write lock.  
Resolved by more fine-grained locks and more timely unlock.

```mermaid
flowchart TD

subgraph Z[" "]
direction LR
  A[MVCC]
end

subgraph ZA[" "]
direction LR
    D[Revision]-->E[TreeIndex]
end

subgraph ZAA[" "]
direction LR
    B[Buffer]-->C[ReadTx/BatchTx]-->G[Backend]
end

subgraph Y[" "]
direction LR
    1[BoltDB]-->2[disk]
end

Z --> ZA --> ZAA --> Y
```

```mermaid
flowchart LR
  A[range request] --> B["`**RLock ReadTx.buffer**`"];
  B--Key-->C[TreeIndex]--revision-->D{buffer};
  D--hit-->E["`**RUnlock ReadTx.buffer**`"];
  D--miss-->F[BoltDB];
  F--value-->E;
  E-->G[return];

  1[put request] --> 2[Lock BatchTx.buffer];
  2--Key-->3[TreeIndex]--revision-->4[generate new revision];
  4--add-->3
  4-->5["`**RLock ReadTx.buffer**`"];
  5--writeback-->6[ReadTx.buffer];
  6-->7{pending commit};
  7--yes-->8[commit to BoltDB];
  7--no-->9
  8-->9["`**RUnlock ReadTx.buffer**`"]
  9-->10[Lock BatchTx.buffer];
  10-->11[return]
```

```mermaid
sequenceDiagram
  participant read request
  participant TreeIndex
  participant txReadBuffer
  participant BoltDB
  note over read request: Rlock ReadTx buffer
  read request->>+TreeIndex: Key
  TreeIndex->>-read request: revision
  read request->>+txReadBuffer: range
  loop for each key
  note over read request: cancel check point
  opt buffer hit
  txReadBuffer->>-read request: value
  end
  alt buffer miss
  read request->>+BoltDB: range
  BoltDB->>-read request: value
  end
  end
  note over read request: RUnlock ReadTx buffer
```

```mermaid
sequenceDiagram
  participant write request
  participant TreeIndex
  participant txReadBuffer
  participant BoltDB
  note over write request: lock BatchTx.buffer
  write request->>+TreeIndex: Key
  TreeIndex->>-write request: revision
  note over write request: generate new revision
  note over write request: ...
  write request->>TreeIndex: add new revision
  note over write request: lock ReadTx.buffer
  write request->>txReadBuffer: writeBack
  opt pending writes
  write request->>BoltDB: commit
  end
  note over write request: Unlock ReadTx.buffer
  note over write request: Unlock BatchTx.buffer
```

## Benchmark Setup

- write 100k KVs with 256B keys and 1KB values
- 1 expensive read per second
- 100 cheap reads per second (<10keys)
- 20 writes per second (1 key)

## Expected Result

### For the result with cancellation, please refer to ```./master/k150k_range_1_2_e2```

Cheap write with concurrent expensive read is blocked significantly.

- expensive read request blocks cheap write request
  ![expensive read request block cheap write request](./etcd_read_block_write.png)
- cheap read request is also blocked
 ![blocked write request blocks cheap read request](./etcd_blocked_write_blocks_read.png)
- Note: expensive read is not extradinary even with 150k KV paris
  ![expensive read](etcd_expensive_read.png)

## Reproduce

- ``Requirements``: Go environment
- To setup etcd server and client

  ```bash
  ./setup.sh
  ```

- To run etcd server with lock contention and proper cancel implemented

  ```bash
  ./cancel/bin/etcd
  ```
  
- To run etcd benchmark

  ```bash
  ./cancel/bin/benchmark
  ```

- The example cancellable API request script is in ```./API/cancel_study/```  
  To run the script

  ```bash
  go run ./API/cancel_study/ <expensive_read_TTL>
  ```

- The example test script is ```test.sh```  
  You may need to manually configure the output directory, and detect what kind of requests to run in the script

  ```bash
  ./test.sh <num_pairs>
  ```

- The example result is ```./master/k150k_range_1_2_e2```
