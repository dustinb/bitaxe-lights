package main

import (
	"context"
	"log"
	"time"

	"github.com/joho/godotenv"
	"oldbute.com/axe_lights/lib"
)

func main() {
	godotenv.Load(".env")

	// Messages sent to this channel are broadcast to all clients
	Broadcast := make(chan lib.Message)

	// Our connected clients (should be the Lights Controller)
	hub := &lib.Hub{
		Clients: make(map[context.Context]lib.MessageClient),
	}

	// Broadcast messages to all clients
	go func() {
		for msg := range Broadcast {
			log.Printf("Broadcasting message: %+v", msg)
			hub.Broadcast(msg)
		}
	}()

	go lib.StartWebsocketServer(hub)

	go lib.StartPriceProvider(Broadcast)

	go lib.StartZMQ(Broadcast)

	for {
		ctx, cancel := context.WithCancel(context.Background())
		bitaxes := lib.ScanNetwork()
		log.Printf("Found %d Bitaxes", len(bitaxes))
		for _, bitaxe := range bitaxes {
			go lib.ConnectToBitaxe(bitaxe, Broadcast, ctx)
		}

		// re-scan every x minutes
		time.Sleep(time.Minute * 5)
		cancel() // This will signal all ConnectToBitaxe goroutines to stop
	}
}
