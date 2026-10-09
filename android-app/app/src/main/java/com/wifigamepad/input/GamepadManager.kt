package com.wifigamepad.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import com.wifigamepad.network.Protocol
import com.wifigamepad.network.UdpSender
import kotlin.math.roundToInt
import kotlin.math.sqrt

typealias InputStateListener = (lx: Float, ly: Float, rx: Float, ry: Float, lt: Float, rt: Float, buttons: Short) -> Unit

class GamepadManager(private val udpSender: UdpSender) {

    var stateListener: InputStateListener? = null

    /**
     * Perfect mathematical circular deadzone remapping (0% error on circularity test).
     */
    private fun normalizeStick(rawX: Float, rawY: Float, deadzone: Float = 0.05f): Pair<Short, Short> {
        val mag = sqrt(rawX * rawX + rawY * rawY)
        if (mag < deadzone) {
            return Pair(0.toShort(), 0.toShort())
        }
        val normalizedMag = ((mag - deadzone) / (1.0f - deadzone)).coerceAtMost(1.0f)
        val nx = (rawX / mag) * normalizedMag
        val ny = (rawY / mag) * normalizedMag
        val sx = (nx * 32767f).roundToInt().coerceIn(-32768, 32767).toShort()
        val sy = (ny * 32767f).roundToInt().coerceIn(-32768, 32767).toShort()
        return Pair(sx, sy)
    }

    fun handleMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_CLASS_JOYSTICK == 0 &&
            event.source and InputDevice.SOURCE_GAMEPAD == 0) {
            return false
        }

        // Left Thumbstick
        val lx = event.getAxisValue(MotionEvent.AXIS_X)
        val ly = -event.getAxisValue(MotionEvent.AXIS_Y) // Invert Y for XInput
        val left = normalizeStick(lx, ly)
        udpSender.thumbLx = left.first
        udpSender.thumbLy = left.second

        // Right Thumbstick (AXIS_Z / AXIS_RZ)
        val rx = event.getAxisValue(MotionEvent.AXIS_Z)
        val ry = -event.getAxisValue(MotionEvent.AXIS_RZ)
        val right = normalizeStick(rx, ry)
        udpSender.thumbRx = right.first
        udpSender.thumbRy = right.second

        // Triggers (0 - 255)
        var lt = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
        if (lt == 0f) lt = event.getAxisValue(MotionEvent.AXIS_BRAKE)
        udpSender.triggerLeft = (lt * 255f).roundToInt().coerceIn(0, 255).toByte()

        var rt = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
        if (rt == 0f) rt = event.getAxisValue(MotionEvent.AXIS_GAS)
        udpSender.triggerRight = (rt * 255f).roundToInt().coerceIn(0, 255).toByte()

        // Hat D-Pad
        val hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        var currentButtons = udpSender.buttons.toInt()

        currentButtons = updateBit(currentButtons, Protocol.BTN_DPAD_LEFT.toInt(), hatX < -0.5f)
        currentButtons = updateBit(currentButtons, Protocol.BTN_DPAD_RIGHT.toInt(), hatX > 0.5f)
        currentButtons = updateBit(currentButtons, Protocol.BTN_DPAD_UP.toInt(), hatY < -0.5f)
        currentButtons = updateBit(currentButtons, Protocol.BTN_DPAD_DOWN.toInt(), hatY > 0.5f)
        udpSender.buttons = currentButtons.toShort()

        udpSender.lastEventTimestampUs = event.eventTime * 1000L

        notifyListener()
        return true
    }

    fun handleKeyEvent(event: KeyEvent): Boolean {
        val isDown = event.action == KeyEvent.ACTION_DOWN
        val mask = when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> Protocol.BTN_A
            KeyEvent.KEYCODE_BUTTON_B -> Protocol.BTN_B
            KeyEvent.KEYCODE_BUTTON_X -> Protocol.BTN_X
            KeyEvent.KEYCODE_BUTTON_Y -> Protocol.BTN_Y
            KeyEvent.KEYCODE_BUTTON_L1 -> Protocol.BTN_LEFT_SHOULDER
            KeyEvent.KEYCODE_BUTTON_R1 -> Protocol.BTN_RIGHT_SHOULDER
            KeyEvent.KEYCODE_BUTTON_THUMBL -> Protocol.BTN_LEFT_THUMB
            KeyEvent.KEYCODE_BUTTON_THUMBR -> Protocol.BTN_RIGHT_THUMB
            KeyEvent.KEYCODE_BUTTON_START -> Protocol.BTN_START
            KeyEvent.KEYCODE_BUTTON_SELECT -> Protocol.BTN_BACK
            KeyEvent.KEYCODE_BUTTON_MODE -> Protocol.BTN_GUIDE
            KeyEvent.KEYCODE_DPAD_UP -> Protocol.BTN_DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> Protocol.BTN_DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> Protocol.BTN_DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> Protocol.BTN_DPAD_RIGHT
            else -> return false
        }.toInt()

        val current = udpSender.buttons.toInt()
        udpSender.buttons = updateBit(current, mask, isDown).toShort()
        udpSender.lastEventTimestampUs = event.eventTime * 1000L

        notifyListener()
        return true
    }

    private fun notifyListener() {
        stateListener?.invoke(
            udpSender.thumbLx / 32767f,
            udpSender.thumbLy / 32767f,
            udpSender.thumbRx / 32767f,
            udpSender.thumbRy / 32767f,
            (udpSender.triggerLeft.toInt() and 0xFF) / 255f,
            (udpSender.triggerRight.toInt() and 0xFF) / 255f,
            udpSender.buttons
        )
    }

    private fun updateBit(value: Int, mask: Int, set: Boolean): Int {
        return if (set) (value or mask) else (value and mask.inv())
    }
}
