cmake_minimum_required(VERSION 3.10)
project(RtspServer)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/net/" "${CMAKE_CURRENT_SOURCE_DIR}/src/xop/")

file(GLOB rtspserver_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/xop/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/xop/*.h"
  )

set(RTSPSERVER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Acceptor.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/BufferReader.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/BufferWriter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/EpollTaskScheduler.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/EventLoop.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Logger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/MemoryManager.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/NetInterface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/SelectTaskScheduler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/SocketUtil.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TcpConnection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TcpServer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Timer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/Timestamp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TaskScheduler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net/TcpSocket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/AACSource.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/AV1Source.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/DigestAuthentication.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/G711ASource.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/H264Source.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/MediaSession.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/H264Parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/H265Source.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/RtpConnection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/RtspConnection.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/RtspMessage.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/RtspPusher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/xop/RtspServer.cpp
)

add_library(RtspServer STATIC ${rtspserver_sources})
add_library(RtspServer::RtspServer ALIAS RtspServer)

target_include_directories(RtspServer
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/xop/"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdpart/"
   )

target_link_libraries(RtspServer
  PRIVATE
    zm-dependency-interface)

