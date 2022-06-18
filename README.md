# Anyplay
Playback media anywhere

## Idea
The basic idea is actually pretty simple. These days almost any device can
hook up to an IP network and either access or serve http. Some devices can
store data and supply that data via http some other devices cna read data
from http and decode and playback these data streams - audio, video etc.
Serving data via http can be done on non privileged ports (bbelow 1024),
accepting incoming http requests too.

The Anyplay service consist of two parties: A receiver and a sender.

An Anyplay receiver accepts an http connection on the assigned port. Via
http a JSON object is sent to the receiver. The JSON object can either
describe a data source (also using http) or a command. A data source
describes a data stream URL that serves media data via the http protocol. A
command is a JSON encapsulated command instruction the receiver - like
start/pause/stop playback, increase/decrease volume, previous/next title
(depending on source) etc.

An Anyplay sender serves can either serve local media data via http on an
unprivileged port (>49150) or issue commands (JSON encapsulated) via http
to an Anyplay receiver, or both.

Anyplay is intended to be used in a local network only, like a home or
office environment. It assumed that local network participants can be
trusted and so there are no security precautions, anyone connected to the
local network can stream meadia to an Anyplay receiver or take control. This
means that no authentication is used and also plain http instead of https.

These assumptions are also made to lower the resources for implementation.
An Anyplay receiver can be implemented on a simple microcontroller e.g. for
plain audio playback. More complex implementaions can be implmented e.g.
using Linux, capable of also high resolution video playback.

Main goal of Anyplay is ease of implementation and use as well as robustness
of the service.


## Example
### Playback of local music file
A user wants to play back a local music file "music.mp3" on a remote Anyplay
receiver device with `<remote-ip>` (remote is probably connected to a decent
HiFi system):
```
 # anyplay music.mp3 <remote-ip>
```
The local Anyplay application will then do two things:
1. spawn a local http server serving the file "music.mp3" on a random
unpriviliged IP port (>49150)
2. it will open an http connection to `<remote-ip>` on port 4424 and send a
JSON encapsulated media source block containing the URL to the locally
spawned http server serving the file, e.g.:
  `http://<local-ip>/music.mp3`

```
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "source"
  "param": "http://<local-ip>/music.mp3",
}
```

Following this the Anyplay receiver will open the URL, stream the data from
it and play back the media, if it can process it.

### Control remote playback

Pause playback:
```
 # anyplay -c pause <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "pause"
  "param": "",
}
```

```
 # anyplay -c play <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "play"
  "param": "",
}
```

```
 # anyplay -c stop <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "stop"
  "param": "",
}
```

```
 # anyplay -c vol+ <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol+"
  "param": "",
}
```

```
 # anyplay -c vol- <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol-"
  "param": "",
}
```

```
 # anyplay -c vol -p <val> <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol-"
  "param": "<val>",
}
```

```
 # anyplay -c next <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "next"
  "param": "",
}
```

```
 # anyplay -c prev <remote-ip>
{
  "service": "Anyplay",
  "version": "1.0",
  "command": "prev"
  "param": "",
}
```
 ...




References:
[] assigned numbers, for IP port assignent:
https://www.iana.org/form/ports-services
[] intended user port:
4424

