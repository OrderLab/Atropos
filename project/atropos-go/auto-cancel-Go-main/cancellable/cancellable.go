package cancellable

import (
	"bytes"
	"context"
	"runtime"
	"strconv"
	"time"
)

// type GID uint64

type Cancallable struct {
	isCancelable bool
	ctx          context.Context
	cancel       context.CancelFunc
	GID          uint64
	StartTime    time.Time
	ExecTime     int64 // us  TODO
}

type CMap struct{ M map[uint64]Cancallable }

type CancellableMap interface {
	CreateCancellable(ctx context.Context, isCancellable bool) (context.Context, uint64, context.CancelFunc)
	GetCancellable(id uint64) (Cancallable, bool)
	RemoveCancellable(id uint64)
	GetSize() int // for checking
}

var Cancellable_map CMap

func getGID() uint64 {
	b := make([]byte, 64)
	b = b[:runtime.Stack(b, false)]
	b = bytes.TrimPrefix(b, []byte("goroutine "))
	b = b[:bytes.IndexByte(b, ' ')]
	n, _ := strconv.ParseUint(string(b), 10, 64)
	return n
}

func (cm CMap) CreateCancellable(ctx context.Context, isCancellable bool) (context.Context, uint64, context.CancelFunc) {
	cctx, cancel := context.WithCancel(ctx)
	gID := getGID()
	cm.M[gID] = Cancallable{isCancellable, cctx, cancel, gID, time.Now(), 0}
	return cctx, gID, cancel
}

func (cm CMap) GetCancellable(id uint64) (Cancallable, bool) {
	if v, ok := cm.M[id]; ok {
		return v, ok
	} else {
		return Cancallable{false, nil, nil, 0, time.Now(), 0}, false
	}
}

func (cm CMap) RemoveCancellable(id uint64) {
	delete(cm.M, id)
}

func (cm CMap) GetSize() int {
	return len(cm.M)
}
