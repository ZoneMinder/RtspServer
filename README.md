## RtspServer

A lightweight RTSP server and pusher library written in C++11.

This repository is maintained by the [ZoneMinder](https://github.com/ZoneMinder) project and is the RTSP server used by ZoneMinder's `zms`/monitor streaming. It originated as a fork of [PHZ76/RtspServer](https://github.com/PHZ76/RtspServer); the original project has been inactive since 2023, so this repository is now developed independently. Many thanks to [PHZ76](https://github.com/PHZ76) for the original implementation.

## Features

- Linux, Windows, and macOS platforms
- Video: H.264, H.265, VP8, AV1 (including a passthrough mode for pre-formed RTP payloads)
- Audio: AAC, G.711 A-law, G.711 µ-law
- Simultaneous audio and video in one session
- Unicast (RTP over UDP, RTP over RTSP/TCP interleaved) and multicast
- RTSP push/publish over TCP
- Digest authentication
- Connection keep-alive/heartbeat detection (unicast)
- `sprop-parameter-sets` in H.264/H.265 SDP for faster stream start-up

## Requirements

- A C++11 compiler (gcc 4.8+/clang or Visual Studio 2015+)
- CMake 3.10 or later

## Examples

- [rtsp_server.cpp](example/rtsp_server.cpp) — serve a live stream
- [rtsp_h264_file.cpp](example/rtsp_h264_file.cpp) — serve an H.264 elementary stream from a file
- [rtsp_pusher.cpp](example/rtsp_pusher.cpp) — push a stream to another RTSP server

## Architecture

![architecture](pic/1.pic.JPG)

## FAQ

- **How do I stream a media file (mp4, mkv, ...)?** This library does not parse container formats. Use FFmpeg (or similar) to demux/read frames and feed them to a `MediaSession`. `rtsp_h264_file.cpp` shows the pattern for a raw H.264 stream.
- **The RTSP client connects but receives no data.** Capture the session with Wireshark and inspect the RTP packets; also double-check byte order (endianness) when packing frames on unusual platforms.

## Acknowledgements

- [PHZ76](https://github.com/PHZ76), author of the original RtspServer
- MD5 implementation from [websocketpp](https://github.com/zaphoyd/websocketpp)
- All [contributors](https://github.com/ZoneMinder/RtspServer/graphs/contributors)

## License

[MIT License](LICENSE)

## Support

Please open an [issue](https://github.com/ZoneMinder/RtspServer/issues) in this repository. For ZoneMinder-specific questions, use the [ZoneMinder forums](https://forums.zoneminder.com) or the main [ZoneMinder repository](https://github.com/ZoneMinder/zoneminder).
