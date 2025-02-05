package lib

import (
	"bytes"
	"context"
	"log"
	"os"
	"sync"
	"time"

	"github.com/0xb10c/rawtx"
	"github.com/btcsuite/btcd/wire"
	"github.com/go-zeromq/zmq4"
)

const BITCOIN = 100_000_000

func StartZMQ(Broadcast chan<- Message) {
	zmqHost := os.Getenv("ZMQ_HOST")

	for {
		subTX := zmq4.NewSub(context.Background())
		subTX.Dial(zmqHost)
		subTX.SetOption(zmq4.OptionSubscribe, "rawtx")

		subBlock := zmq4.NewSub(context.Background())
		subBlock.Dial(zmqHost)
		subBlock.SetOption(zmq4.OptionSubscribe, "rawblock")

		// We get a burst of rawtx after a block is mined. Keep track of hashes
		txHashes := make(map[string]bool)
		var txHashesMutex sync.RWMutex

		// Create error channels to monitor connection status
		txErrors := make(chan error, 1)
		blockErrors := make(chan error, 1)

		log.Printf("Starting TX message loop")
		go func() {
			for {
				msg, err := subTX.Recv()
				if err != nil {
					log.Printf("TX receive error: %v", err)
					txErrors <- err
					return
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

				if satoshis/BITCOIN > 5 {
					message := Message{
						Segment: 7,
						Type:    "tx",
						Value:   satoshis,
					}
					Broadcast <- message
				}
			}
		}()

		log.Printf("Starting Block message loop")
		go func() {
			for {
				_, err := subBlock.Recv()
				if err != nil {
					log.Printf("Block receive error: %v", err)
					blockErrors <- err
					return
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
		}()

		select {
		case <-txErrors:
		case <-blockErrors:
		}

		log.Printf("ZMQ connection lost, retrying in 5 seconds...")
		subTX.Close()
		subBlock.Close()
		time.Sleep(5 * time.Second)
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
