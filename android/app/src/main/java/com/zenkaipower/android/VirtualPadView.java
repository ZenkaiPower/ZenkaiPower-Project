package com.zenkaipower.android;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Rect;
import android.util.SparseIntArray;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import org.libsdl.app.TouchPadBridge;
import java.io.File;
import java.util.ArrayList;

/** Multitouch gamepad; pointer IDs keep movement independent of action buttons. */
final class VirtualPadView extends View {
    private static final class Button {
        final String label;
        final int key, axis;
        final Bitmap image;
        final Bitmap pressedImage;
        final Bitmap directionImage;
        final Bitmap directionPressedImage;
        final Rect directionCrop;
        final Rect directionPressedCrop;
        final int directionKey;
        final Rect imageCrop, pressedCrop;
        final float artSize;
        final RectF bounds = new RectF();
        boolean held;
        boolean wanted;
        long pressedAt;
        long animationUntil;
        Runnable release;
        Button(File art, String label, int key, int axis) {
            this(art, label, key, axis, null, 0);
        }
        Button(File art, String label, int key, int axis, String direction, int directionKey) {
            this.directionKey = directionKey;
            directionImage = direction == null ? null : loadArt(art, direction + ".png");
            directionPressedImage = direction == null ? null : loadArt(art, direction + "-pressed.png");
            directionCrop = crop(directionImage);
            directionPressedCrop = crop(directionPressedImage);
            this.label = direction == null ? label : label + " + " + direction;
            this.key = key;
            this.axis = axis;
            image = loadArt(art, label + ".png");
            pressedImage = loadArt(art, label + "-pressed.png");
            imageCrop = crop(image);
            pressedCrop = crop(pressedImage);
            artSize = Math.max(1, Math.max(Math.max(imageCrop.width(), imageCrop.height()),
                    Math.max(pressedCrop.width(), pressedCrop.height())));
        }
        private static Bitmap loadArt(File art, String name) {
            try {
                byte[] bytes = GamePak.read(GameIO.file(art.getPath() + ".pak"), name);
                return BitmapFactory.decodeByteArray(bytes, 0, bytes.length);
            } catch (java.io.IOException e) { return null; }
        }
        private static Rect crop(Bitmap image) {
            if (image == null) return new Rect();
            int left = image.getWidth(), top = image.getHeight(), right = 0, bottom = 0;
            for (int y = 0; y < image.getHeight(); y++) for (int x = 0; x < image.getWidth(); x++) {
                if ((image.getPixel(x, y) >>> 24) > 12) {
                    left = Math.min(left, x); top = Math.min(top, y);
                    right = Math.max(right, x + 1); bottom = Math.max(bottom, y + 1);
                }
            }
            return right > left && bottom > top ? new Rect(left, top, right, bottom) :
                    new Rect(0, 0, image.getWidth(), image.getHeight());
        }
    }

    private final TouchMouseSurface surface;
    private final DeviceVibration vibration;
    private boolean vibrationResumed;
    private final ArrayList<Button> buttons = new ArrayList<>();
    private final SparseIntArray pointers = new SparseIntArray();
    private final SparseIntArray outputKeys = new SparseIntArray();
    private final SparseIntArray wantedKeys = new SparseIntArray();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final RectF artBounds = new RectF();
    private final RectF timerBounds = new RectF();
    private boolean padMode, gesture, suppressUntilUp;
    private boolean battleActive;
    private int timerId=-1;
    private float timerStartX,timerStartY;
    private int stickId = -1;
    private float stickX, stickY, stickDX, stickDY, radius, unit;

    VirtualPadView(Context context, TouchMouseSurface surface, File gameRoot) {
        super(context);
        this.surface = surface;
        vibration = new DeviceVibration(context);
        setContentDescription("Battle touch gamepad");
        File art = GameIO.file(gameRoot, "gamedata/touch-buttons");
        add(art, "triangle", KeyEvent.KEYCODE_BUTTON_Y, -1);
        add(art, "circle", KeyEvent.KEYCODE_BUTTON_B, -1);
        add(art, "cross", KeyEvent.KEYCODE_BUTTON_A, -1);
        add(art, "square", KeyEvent.KEYCODE_BUTTON_X, -1);
        add(art, "l1", KeyEvent.KEYCODE_BUTTON_L1, -1);
        add(art, "l2", 0, 4);
        add(art, "r1", KeyEvent.KEYCODE_BUTTON_R1, -1);
        add(art, "r2", 0, 5);
        add(art, "l3", KeyEvent.KEYCODE_BUTTON_THUMBL, -1);
        add(art, "r3", KeyEvent.KEYCODE_BUTTON_THUMBR, -1);
        add(art, "select", KeyEvent.KEYCODE_BUTTON_SELECT, -1);
        add(art, "start", KeyEvent.KEYCODE_BUTTON_START, -1);
        buttons.add(new Button(art, "l2", 0, 4, "up", KeyEvent.KEYCODE_DPAD_UP));
        buttons.add(new Button(art, "l2", 0, 4, "down", KeyEvent.KEYCODE_DPAD_DOWN));
        buttons.add(new Button(art, "triangle", KeyEvent.KEYCODE_BUTTON_Y, -1,
                "circle", KeyEvent.KEYCODE_BUTTON_B));
    }

