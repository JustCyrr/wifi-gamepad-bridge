package com.wifigamepad.network

import android.os.Process
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

typealias RumbleListener = (largeMotor: Int, smallMotor: Int) -> Unit

class UdpSender {
    private var socket: DatagramSocket? = null
    private var serverAddress: InetAddress? = null
    private var serverPort: Int = Protocol.DEFAULT_SERVER_PORT

    // Dedicated sender thread
    private var sendThread: Thread? = null
    private var recvThread: Thread? = null
    private val isRunning = AtomicBoolean(false)

    // Current State (written by UI thread, read by sender thread)
    @Volatile var sessionToken: Short = 0x8F4A.toShort()
    @Volatile var buttons: Short = 0
    @Volatile var triggerLeft: Byte = 0
    @Volatile var triggerRight: Byte = 0
    @Volatile var thumbLx: Short = 0
    @Volatile var thumbLy: Short = 0
    @Volatile var thumbRx: Short = 0
    @Volatile var thumbRy: Short = 0
    @Volatile var lastEventTimestampUs: Long = 0

    // Telemetry & Metrics
    private val sequence = AtomicInteger(0)
    private val packetsThisSec = AtomicInteger(0)
    @Volatile var totalPacketsSent: Long = 0L
    @Volatile var currentRateHz: Int = 0
    @Volatile var lastRttMs: Float = 0f

    @Volatile
    var isConnected = false
        private set

    var rumbleListener: RumbleListener? = null

    fun connect(ipAddress: String, port: Int = Protocol.DEFAULT_SERVER_PORT) {
        disconnect()
        try {
            serverAddress = InetAddress.getByName(ipAddress)
            serverPort = port
            socket = DatagramSocket().apply {
                trafficClass = 0x00 // Standard Best Effort: 0 router policer drops!
                sendBufferSize = 65536
                receiveBufferSize = 65536
            }

            sequence.set(0)
            packetsThisSec.set(0)
            totalPacketsSent = 0L
            currentRateHz = 0
            isRunning.set(true)
            isConnected = true

            // 1. High-Priority Sender Thread (Deterministic 125 Hz Clock Pacing)
            sendThread = Thread {
                Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)
                val buffer = ByteBuffer.allocate(Protocol.INPUT_PACKET_SIZE).apply {
                    order(ByteOrder.LITTLE_ENDIAN)
                }
                val pingBuffer = ByteBuffer.allocate(24).apply {
                    order(ByteOrder.LITTLE_ENDIAN)
                }

                val packet = DatagramPacket(buffer.array(), Protocol.INPUT_PACKET_SIZE, serverAddress, serverPort)
                val pingPacket = DatagramPacket(pingBuffer.array(), 24, serverAddress, serverPort)

                val intervalNs = 8_000_000L // 8ms = 125 Hz
                var nextTickNs = System.nanoTime()
                var lastSecTimeNs = System.nanoTime()
                var lastPingTimeNs = System.nanoTime()

                while (isRunning.get() && isConnected) {
                    val s = socket ?: break
                    try {
                        val seq = sequence.incrementAndGet()
                        var ts = lastEventTimestampUs
                        if (ts == 0L) ts = System.currentTimeMillis() * 1000L

                        buffer.clear()
                        buffer.put(Protocol.MAGIC)
                        buffer.put(Protocol.PACKET_TYPE_INPUT)
                        buffer.putShort(sessionToken)
                        buffer.putInt(seq)
                        buffer.putLong(ts)
                        buffer.putShort(buttons)
                        buffer.put(triggerLeft)
                        buffer.put(triggerRight)
                        buffer.putShort(thumbLx)
                        buffer.putShort(thumbLy)
                        buffer.putShort(thumbRx)
                        buffer.putShort(thumbRy)

                        packet.setData(buffer.array(), 0, Protocol.INPUT_PACKET_SIZE)
                        s.send(packet)
                        totalPacketsSent++
                        packetsThisSec.incrementAndGet()

                        // Ping every 1 second for live RTT calculation
                        val nowNs = System.nanoTime()
                        if (nowNs - lastPingTimeNs >= 1_000_000_000L) {
                            lastPingTimeNs = nowNs
                            pingBuffer.clear()
                            pingBuffer.put(Protocol.MAGIC)
                            pingBuffer.put(Protocol.PACKET_TYPE_PING)
                            pingBuffer.putShort(sessionToken)
                            pingBuffer.putInt(seq)
                            pingBuffer.putLong(nowNs / 1000L) // Client timestamp in us
                            pingBuffer.putLong(0L)
                            pingPacket.setData(pingBuffer.array(), 0, 24)
                            s.send(pingPacket)
                        }

                        // Update live Hz rate once every second
                        if (nowNs - lastSecTimeNs >= 1_000_000_000L) {
                            currentRateHz = packetsThisSec.getAndSet(0)
                            lastSecTimeNs = nowNs
                        }

                    } catch (e: Exception) {
                        // socket error
                    }

                    // High-precision nanosecond timing
                    nextTickNs += intervalNs
                    val now = System.nanoTime()
                    val sleepNs = nextTickNs - now
                    if (sleepNs > 2_000_000L) {
                        try {
                            Thread.sleep(sleepNs / 1_000_000L)
                        } catch (ignored: InterruptedException) {
                            break
                        }
                    }
                    while (System.nanoTime() < nextTickNs) {
                        Thread.yield()
                    }
                }
            }.apply {
                isDaemon = true
                name = "UdpInputSenderThread"
                start()
            }

            // 2. Receiver Thread for Rumble & Pong
            recvThread = Thread {
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
                val recvBuf = ByteArray(64)
                val recvPkt = DatagramPacket(recvBuf, recvBuf.size)

                while (isRunning.get() && isConnected) {
                    val s = socket ?: break
                    try {
                        s.receive(recvPkt)
                        if (recvPkt.length >= 4 && recvBuf[0] == Protocol.MAGIC) {
                            val type = recvBuf[1]
                            if (type == Protocol.PACKET_TYPE_RUMBLE) {
                                val large = recvBuf[2].toInt() and 0xFF
                                val small = recvBuf[3].toInt() and 0xFF
                                rumbleListener?.invoke(large, small)
                            } else if (type == Protocol.PACKET_TYPE_PONG && recvPkt.length >= 24) {
                                val bb = ByteBuffer.wrap(recvBuf, 0, recvPkt.length).order(ByteOrder.LITTLE_ENDIAN)
                                bb.position(8) // client_timestamp offset
                                val clientTsUs = bb.getLong()
                                val nowUs = System.nanoTime() / 1000L
                                val rtt = (nowUs - clientTsUs) / 1000f
                                if (rtt in 0f..500f) {
                                    lastRttMs = rtt
                                }
                            }
                        }
                    } catch (e: Exception) {
                        break
                    }
                }
            }.apply {
                isDaemon = true
                name = "UdpReceiverThread"
                start()
            }

        } catch (e: Exception) {
            e.printStackTrace()
            isConnected = false
        }
    }

    fun disconnect() {
        isConnected = false
        isRunning.set(false)

        sendThread?.interrupt()
        recvThread?.interrupt()
        sendThread = null
        recvThread = null

        try {
            socket?.close()
        } catch (ignored: Exception) {}
        socket = null
        serverAddress = null
    }
}
