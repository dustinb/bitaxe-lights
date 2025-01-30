package lib

import (
	"context"
	"fmt"
	"log"
	"strings"

	"github.com/coder/websocket"
)

func ConnectToBitaxe(bitaxe Bitaxe, broadcast chan<- Message, ctx context.Context) {

	// Websocket context
	wsCtx, cancel := context.WithCancel(ctx)
	defer cancel()

	url := fmt.Sprintf("ws://%s/api/ws", bitaxe.IP)
	c, _, err := websocket.Dial(wsCtx, url, nil)
	if err != nil {
		log.Printf("Dial error: %v", err)
		return
	}
	defer c.CloseNow()

	for {
		select {
		case <-ctx.Done():
			return
		default:
			_, msg, err := c.Read(wsCtx)
			if err != nil {
				log.Printf("websocket error: %v", err)
				c.CloseNow()
				return
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
