package main

import (
	"context"
	"log"

	"github.com/joho/godotenv"
	"oldbute.com/axe_lights/lib"
)

func main() {
	godotenv.Load(".env")

	// All messages sent to this channel are broadcast to all clients
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

	bitaxes := lib.ScanNetwork()
	log.Printf("Found %d Bitaxes", len(bitaxes))
	for _, bitaxe := range bitaxes {
		go lib.ConnectToBitaxe(bitaxe, Broadcast)
	}

	lib.StartZMQ(Broadcast)
}
