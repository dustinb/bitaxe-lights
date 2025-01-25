package lib

import (
	"bytes"
	"context"
	"fmt"
	"log"
	"os"
	"sync"

	"github.com/0xb10c/rawtx"
	"github.com/btcsuite/btcd/wire"
	"github.com/go-zeromq/zmq4"
)

const BITCOIN = 100_000_000

func StartZMQ(Broadcast chan<- Message) {
	zmqHost := os.Getenv("ZMQ_HOST")

	subTX := zmq4.NewSub(context.Background())
	defer subTX.Close()
	subTX.Dial(zmqHost)
	subTX.SetOption(zmq4.OptionSubscribe, "rawtx")

	subBlock := zmq4.NewSub(context.Background())
	defer subBlock.Close()
	subBlock.Dial(zmqHost)
	subBlock.SetOption(zmq4.OptionSubscribe, "rawblock")

	// We get a burst of rawtx after a block is mined. Keep track of hashes
	txHashes := make(map[string]bool)
	var txHashesMutex sync.RWMutex

	log.Printf("Starting TX message loop")
	go func() {
		for {
			msg, err := subTX.Recv()
			if err != nil {
				log.Printf("receive error: %v", err)
				continue
			}

			raw := []byte(msg.Frames[1])
			satoshis, hash := GetTransactionValue(raw)

			txHashesMutex.RLock()
			_, exists := txHashes[string(hash)]
			txHashesMutex.RUnlock()

			if exists {
				continue
			}

			txHashesMutex.Lock()
			txHashes[string(hash)] = true
			txHashesMutex.Unlock()

			if satoshis/BITCOIN > 10 {
				message := Message{
					Segment: 7,
					Type:    "tx",
					Value:   fmt.Sprintf("%d", satoshis),
				}
				Broadcast <- message
			}
		}
	}()

	log.Printf("Starting Block message loop")
	for {
		_, err := subBlock.Recv()
		if err != nil {
			log.Printf("receive error: %v", err)
			continue
		}
		message := Message{
			Segment: 7,
			Type:    "block",
		}

		txHashesMutex.Lock()
		txHashes = make(map[string]bool)
		txHashesMutex.Unlock()

		Broadcast <- message
	}
}

func GetTransactionValue(raw []byte) (int64, []byte) {
	var value int64
	wireTx := wire.MsgTx{}
	wireTx.Deserialize(bytes.NewReader(raw))
	tx := rawtx.Tx{}
	tx.FromWireMsgTx(&wireTx)

	for _, out := range tx.Outputs {
		value += out.Value
	}
	return value, tx.Hash
}
