package com.wifigamepad.ui

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import com.wifigamepad.network.Protocol

/**
 * Frutiger Aero Aqua Glass Xbox Controller Visualizer.
 * Faithfully replicates the iconic 2000s skeuomorphic Aero Glass aesthetic:
 * - Radiant cyan/azure translucent glass chassis
 * - Specular curved glass gloss reflection
 * - 3D crystal gem buttons (A, B, X, Y) with specular sheen
 * - 3D glowing analog stick orbs
 * - Neon cyan rim glow & live force-feedback pulse aura
 */
class GamepadVisualView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var lx: Float = 0f
    private var ly: Float = 0f
    private var rx: Float = 0f
    private var ry: Float = 0f
    private var lt: Float = 0f
    private var rt: Float = 0f
    private var buttons: Short = 0
    private var isRumbling: Boolean = false

    // Paints
    private val bodyPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val bodyBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3.5f
        color = Color.parseColor("#00E5FF")
    }
    private val bodyGlowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 8f
        color = Color.parseColor("#4000E5FF")
    }
    private val glossPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val stickWellPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.parseColor("#0A1C36")
    }
    private val stickBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f
        color = Color.parseColor("#00E5FF")
    }
    private val stickCapPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#BAE6FD")
        textSize = 22f
        textAlign = Paint.Align.CENTER
    }
    private val meterBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#0C203D")
        style = Paint.Style.FILL
    }
    private val meterBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#38BDF8")
        style = Paint.Style.STROKE
        strokeWidth = 1.5f
    }
    private val btnPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val btnStrokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }

    private val bodyRect = RectF()
    private val tempRect = RectF()
    private val glossPath = Path()

    fun updateState(
        lxVal: Float,
        lyVal: Float,
        rxVal: Float,
        ryVal: Float,
        ltVal: Float,
        rtVal: Float,
        btnMask: Short,
        rumble: Boolean = false
    ) {
        lx = lxVal.coerceIn(-1f, 1f)
        ly = lyVal.coerceIn(-1f, 1f)
        rx = rxVal.coerceIn(-1f, 1f)
        ry = ryVal.coerceIn(-1f, 1f)
        lt = ltVal.coerceIn(0f, 1f)
        rt = rtVal.coerceIn(0f, 1f)
        buttons = btnMask
        isRumbling = rumble
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0 || h <= 0) return

        val padX = 20f
        val padY = 20f
        bodyRect.set(padX, padY, w - padX, h - padY)

        // 1. Controller Chassis Background: Radiant Frutiger Aero Aqua Glass
        val bodyShader = LinearGradient(
            bodyRect.left, bodyRect.top,
            bodyRect.left, bodyRect.bottom,
            intArrayOf(
                Color.parseColor("#D90D325E"), // Deep oceanic aqua top
                Color.parseColor("#E6092040"), // Radiant blue center
                Color.parseColor("#F2041226")  // Deep navy bottom
            ),
            floatArrayOf(0f, 0.45f, 1f),
            Shader.TileMode.CLAMP
        )
        bodyPaint.shader = bodyShader

        // Outer Aura Glow (changes to purple during rumble!)
        bodyGlowPaint.color = if (isRumbling) Color.parseColor("#80A855F7") else Color.parseColor("#4000E5FF")
        bodyBorderPaint.color = if (isRumbling) Color.parseColor("#C084FC") else Color.parseColor("#00E5FF")
        canvas.drawRoundRect(bodyRect, 44f, 44f, bodyGlowPaint)
        canvas.drawRoundRect(bodyRect, 44f, 44f, bodyPaint)
        canvas.drawRoundRect(bodyRect, 44f, 44f, bodyBorderPaint)

        // 2. Frutiger Aero Specular Curved Glass Highlight (The signature sheen in the PRYDA image!)
        glossPath.reset()
        val glossHeight = bodyRect.height() * 0.40f
        glossPath.moveTo(bodyRect.left + 4f, bodyRect.top + 44f)
        glossPath.quadTo(
            bodyRect.left + 4f, bodyRect.top + 4f,
            bodyRect.left + 44f, bodyRect.top + 4f
        )
        glossPath.lineTo(bodyRect.right - 44f, bodyRect.top + 4f)
        glossPath.quadTo(
            bodyRect.right - 4f, bodyRect.top + 4f,
            bodyRect.right - 4f, bodyRect.top + 44f
        )
        glossPath.lineTo(bodyRect.right - 4f, bodyRect.top + (glossHeight * 0.7f))
        glossPath.quadTo(
            bodyRect.centerX(), bodyRect.top + glossHeight,
            bodyRect.left + 4f, bodyRect.top + (glossHeight * 0.7f)
        )
        glossPath.close()

        val glossShader = LinearGradient(
            bodyRect.left, bodyRect.top,
            bodyRect.left, bodyRect.top + glossHeight,
            intArrayOf(
                Color.parseColor("#5AFFFFFF"), // Bright white specular reflection
                Color.parseColor("#20FFFFFF"),
                Color.parseColor("#00FFFFFF")  // Fades to transparent
            ),
            floatArrayOf(0f, 0.4f, 1f),
            Shader.TileMode.CLAMP
        )
        glossPaint.shader = glossShader
        canvas.drawPath(glossPath, glossPaint)

        // 3. Center Xbox Guide Orb
        val centerX = bodyRect.centerX()
        val centerY = bodyRect.top + 64f
        val guideActive = (buttons.toInt() and Protocol.BTN_GUIDE.toInt()) != 0

        // Guide Orb 3D Gem effect
        val guideShader = RadialGradient(
            centerX - 6f, centerY - 6f, 26f,
            if (guideActive) Color.parseColor("#6EE7B7") else Color.parseColor("#38BDF8"),
            if (guideActive) Color.parseColor("#059669") else Color.parseColor("#0284C7"),
            Shader.TileMode.CLAMP
        )
        btnPaint.shader = guideShader
        canvas.drawCircle(centerX, centerY, 24f, btnPaint)
        btnPaint.shader = null

        btnStrokePaint.color = Color.parseColor("#E0F2FE")
        canvas.drawCircle(centerX, centerY, 24f, btnStrokePaint)

        // White specular dot on guide button
        val specPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#A0FFFFFF") }
        canvas.drawCircle(centerX - 7f, centerY - 7f, 5f, specPaint)

        textPaint.textSize = 20f
        textPaint.color = Color.WHITE
        canvas.drawText("X", centerX, centerY + 7f, textPaint)

        // Force Feedback / Rumble Active Glowing Pill
        if (isRumbling) {
            val rPill = RectF(centerX - 68f, centerY + 34f, centerX + 68f, centerY + 58f)
            btnPaint.color = Color.parseColor("#9333EA")
            canvas.drawRoundRect(rPill, 12f, 12f, btnPaint)
            btnStrokePaint.color = Color.parseColor("#E9D5FF")
            canvas.drawRoundRect(rPill, 12f, 12f, btnStrokePaint)
            textPaint.textSize = 13f
            textPaint.color = Color.WHITE
            canvas.drawText("⚡ RUMBLE", centerX, centerY + 51f, textPaint)
        }

        // 4. Left Analog Stick (Top Left)
        val stickRadius = (bodyRect.height() * 0.18f).coerceIn(40f, 75f)
        val lxCenter = bodyRect.left + (bodyRect.width() * 0.26f)
        val lyCenter = bodyRect.top + (bodyRect.height() * 0.38f)

        // Stick Well with deep glass rim
        canvas.drawCircle(lxCenter, lyCenter, stickRadius, stickWellPaint)
        canvas.drawCircle(lxCenter, lyCenter, stickRadius, stickBorderPaint)

        // Moving 3D Stick Orb
        val l3Pressed = (buttons.toInt() and Protocol.BTN_LEFT_THUMB.toInt()) != 0
        val capRadius = stickRadius * 0.40f
        val maxTravel = stickRadius - capRadius - 4f
        val stickPosX = lxCenter + (lx * maxTravel)
        val stickPosY = lyCenter - (ly * maxTravel)

        val stickShaderL = RadialGradient(
            stickPosX - 4f, stickPosY - 4f, capRadius,
            if (l3Pressed) Color.parseColor("#FCA5A5") else Color.parseColor("#6EE7B7"),
            if (l3Pressed) Color.parseColor("#DC2626") else Color.parseColor("#059669"),
            Shader.TileMode.CLAMP
        )
        stickCapPaint.shader = stickShaderL
        canvas.drawCircle(stickPosX, stickPosY, capRadius, stickCapPaint)
        stickCapPaint.shader = null

        // Specular dot on stick cap
        canvas.drawCircle(stickPosX - capRadius * 0.35f, stickPosY - capRadius * 0.35f, capRadius * 0.28f, specPaint)
        canvas.drawText("Left Stick (L3)", lxCenter, lyCenter + stickRadius + 24f, labelPaint)

        // 5. D-Pad (Bottom Left)
        val dpadX = bodyRect.left + (bodyRect.width() * 0.28f)
        val dpadY = bodyRect.bottom - (bodyRect.height() * 0.26f)
        val dSize = stickRadius * 0.44f

        drawFrutigerDpad(canvas, dpadX, dpadY, dSize)

        // 6. Right Analog Stick (Bottom Right)
        val rxCenter = bodyRect.right - (bodyRect.width() * 0.32f)
        val ryCenter = bodyRect.bottom - (bodyRect.height() * 0.26f)

        canvas.drawCircle(rxCenter, ryCenter, stickRadius, stickWellPaint)
        canvas.drawCircle(rxCenter, ryCenter, stickRadius, stickBorderPaint)

        val r3Pressed = (buttons.toInt() and Protocol.BTN_RIGHT_THUMB.toInt()) != 0
        val rxPosX = rxCenter + (rx * maxTravel)
        val rxPosY = ryCenter - (ry * maxTravel)

        val stickShaderR = RadialGradient(
            rxPosX - 4f, rxPosY - 4f, capRadius,
            if (r3Pressed) Color.parseColor("#FCA5A5") else Color.parseColor("#7DD3FC"),
            if (r3Pressed) Color.parseColor("#DC2626") else Color.parseColor("#0284C7"),
            Shader.TileMode.CLAMP
        )
        stickCapPaint.shader = stickShaderR
        canvas.drawCircle(rxPosX, rxPosY, capRadius, stickCapPaint)
        stickCapPaint.shader = null

        canvas.drawCircle(rxPosX - capRadius * 0.35f, rxPosY - capRadius * 0.35f, capRadius * 0.28f, specPaint)
        canvas.drawText("Right Stick (R3)", rxCenter, ryCenter + stickRadius + 24f, labelPaint)

        // 7. Action Buttons A, B, X, Y (Top Right)
        val btnCenterX = bodyRect.right - (bodyRect.width() * 0.24f)
        val btnCenterY = bodyRect.top + (bodyRect.height() * 0.38f)
        val btnDist = stickRadius * 0.82f
        val btnRadius = stickRadius * 0.34f

        val aPressed = (buttons.toInt() and Protocol.BTN_A.toInt()) != 0
        val bPressed = (buttons.toInt() and Protocol.BTN_B.toInt()) != 0
        val xPressed = (buttons.toInt() and Protocol.BTN_X.toInt()) != 0
        val yPressed = (buttons.toInt() and Protocol.BTN_Y.toInt()) != 0

        drawFrutigerGem(canvas, btnCenterX, btnCenterY + btnDist, "A", aPressed, Color.parseColor("#10B981"), Color.parseColor("#6EE7B7"), btnRadius)
        drawFrutigerGem(canvas, btnCenterX + btnDist, btnCenterY, "B", bPressed, Color.parseColor("#EF4444"), Color.parseColor("#FCA5A5"), btnRadius)
        drawFrutigerGem(canvas, btnCenterX - btnDist, btnCenterY, "X", xPressed, Color.parseColor("#06B6D4"), Color.parseColor("#67E8F9"), btnRadius)
        drawFrutigerGem(canvas, btnCenterX, btnCenterY - btnDist, "Y", yPressed, Color.parseColor("#F59E0B"), Color.parseColor("#FDE68A"), btnRadius)

        // 8. Bumpers (LB / RB) at Top
        val lbPressed = (buttons.toInt() and Protocol.BTN_LEFT_SHOULDER.toInt()) != 0
        val rbPressed = (buttons.toInt() and Protocol.BTN_RIGHT_SHOULDER.toInt()) != 0
        val bumperW = 92f
        val bumperH = 34f

        drawFrutigerBumper(canvas, bodyRect.left + (bodyRect.width() * 0.18f), bodyRect.top + 16f, bumperW, bumperH, "LB", lbPressed)
        drawFrutigerBumper(canvas, bodyRect.right - (bodyRect.width() * 0.18f) - bumperW, bodyRect.top + 16f, bumperW, bumperH, "RB", rbPressed)

        // 9. Triggers (LT / RT)
        val triggerBarW = 86f
        val triggerBarH = 16f

        drawFrutigerTrigger(canvas, bodyRect.left + (bodyRect.width() * 0.35f), bodyRect.top + 24f, triggerBarW, triggerBarH, "LT", lt)
        drawFrutigerTrigger(canvas, bodyRect.right - (bodyRect.width() * 0.35f) - triggerBarW, bodyRect.top + 24f, triggerBarW, triggerBarH, "RT", rt)
    }

    private fun drawFrutigerGem(
        canvas: Canvas,
        x: Float,
        y: Float,
        label: String,
        pressed: Boolean,
        baseColor: Int,
        lightColor: Int,
        radius: Float
    ) {
        val gemShader = RadialGradient(
            x - radius * 0.3f, y - radius * 0.3f, radius * 1.2f,
            if (pressed) Color.WHITE else lightColor,
            if (pressed) lightColor else baseColor,
            Shader.TileMode.CLAMP
        )
        btnPaint.shader = gemShader
        canvas.drawCircle(x, y, radius, btnPaint)
        btnPaint.shader = null

        btnStrokePaint.color = if (pressed) Color.WHITE else Color.parseColor("#00E5FF")
        canvas.drawCircle(x, y, radius, btnStrokePaint)

        // Top specular highlight
        val specPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#B0FFFFFF") }
        canvas.drawCircle(x - radius * 0.32f, y - radius * 0.32f, radius * 0.28f, specPaint)

        textPaint.textSize = radius * 1.15f
        textPaint.color = if (pressed) Color.BLACK else Color.WHITE
        canvas.drawText(label, x, y + (radius * 0.40f), textPaint)
    }

    private fun drawFrutigerDpad(canvas: Canvas, cx: Float, cy: Float, size: Float) {
        val up = (buttons.toInt() and Protocol.BTN_DPAD_UP.toInt()) != 0
        val down = (buttons.toInt() and Protocol.BTN_DPAD_DOWN.toInt()) != 0
        val left = (buttons.toInt() and Protocol.BTN_DPAD_LEFT.toInt()) != 0
        val right = (buttons.toInt() and Protocol.BTN_DPAD_RIGHT.toInt()) != 0

        val activeColor = Color.parseColor("#00F5FF")
        val inactiveColor = Color.parseColor("#0E2648")

        // Draw cross segments
        autoDrawDpadSegment(canvas, cx - size / 2, cy - size * 1.8f, cx + size / 2, cy - size * 0.6f, up, activeColor, inactiveColor)
        autoDrawDpadSegment(canvas, cx - size / 2, cy + size * 0.6f, cx + size / 2, cy + size * 1.8f, down, activeColor, inactiveColor)
        autoDrawDpadSegment(canvas, cx - size * 1.8f, cy - size / 2, cx - size * 0.6f, cy + size / 2, left, activeColor, inactiveColor)
        autoDrawDpadSegment(canvas, cx + size * 0.6f, cy - size / 2, cx + size * 1.8f, cy + size / 2, right, activeColor, inactiveColor)

        tempRect.set(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2)
        btnPaint.color = inactiveColor
        canvas.drawRect(tempRect, btnPaint)
    }

    private fun autoDrawDpadSegment(
        canvas: Canvas,
        l: Float, t: Float, r: Float, b: Float,
        active: Boolean,
        actColor: Int,
        inactColor: Int
    ) {
        tempRect.set(l, t, r, b)
        btnPaint.color = if (active) actColor else inactColor
        canvas.drawRoundRect(tempRect, 6f, 6f, btnPaint)
        btnStrokePaint.color = if (active) Color.WHITE else Color.parseColor("#00E5FF")
        canvas.drawRoundRect(tempRect, 6f, 6f, btnStrokePaint)
    }

    private fun drawFrutigerBumper(
        canvas: Canvas,
        x: Float,
        y: Float,
        w: Float,
        h: Float,
        label: String,
        pressed: Boolean
    ) {
        tempRect.set(x, y, x + w, y + h)
        val bumperShader = LinearGradient(
            x, y, x, y + h,
            if (pressed) Color.parseColor("#38BDF8") else Color.parseColor("#0E2A50"),
            if (pressed) Color.parseColor("#0284C7") else Color.parseColor("#06172E"),
            Shader.TileMode.CLAMP
        )
        btnPaint.shader = bumperShader
        canvas.drawRoundRect(tempRect, 10f, 10f, btnPaint)
        btnPaint.shader = null

        btnStrokePaint.color = if (pressed) Color.WHITE else Color.parseColor("#00E5FF")
        canvas.drawRoundRect(tempRect, 10f, 10f, btnStrokePaint)

        // Gloss highlight line
        val bumpGloss = RectF(x + 2f, y + 2f, x + w - 2f, y + (h * 0.4f))
        val specPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#40FFFFFF") }
        canvas.drawRoundRect(bumpGloss, 8f, 8f, specPaint)

        textPaint.textSize = 19f
        textPaint.color = if (pressed) Color.BLACK else Color.parseColor("#E0F2FE")
        canvas.drawText(label, tempRect.centerX(), tempRect.centerY() + 7f, textPaint)
    }

    private fun drawFrutigerTrigger(
        canvas: Canvas,
        x: Float,
        y: Float,
        w: Float,
        h: Float,
        label: String,
        amount: Float
    ) {
        textPaint.textSize = 17f
        textPaint.color = Color.parseColor("#00E5FF")
        canvas.drawText(label, x - 20f, y + h - 1f, textPaint)

        tempRect.set(x, y, x + w, y + h)
        canvas.drawRoundRect(tempRect, 6f, 6f, meterBgPaint)
        canvas.drawRoundRect(tempRect, 6f, 6f, meterBorderPaint)

        if (amount > 0.01f) {
            val fillW = w * amount
            val fRect = RectF(x, y, x + fillW, y + h)
            val fillShader = LinearGradient(
                x, y, x, y + h,
                Color.parseColor("#22D3EE"),
                Color.parseColor("#0284C7"),
                Shader.TileMode.CLAMP
            )
            btnPaint.shader = fillShader
            canvas.drawRoundRect(fRect, 6f, 6f, btnPaint)
            btnPaint.shader = null
        }
    }
}
