# Bitaxe 6 Pack

Inspired by the Bithalo https://github.com/IamGPIO/BitHalo-201-204-Bitaxe

# Operation

There are 6 LED segments using NeoPixel LEDs.  The network is scanned for Bitaxe devices with hostname containing _led1, _led2, etc.  A web socket is opened to each device to identify mining.submit and mining.notify events.  A small go program is used to listen these events.

The go server also listens for bitcoind events of rawtx and rawblock via zeromq.  Add these to your bitcoind.conf:

```
zmqpubrawtx=tcp://0.0.0.0:3000
zmqpubrawblock=tcp://0.0.0.0:3000
zmqpubsequence=tcp://0.0.0.0:3000
```

# 3D Prints

Frame diffuser printed in transparent PLA

https://cad.onshape.com/documents/0c7a2b73a8844e0859590ea8/w/2da60e5a1068f76b5871b326/e/95ed48e1cc17c5f64a4e80ce?renderMode=0&uiState=67953218ecbe69225db10fe7
