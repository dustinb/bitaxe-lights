package lib

import (
	"context"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/coder/websocket"
)

func ConnectToBitaxe(bitaxe Bitaxe, broadcast chan<- Message) {

	var connects int
	for connects < 10 {
		connects++

		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		url := fmt.Sprintf("ws://%s/api/ws", bitaxe.IP)
		c, _, err := websocket.Dial(ctx, url, nil)
		if err != nil {
			log.Printf("Dial error: %v", err)
			time.Sleep(time.Second * 10)
			continue
		}
		defer c.CloseNow()

		for {
			_, msg, err := c.Read(ctx)
			if err != nil {
				log.Printf("websocket error: %v", err)
				c.CloseNow()
				time.Sleep(time.Second * 10)
				break
			}
			var msgType string
			if strings.Contains(string(msg), "mining.submit") {
				msgType = "mining.submit"
			} else if strings.Contains(string(msg), "mining.notify") {
				msgType = "mining.notify"
			}
			if msgType != "" {
				message := Message{
					Segment: bitaxe.Segment,
					Type:    msgType,
				}
				broadcast <- message
			}
		}
	}
}