    private final Runnable pollBattle = new Runnable() {
        @Override public void run() {
            battleActive=hasWindowFocus() && GameActivity.nativeBattleActive();
            boolean active = hasWindowFocus() && GameActivity.nativeTouchControlsActive();
            vibration.update(active && vibrationResumed ? GameActivity.nativeTouchVibration() : 0);
            if (active != padMode) {
                suppressUntilUp |= gesture;
                release();
                surface.cancelTouch();
                padMode = active;
                invalidate();
            }
            if (active) {
                TouchPadBridge.attach();
                // Keep the trigger axes at their released endpoint after attachment.
                updatePadButtons();
            }
            postDelayed(this, 32);
        }
    };

    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        post(pollBattle);
    }

    @Override protected void onDetachedFromWindow() {
        removeCallbacks(pollBattle);
        release();
        super.onDetachedFromWindow();
    }

    private void add(File art, String name, int key, int axis) {
        buttons.add(new Button(art, name, key, axis));
    }

    private void place(int index, float x, float y, float size) {
        buttons.get(index).bounds.set(x - size / 2, y - size / 2, x + size / 2, y + size / 2);
    }

    @Override protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        release();
        unit = Math.min(h / 9f, w / 20f);
        timerBounds.set(w*.5f-unit*.9f,0,w*.5f+unit*.9f,unit*.9f);
        radius = unit * 1.1f;
        float x = w - 2.6f * unit, y = h - 2.8f * unit, gap = unit * 1.12f;
        place(0, x, y - gap, unit);
        place(1, x + gap, y, unit);
        place(2, x, y + gap, unit);
        place(3, x - gap, y, unit);
        place(4, 1.8f * unit, h - 7.3f * unit, unit);
        place(5, 1.8f * unit, h - 6.1f * unit, unit);
        place(6, w - 1.8f * unit, h - 7.3f * unit, unit);
        place(7, w - 1.8f * unit, h - 6.1f * unit, unit);
        place(8, 1.1f * unit, h - unit, unit * .85f);
        place(9, w - 1.1f * unit, h - unit, unit * .85f);
        place(10, w * .5f - unit, h - unit * .65f, unit * .8f);
        place(11, w * .5f + unit, h - unit * .65f, unit * .8f);
        place(12, 3.05f * unit, h - 6.1f * unit, unit);
        place(13, 4.3f * unit, h - 6.1f * unit, unit);
        place(14, x + gap * .58f + unit * .25f, y - gap * .58f - unit * .25f, unit * .75f);
        stickX = unit * 2.5f;
        stickY = h - unit * 2.5f;
    }

    private void held(Button button, boolean down) {
        if (button.held == down) return;
        button.held = down;
        updatePadButtons();
    }

    private void sendKey(int key, boolean down) {
        if (key == 0 || outputKeys.get(key, 0) == (down ? 1 : 0)) return;
        outputKeys.put(key, down ? 1 : 0);
        TouchPadBridge.button(key, down);
    }

    private void updatePadButtons() {
        boolean l2 = false, r2 = false;
        wantedKeys.clear();
        for (Button button : buttons) {
            if (button.axis == 4) l2 |= button.held;
            if (button.axis == 5) r2 |= button.held;
            if (button.held) {
                wantedKeys.put(button.key, 1);
                wantedKeys.put(button.directionKey, 1);
            }
        }
        // A combo and a separate face button can hold the same key together.
        for (Button button : buttons) {
            sendKey(button.key, wantedKeys.get(button.key, 0) != 0);
            sendKey(button.directionKey, wantedKeys.get(button.directionKey, 0) != 0);
        }
        // Shared triggers stay down until all buttons that use them are released.
        TouchPadBridge.axis(4, l2 ? 1 : -1);
        TouchPadBridge.axis(5, r2 ? 1 : -1);
    }

    private void request(Button button, boolean down) {
        if (button.wanted == down) return;
        button.wanted = down;
        if (button.release != null) {
            removeCallbacks(button.release);
            button.release = null;
        }
        if (down) {
            if (!button.held) button.pressedAt = SystemClock.uptimeMillis();
            button.animationUntil = SystemClock.uptimeMillis() + 120;
            held(button, true);
        } else {
            // The game samples pad state each frame; preserve very short taps.
            long delay = Math.max(0, 40 - (SystemClock.uptimeMillis() - button.pressedAt));
            button.release = () -> {
                button.release = null;
                held(button, false);
                invalidate();
            };
            postDelayed(button.release, delay);
        }
    }

    private void syncButtons() {
        for (int b = 0; b < buttons.size(); b++) {
            boolean down = false;
            for (int p = 0; p < pointers.size(); p++) down |= pointers.valueAt(p) == b;
            request(buttons.get(b), down);
        }
    }

    private int hit(float x, float y) {
        for (int b = buttons.size() - 1; b >= 0; b--) if (buttons.get(b).bounds.contains(x, y)) return b;
        return -1;
    }

    private void stick(float x, float y) {
        float dx = (x - stickX) / radius, dy = (y - stickY) / radius;
        float length = (float)Math.sqrt(dx * dx + dy * dy);
        if (length > 1) { dx /= length; dy /= length; }
        stickDX = dx; stickDY = dy;
        float magnitude = Math.min(1, length);
        float scaled = magnitude <= .12f ? 0 : (magnitude - .12f) / .88f;
        float factor = magnitude > 0 ? scaled / magnitude : 0;
        TouchPadBridge.axis(0, dx * factor);
        TouchPadBridge.axis(1, dy * factor);
    }

    void release() {
        vibration.update(0);
        pointers.clear();
        for (Button button : buttons) {
            if (button.release != null) removeCallbacks(button.release);
            button.release = null;
            button.wanted = false;
            button.animationUntil = 0;
            held(button, false);
        }
        if (stickId >= 0) { TouchPadBridge.axis(0, 0); TouchPadBridge.axis(1, 0); }
        stickId = -1;
        timerId = -1;
        stickDX = stickDY = 0;
        invalidate();
    }

    void setVibrationResumed(boolean resumed) {
        vibrationResumed = resumed;
        if (!resumed) vibration.update(0);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked(), index = event.getActionIndex();
        int id = event.getPointerId(index);
        float x = event.getX(index), y = event.getY(index);
        if (action == MotionEvent.ACTION_DOWN) gesture = true;
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) gesture = false;
        if (suppressUntilUp) {
            if (!gesture) { suppressUntilUp = false; timerId = -1; }
            return true;
        }
        if(action==MotionEvent.ACTION_DOWN && battleActive && timerBounds.contains(x,y)) {
            release();
            surface.cancelTouch();
            timerId=id;timerStartX=x;timerStartY=y;
            return true;
        }
        if(timerId>=0) {
            if(event.getPointerCount()>1 || Math.abs(x-timerStartX)>unit*.2f || Math.abs(y-timerStartY)>unit*.2f)
                suppressUntilUp=true;
            if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_CANCEL) {
                boolean pause=action==MotionEvent.ACTION_UP && !suppressUntilUp && battleActive && timerBounds.contains(x,y);
                timerId=-1;suppressUntilUp=false;
                if(pause) GameActivity.nativePauseBattle();
            }
            return true;
        }
        if (!padMode || event.getToolType(index) == MotionEvent.TOOL_TYPE_MOUSE ||
                event.getToolType(index) == MotionEvent.TOOL_TYPE_STYLUS)
            return surface.forwardTouch(this, event);
        if (action == MotionEvent.ACTION_CANCEL) { release(); return true; }
        TouchPadBridge.attach();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int button = hit(x, y);
            if (button >= 0) pointers.put(id, button);
            else if (x < getWidth() * .5f && stickId < 0) {
                stickId = id;
                stickX = x;
                stickY = y;
                stick(x, y);
            } else pointers.put(id, -1);
        }
        for (int i = 0; i < event.getPointerCount(); i++) {
            int pointer = event.getPointerId(i);
            if (pointer == stickId) stick(event.getX(i), event.getY(i));
            else if (pointers.indexOfKey(pointer) >= 0)
                pointers.put(pointer, hit(event.getX(i), event.getY(i)));
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            if (id == stickId) {
                TouchPadBridge.axis(0, 0); TouchPadBridge.axis(1, 0);
                stickId = -1; stickDX = stickDY = 0;
            }
            pointers.delete(id);
        }
        syncButtons();
        invalidate();
        return true;
    }

    @Override public boolean performClick() { super.performClick(); return true; }

    private void drawComboArt(Canvas canvas, Bitmap image, Rect crop, float x, float y, float size) {
        float scale = size / Math.max(1, Math.max(crop.width(), crop.height()));
        float halfW = crop.width() * scale * .5f, halfH = crop.height() * scale * .5f;
        artBounds.set(x - halfW, y - halfH, x + halfW, y + halfH);
        canvas.drawBitmap(image, crop, artBounds, paint);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!padMode) return;
        paint.setStyle(Paint.Style.FILL);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setColor(0x44606060);
        canvas.drawCircle(stickX, stickY, radius, paint);
        paint.setColor(0x99ffffff);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * .025f);
        canvas.drawCircle(stickX, stickY, radius, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(stickId >= 0 ? 0xaaeeeeee : 0x66cccccc);
        canvas.drawCircle(stickX + stickDX * radius, stickY + stickDY * radius, radius * .42f, paint);
        long now = SystemClock.uptimeMillis();
        boolean animating = false;
        for (Button button : buttons) {
            boolean pressed = button.held || now < button.animationUntil;
            animating |= now < button.animationUntil;
            paint.setColor(pressed ? 0x99aaaaaa : 0x66000000);
            canvas.drawOval(button.bounds, paint);
            if (button.directionKey != 0) {
                paint.setAlpha(pressed ? 255 : 200);
                if (button.image != null && button.directionImage != null) {
                    boolean faceCombo = button.axis < 0;
                    boolean firstPressed = pressed && button.pressedImage != null;
                    boolean secondPressed = pressed && button.directionPressedImage != null;
                    drawComboArt(canvas, firstPressed ? button.pressedImage : button.image,
                            firstPressed ? button.pressedCrop : button.imageCrop,
                            button.bounds.centerX() - button.bounds.width() * (faceCombo ? .23f : .17f),
                            button.bounds.centerY(), button.bounds.width() * .44f);
                    drawComboArt(canvas, secondPressed ? button.directionPressedImage : button.directionImage,
                            secondPressed ? button.directionPressedCrop : button.directionCrop,
                            button.bounds.centerX() + button.bounds.width() * (faceCombo ? .23f : .25f),
                            button.bounds.centerY(), button.bounds.width() * (faceCombo ? .44f : .26f));
                } else {
                    paint.setColor(Color.WHITE);
                    paint.setTextSize(unit * .18f);
                    canvas.drawText(button.label.toUpperCase(java.util.Locale.ROOT), button.bounds.centerX(),
                            button.bounds.centerY() + unit * .06f, paint);
                }
                paint.setAlpha(255);
            } else if (button.image != null) {
                paint.setAlpha(pressed ? 255 : 200);
                boolean usePressed = pressed && button.pressedImage != null;
                Rect crop = usePressed ? button.pressedCrop : button.imageCrop;
                // Brief press-and-return motion, using the game's pressed artwork.
                float pulse = 1f;
                if (button.pressedImage != null && now < button.animationUntil) {
                    float progress = 1f - (button.animationUntil - now) / 120f;
                    pulse -= .12f * (float)Math.sin(Math.PI * progress);
                }
                float scale = button.bounds.width() * .72f * pulse / button.artSize;
                float halfW = crop.width() * scale * .5f, halfH = crop.height() * scale * .5f;
                artBounds.set(button.bounds.centerX() - halfW, button.bounds.centerY() - halfH,
                        button.bounds.centerX() + halfW, button.bounds.centerY() + halfH);
                canvas.drawBitmap(usePressed ? button.pressedImage : button.image, crop, artBounds, paint);
                paint.setAlpha(255);
            } else {
                paint.setColor(Color.WHITE);
                paint.setTextSize(unit * .24f);
                canvas.drawText(button.label.toUpperCase(java.util.Locale.ROOT), button.bounds.centerX(),
                        button.bounds.centerY() + unit * .08f, paint);
            }
        }
        if (animating) postInvalidateOnAnimation();
    }
}
