# Anyplay
Playback any media anywhere

For a long I am pretty annoyed that playing back media on different devices
is usually a total pain. I have quite a bunch off different devices in my
house and in my office, most of them are either capable of media playback or
contain some media I would like to play back. But not ncessarily all of it
at the same time! There is some media on my laptop but the speakers are,
well, laptop speakers. But there is my desktop workstation which is
connected to a larger set of nice speakers. So I would have to copy the data
over to the desktop and start a media player there. But if I am working on
my laptop (for some reason) this means I have to from time to time switch my
attention over to the desktop, control the playback from there and then
return to my laptop. Not very convenient.

Worse is playback of larger video files. I would first have to transfer it
to some common storage that also the video player can access, then search it
on the video player and finally start playback. If we are talking about a 1.5h
fullHD recording this can take a while.

All that does not make much sense. All of these devices are usually
connected to my local network, all can easily stream (sending and receiving)
data over that network, some have the data to send, some have appropriate
capabilities for playback. So why not send the data for playback to a
receiver and just have - played back?

After months of pondering and research I found that there is indeed no
simple solution to the problem - and I tried quite a bit. The closest I
found was DLNA/UPNP but this has become pretty uncommon by now. The
implementations and even commercial devices I found do not really work that
well - discovery often does not work reliably (but is the only way),
playback often hangs or stutters, and the specification is parts proprietary
and *complicated*. Why make it so complicated for such a simple task? The
closest thing I found so far is Chromecast but also this is pretty
complicated and also pretty proprietary.

So I came up with my own which should hopefully be 'good enough' and should
be possible to implement on pretty my any device that can drive a network
interface: Anyplay.

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
### Start the receiver
The Anyplay receiver will listen on port 4424 for incoming conection and
then serve the requests:
```
 # anyplay -l
```

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
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "source",
  "param": "http://<local-ip>/music.mp3"
}
```

Following this the Anyplay receiver will open the URL, stream the data from
it and play back the media, if it can process it.

### Control remote playback

Pause playback:
```
 # anyplay -c pause <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "pause"
  "param": ""
}
```

Start playback:
```
 # anyplay -c play <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "play"
  "param": ""
}
```

Stop playback:
```
 # anyplay -c stop <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "stop"
  "param": ""
}
```

Inrease audio playback volume:
```
 # anyplay -c vol+ <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol+"
  "param": ""
}
```

Decrease audio playback volume:
```
 # anyplay -c vol- <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol-"
  "param": ""
}
```

Set audio playback volume to a certain level:
```
 # anyplay -c vol -p <val> <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "vol-"
  "param": "<val>"
}
```

If avaiable, play next source:
```
 # anyplay -c next <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "next"
  "param": ""
}
```

If avaiable, play previous source:
```
 # anyplay -c prev <remote-ip>
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "command": "prev"
  "param": ""
}
```

 ...

JSON responses can be, for successful command:

```
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "result": "OK"
}
```

or for an unsuccessful / failed command:

```
{
  "jsonrpc": "2.0",
  "id": "id",
  "service": "Anyplay",
  "version": "1.0",
  "result": "FAIL"
}
```


### Testing

`curl -X POST -H "Content-Type: application/json" -d "{\"protocol\": \"1.0\"}" http://192.168.178.76:4424`

`curl -X POST -H "Content-Type: application/json" -d @play.json http://192.168.178.94:49485`

### References

[] mDNS host and service discovery

[] assigned numbers, for IP port assignent:
https://www.iana.org/form/ports-services

[] intended user port:
4424

