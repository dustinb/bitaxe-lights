package lib

import (
	"context"
	"fmt"
	"log"
	"regexp"
	"strconv"
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

	// Last known difficulty for this Bitaxe
	var difficulty int64
	diffRegEx := regexp.MustCompile(`diff (\d+)`)

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

			// Store and broadcast the last known difficulty found
			if strings.Contains(string(msg), "asic_result") {
				matches := diffRegEx.FindStringSubmatch(string(msg))
				if len(matches) > 1 {
					difficulty, _ = strconv.ParseInt(matches[1], 10, 64)
					broadcast <- Message{
						Segment: bitaxe.Segment,
						Type:    "asic_result",
						Value:   difficulty,
					}
				}
				continue
			}

			if strings.Contains(string(msg), "mining.submit") {
				broadcast <- Message{
					Segment: bitaxe.Segment,
					Type:    "mining.submit",
					Value:   difficulty,
				}
				continue
			}

			if strings.Contains(string(msg), "mining.notify") {
				broadcast <- Message{
					Segment: bitaxe.Segment,
					Type:    "mining.notify",
				}
			}

		}
	}
}
