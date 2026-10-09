package com.wifigamepad.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder

data class DiscoveredPc(
    val name: String,
    val ip: String,
    val port: Int
)

class DiscoveryClient {
    suspend fun discoverPcs(timeoutMs: Int = 2000): List<DiscoveredPc> = withContext(Dispatchers.IO) {
        val discovered = mutableListOf<DiscoveredPc>()
        var socket: DatagramSocket? = null

        try {
            socket = DatagramSocket().apply {
                broadcast = true
                soTimeout = timeoutMs
            }

            val probeBuffer = ByteBuffer.allocate(4).apply {
                order(ByteOrder.LITTLE_ENDIAN)
                put(Protocol.MAGIC)
                put(Protocol.PACKET_TYPE_DISCOVERY)
                putShort(0)
            }

            val broadcastAddr = InetAddress.getByName("255.255.255.255")
            val sendPacket = DatagramPacket(probeBuffer.array(), 4, broadcastAddr, Protocol.DISCOVERY_PORT)
            socket.send(sendPacket)

            val recvBuffer = ByteArray(128)
            val recvPacket = DatagramPacket(recvBuffer, recvBuffer.size)

            val startTime = System.currentTimeMillis()
            while (System.currentTimeMillis() - startTime < timeoutMs) {
                try {
                    socket.receive(recvPacket)
                    val buf = ByteBuffer.wrap(recvPacket.data, 0, recvPacket.length).order(ByteOrder.LITTLE_ENDIAN)
                    if (buf.remaining() >= 4 && buf.get() == Protocol.MAGIC && buf.get() == Protocol.PACKET_TYPE_DISCOVERY_ACK) {
                        val port = buf.short.toInt() and 0xFFFF
                        val nameBytes = ByteArray(32)
                        buf.get(nameBytes)
                        val name = String(nameBytes).trim { it <= ' ' || it == '\u0000' }
                        val ip = recvPacket.address.hostAddress ?: ""

                        if (discovered.none { it.ip == ip }) {
                            discovered.add(DiscoveredPc(name = name.ifEmpty { "Gaming-PC" }, ip = ip, port = port))
                        }
                    }
                } catch (e: Exception) {
                    break // Timeout reached
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        } finally {
            socket?.close()
        }

        return@withContext discovered
    }
}
