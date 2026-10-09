package com.wifigamepad

import android.content.Context
import android.hardware.input.InputManager
import android.net.wifi.WifiManager
import android.os.Build
import android.os.Bundle
import android.os.CombinedVibration
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.wifigamepad.input.GamepadManager
import com.wifigamepad.network.DiscoveryClient
import com.wifigamepad.network.UdpSender
import com.wifigamepad.ui.GamepadVisualView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity(), InputManager.InputDeviceListener {

    private lateinit var etIpAddress: EditText
    private lateinit var btnScan: Button
    private lateinit var btnConnect: Button
    private lateinit var btnTestRumble: Button
    private lateinit var tvDiscoveredList: TextView
    private lateinit var tvGamepadStatus: TextView
    private lateinit var tvConnectionStatus: TextView
    private lateinit var tvMetrics: TextView
    private lateinit var gamepadVisualView: GamepadVisualView

    private val udpSender = UdpSender()
    private val gamepadManager = GamepadManager(udpSender)
    private val discoveryClient = DiscoveryClient()
    private lateinit var inputManager: InputManager

    private var wifiLock: WifiManager.WifiLock? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var activeGamepadDevice: InputDevice? = null
    private var isCurrentlyRumbling = false
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // 1. Keep screen on
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // 2. Request unbuffered input dispatch for ultra-low latency
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            window.decorView.requestUnbufferedDispatch(
                InputDevice.SOURCE_JOYSTICK or InputDevice.SOURCE_GAMEPAD
            )
        }

        // 3. Acquire Low-Latency Wi-Fi & Wake Locks
        acquireLowLatencyLocks()

        etIpAddress = findViewById(R.id.etIpAddress)
        btnScan = findViewById(R.id.btnScan)
        btnConnect = findViewById(R.id.btnConnect)
        btnTestRumble = findViewById(R.id.btnTestRumble)
        tvDiscoveredList = findViewById(R.id.tvDiscoveredList)
        tvGamepadStatus = findViewById(R.id.tvGamepadStatus)
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus)
        tvMetrics = findViewById(R.id.tvMetrics)
        gamepadVisualView = findViewById(R.id.gamepadVisualView)

        inputManager = getSystemService(Context.INPUT_SERVICE) as InputManager
        inputManager.registerInputDeviceListener(this, null)

        // Forward input changes directly to GamepadVisualView
        gamepadManager.stateListener = { lx, ly, rx, ry, lt, rt, buttons ->
            gamepadVisualView.updateState(lx, ly, rx, ry, lt, rt, buttons, isCurrentlyRumbling)
        }

        checkConnectedGamepads()
        setupRumbleFeedback()
        startTelemetryLoop()

        btnScan.setOnClickListener {
            scanForPcs()
        }

        btnConnect.setOnClickListener {
            toggleConnection()
        }

        btnTestRumble.setOnClickListener {
            triggerVibrationTest()
        }
    }

    private fun triggerVibrationTest() {
        // High-power 1.5s vibration test on both motors at maximum 255 force
        triggerVibration(largeMotor = 255, smallMotor = 255, durationMs = 1500)
    }

    /**
     * Drives dual-motor vibration across all connected Bluetooth gamepads
     * and the host device's internal vibration motor with zero clipping.
     */
    private fun triggerVibration(largeMotor: Int, smallMotor: Int, durationMs: Int) {
        val maxStrength = maxOf(largeMotor, smallMotor)
        val shouldVibrate = maxStrength > 0

        isCurrentlyRumbling = shouldVibrate
        mainHandler.removeCallbacksAndMessages(null)
        if (shouldVibrate) {
            mainHandler.postDelayed({
                isCurrentlyRumbling = false
            }, durationMs.toLong())
        }

        // 1. Iterate ALL connected gamepads and send hardware rumble
        val deviceIds = inputManager.inputDeviceIds
        for (id in deviceIds) {
            val dev = inputManager.getInputDevice(id) ?: continue
            val sources = dev.sources
            if ((sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD) ||
                (sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)) {

                // Android 12+ (API 31+) Dual-Motor VibratorManager
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    try {
                        val vm = dev.vibratorManager
                        if (!shouldVibrate) {
                            vm.cancel()
                        } else {
                            val vibIds = vm.vibratorIds
                            if (vibIds.size >= 2) {
                                val effL = VibrationEffect.createOneShot(durationMs.toLong(), largeMotor.coerceIn(1, 255))
                                val effS = VibrationEffect.createOneShot(durationMs.toLong(), smallMotor.coerceIn(1, 255))
                                val comb = CombinedVibration.startParallel()
                                    .addVibrator(vibIds[0], effL)
                                    .addVibrator(vibIds[1], effS)
                                    .combine()
                                vm.vibrate(comb)
                            } else if (vibIds.isNotEmpty()) {
                                val eff = VibrationEffect.createOneShot(durationMs.toLong(), maxStrength.coerceIn(1, 255))
                                val comb = CombinedVibration.startParallel()
                                    .addVibrator(vibIds[0], eff)
                                    .combine()
                                vm.vibrate(comb)
                            } else {
                                val defVib = vm.defaultVibrator
                                if (defVib.hasVibrator()) {
                                    defVib.vibrate(VibrationEffect.createOneShot(durationMs.toLong(), maxStrength.coerceIn(1, 255)))
                                }
                            }
                        }
                    } catch (ignored: Exception) {}
                }

                // Legacy InputDevice.vibrator fallback
                try {
                    val vib = dev.vibrator
                    if (vib.hasVibrator()) {
                        if (!shouldVibrate) {
                            vib.cancel()
                        } else {
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                                vib.vibrate(VibrationEffect.createOneShot(durationMs.toLong(), maxStrength.coerceIn(1, 255)))
                            } else {
                                @Suppress("DEPRECATION")
                                vib.vibrate(durationMs.toLong())
                            }
                        }
                    }
                } catch (ignored: Exception) {}
            }
        }

        // 2. Also vibrate host phone / tablet vibrator with full force
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vm = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                if (!shouldVibrate) {
                    vm?.cancel()
                } else {
                    vm?.defaultVibrator?.vibrate(VibrationEffect.createOneShot(durationMs.toLong(), maxStrength.coerceIn(1, 255)))
                }
            } else {
                @Suppress("DEPRECATION")
                val phoneVibrator = getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
                if (phoneVibrator != null && phoneVibrator.hasVibrator()) {
                    if (!shouldVibrate) {
                        phoneVibrator.cancel()
                    } else {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            phoneVibrator.vibrate(VibrationEffect.createOneShot(durationMs.toLong(), maxStrength.coerceIn(1, 255)))
                        } else {
                            @Suppress("DEPRECATION")
                            phoneVibrator.vibrate(durationMs.toLong())
                        }
                    }
                }
            }
        } catch (ignored: Exception) {}
    }

    private fun acquireLowLatencyLocks() {
        try {
            val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
            wifiLock = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                wifiManager.createWifiLock(WifiManager.WIFI_MODE_FULL_LOW_LATENCY, "WiFiGamepad:LowLatency")
            } else {
                @Suppress("DEPRECATION")
                wifiManager.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "WiFiGamepad:HighPerf")
            }
            wifiLock?.acquire()

            val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
            wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "WiFiGamepad:WakeLock")
            wakeLock?.acquire(24 * 60 * 60 * 1000L)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun setupRumbleFeedback() {
        udpSender.rumbleListener = { largeMotor, smallMotor ->
            if (largeMotor > 0 || smallMotor > 0) {
                // Sustain continuous vibration up to 600ms per packet
                triggerVibration(largeMotor, smallMotor, durationMs = 600)
            } else {
                // Cut off immediately when game motor stops
                triggerVibration(0, 0, durationMs = 0)
            }
        }
    }

    private fun startTelemetryLoop() {
        lifecycleScope.launch {
            while (isActive) {
                delay(500)
                if (udpSender.isConnected) {
                    val hz = udpSender.currentRateHz
                    val rtt = udpSender.lastRttMs
                    val total = udpSender.totalPacketsSent
                    val rttStr = if (rtt > 0f) String.format("%.1f ms", rtt) else "Measuring..."
                    tvMetrics.text = "Polling Rate: $hz Hz (Paced)\nLatency (RTT): $rttStr\nTotal Packets: $total\nWi-Fi Mode: Low Latency"
                }
            }
        }
    }

    private fun checkConnectedGamepads() {
        val deviceIds = inputManager.inputDeviceIds
        activeGamepadDevice = null

        for (id in deviceIds) {
            val device = inputManager.getInputDevice(id) ?: continue
            val sources = device.sources
            if ((sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD) ||
                (sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)) {
                activeGamepadDevice = device
                break
            }
        }

        if (activeGamepadDevice != null) {
            var hasVib = false
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                hasVib = activeGamepadDevice?.vibratorManager?.vibratorIds?.isNotEmpty() == true
            }
            if (!hasVib) {
                hasVib = activeGamepadDevice?.vibrator?.hasVibrator() == true
            }

            tvGamepadStatus.text = "Gamepad: 🟢 ${activeGamepadDevice?.name}\nMotors: ${if (hasVib) "Gamepad Dual-Motor Active" else "Device Haptics Ready"}"
        } else {
            tvGamepadStatus.text = "Gamepad: ⚪ Waiting (Connect BT Gamepad)"
        }
    }

    private fun scanForPcs() {
        tvDiscoveredList.text = "Scanning network (Port 52151)..."
        btnScan.isEnabled = false

        lifecycleScope.launch {
            val pcs = discoveryClient.discoverPcs()
            btnScan.isEnabled = true
            if (pcs.isNotEmpty()) {
                val first = pcs.first()
                etIpAddress.setText(first.ip)
                tvDiscoveredList.text = "Found PC: ${first.name} (${first.ip})"
            } else {
                tvDiscoveredList.text = "No PC found. Enter IP manually."
            }
        }
    }

    private fun toggleConnection() {
        if (udpSender.isConnected) {
            udpSender.disconnect()
            btnConnect.text = "Connect"
            tvConnectionStatus.text = "Connection: Disconnected"
        } else {
            val ip = etIpAddress.text.toString().trim()
            if (ip.isEmpty()) {
                etIpAddress.error = "Please enter PC IP address"
                return
            }

            lifecycleScope.launch(Dispatchers.IO) {
                udpSender.connect(ip)
                withContext(Dispatchers.Main) {
                    if (udpSender.isConnected) {
                        btnConnect.text = "Disconnect"
                        tvConnectionStatus.text = "🟢 Connected ($ip)\nRate: 125 Hz Active"
                    } else {
                        tvConnectionStatus.text = "🔴 Connection Error"
                    }
                }
            }
        }
    }

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        if (gamepadManager.handleMotionEvent(ev)) {
            return true
        }
        return super.dispatchGenericMotionEvent(ev)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (gamepadManager.handleKeyEvent(event)) {
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onInputDeviceAdded(deviceId: Int) = checkConnectedGamepads()
    override fun onInputDeviceRemoved(deviceId: Int) = checkConnectedGamepads()
    override fun onInputDeviceChanged(deviceId: Int) = checkConnectedGamepads()

    override fun onDestroy() {
        super.onDestroy()
        inputManager.unregisterInputDeviceListener(this)
        udpSender.disconnect()
        try {
            if (wifiLock?.isHeld == true) wifiLock?.release()
            if (wakeLock?.isHeld == true) wakeLock?.release()
        } catch (ignored: Exception) {}
    }
}
