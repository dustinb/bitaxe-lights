package lib

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
)

type Info struct {
	Temp              float64 `json:"temp"`
	VRTemp            float64 `json:"vrTemp"`
	HashRate          float64 `json:"hashRate"`
	CoreVoltage       int     `json:"coreVoltage"`
	CoreVoltageActual int     `json:"coreVoltageActual"`
	Frequency         int     `json:"frequency"`
	Hostname          string  `json:"hostname"`
	AsicCount         int     `json:"asicCount"`
	SmallCoreCount    int     `json:"smallCoreCount"`
	OverHeatMode      bool    `json:"overheat_mode"`
	FanSpeed          int     `json:"fanSpeed"`
	MacAddr           string  `json:"macAddr"`
}

type Bitaxe struct {
	IP       string
	Hostname string
	MacAddr  string
	Segment  int
}

const DHCP_START = 100
const DHCP_END = 249

func GetSystemInfo(ip string) Info {
	client := http.Client{}
	client.Timeout = 2 * time.Second
	resp, err := client.Get("http://" + ip + "/api/system/info")
	if err != nil {
		return Info{}
	}
	body, _ := io.ReadAll(resp.Body)
	info := Info{}
	json.Unmarshal(body, &info)
	return info
}

// ScanNetwork scans the network for the Bitaxe
func ScanNetwork() []Bitaxe {
	var bitaxes []Bitaxe
	var mutex sync.Mutex
	var waitgroup sync.WaitGroup

	log.Printf("Scanning network for Bitaxes")
	addrs, _ := net.InterfaceAddrs()
	for _, address := range addrs {
		host, _ := address.(*net.IPNet)
		if host.IP.IsLoopback() {
			continue
		}
		// Check for IPv4
		if host.IP.To4() == nil {
			continue
		}
		log.Print(host.IP.String())
		octets := strings.Split(host.IP.String(), ".")
		network := octets[0] + "." + octets[1] + "." + octets[2] + ".%d"

		for i := DHCP_START; i < DHCP_END+1; i++ {
			ip := fmt.Sprintf(network, i)
			waitgroup.Add(1)
			go func() {
				log.Printf("Scanning %s", ip)
				defer waitgroup.Done()
				info := GetSystemInfo(ip)
				if info.Hostname != "" {
					segment := 0
					if matches := regexp.MustCompile(`_led(\d+)`).FindStringSubmatch(info.Hostname); len(matches) > 1 {
						log.Printf("Found Bitaxe LED: %s", info.Hostname)
						segment, _ = strconv.Atoi(matches[1])
						bitaxe := Bitaxe{IP: ip, Hostname: info.Hostname, MacAddr: info.MacAddr, Segment: segment}
						mutex.Lock()
						bitaxes = append(bitaxes, bitaxe)
						mutex.Unlock()
					}
				}
			}()
		}
	}
	waitgroup.Wait()
	return bitaxes
}
