package com.wifigamepad.network

object Protocol {
    const val MAGIC: Byte = 0x57 // 'W'
    const val DEFAULT_SERVER_PORT = 52150
    const val DISCOVERY_PORT = 52151

    const val PACKET_TYPE_INPUT: Byte = 0x01
    const val PACKET_TYPE_PING: Byte = 0x02
    const val PACKET_TYPE_PONG: Byte = 0x03
    const val PACKET_TYPE_DISCOVERY: Byte = 0x04
    const val PACKET_TYPE_DISCOVERY_ACK: Byte = 0x05
    const val PACKET_TYPE_PAIR_REQ: Byte = 0x06
    const val PACKET_TYPE_PAIR_RESP: Byte = 0x07
    const val PACKET_TYPE_DISCONNECT: Byte = 0x08
    const val PACKET_TYPE_RUMBLE: Byte = 0x09

    const val INPUT_PACKET_SIZE = 28

    // Button bitmasks (Matching XInput)
    const val BTN_DPAD_UP: Short        = 0x0001
    const val BTN_DPAD_DOWN: Short      = 0x0002
    const val BTN_DPAD_LEFT: Short      = 0x0004
    const val BTN_DPAD_RIGHT: Short     = 0x0008
    const val BTN_START: Short          = 0x0010
    const val BTN_BACK: Short           = 0x0020
    const val BTN_LEFT_THUMB: Short     = 0x0040 // L3
    const val BTN_RIGHT_THUMB: Short    = 0x0080 // R3
    const val BTN_LEFT_SHOULDER: Short  = 0x0100 // LB
    const val BTN_RIGHT_SHOULDER: Short = 0x0200 // RB
    const val BTN_GUIDE: Short          = 0x0400 // Home
    const val BTN_A: Short              = 0x1000.toShort()
    const val BTN_B: Short              = 0x2000.toShort()
    const val BTN_X: Short              = 0x4000.toShort()
    const val BTN_Y: Short              = 0x8000.toShort()
}
