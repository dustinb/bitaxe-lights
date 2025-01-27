package lib

import (
	"encoding/json"
	"log"
	"net/http"
	"time"
)

type CoinDeskResponse struct {
	BPI struct {
		USD struct {
			RateFloat float64 `json:"rate_float"`
		} `json:"USD"`
	} `json:"bpi"`
}

// StartPriceProvider gets the bitcoin price from Coindesk API
// and broadcasts it
func StartPriceProvider(Broadcast chan<- Message) {

	for {
		resp, err := http.Get("https://api.coindesk.com/v1/bpi/currentprice/USD.json")
		if err != nil {
			log.Printf("Error getting price: %v", err)
			continue
		}

		var data CoinDeskResponse
		err = json.NewDecoder(resp.Body).Decode(&data)
		if err != nil {
			log.Printf("Error decoding price: %v", err)
			continue
		}

		price := data.BPI.USD.RateFloat
		Broadcast <- Message{Segment: 7, Type: "price", Value: int64(price)}
		resp.Body.Close()
		time.Sleep(10 * time.Minute)
	}

}
