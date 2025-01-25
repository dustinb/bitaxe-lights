package lib

import (
	"context"
	"encoding/json"
	"log"
	"net/http"
	"os"

	"github.com/coder/websocket"
)

type Message struct {
	Segment int    `json:"segment"`
	Type    string `json:"type"`
	Value   string `json:"value"`
}

type MessageClient interface {
	Write(ctx context.Context, msgType websocket.MessageType, msg []byte) error
}

type Hub struct {
	Clients map[context.Context]MessageClient
}

func (h *Hub) AddClient(ctx context.Context, client MessageClient) {
	h.Clients[ctx] = client
}

func (h *Hub) RemoveClient(ctx context.Context) {
	delete(h.Clients, ctx)
}

func (h *Hub) Broadcast(msg Message) {
	for ctx, client := range h.Clients {
		json, _ := json.Marshal(msg)
		client.Write(ctx, websocket.MessageText, json)
	}
}

func StartWebsocketServer(hub *Hub) {

	fn := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Add CORS headers
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")

		// Handle preflight
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}

		// Accept websocket connection
		c, err := websocket.Accept(w, r, &websocket.AcceptOptions{
			InsecureSkipVerify: true, // Add this line to skip origin check
		})
		if err != nil {
			log.Println(err)
			return
		}
		defer c.CloseNow()

		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		ctx = c.CloseRead(ctx)
		hub.AddClient(ctx, c)
		// Wait until the connection is closed
		<-ctx.Done()
		log.Println("Client disconnected")
		hub.RemoveClient(ctx)
	})

	log.Println("Starting websocket server on port " + os.Getenv("WEBSOCKET_PORT"))

	err := http.ListenAndServe("0.0.0.0:"+os.Getenv("WEBSOCKET_PORT"), fn)
	log.Fatal(err)
}
