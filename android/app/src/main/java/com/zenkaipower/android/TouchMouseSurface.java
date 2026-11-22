package com.zenkaipower.android;

import android.content.Context;
import android.graphics.Matrix;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/** Finger gestures feed the existing mouse menus, without SDL's immediate click. */
final class TouchMouseSurface extends SDLSurface {
    private final float slopSquared;
    private int primaryId = -1;
    private int secondaryId = -1;
    private float startX, startY, secondX, secondY, mouseX, mouseY;
    private long started;
    private boolean moved, multiple, cancelled, dragging;
    private int cameraDragMode;
    private float dragX,dragY;

    void cancelTouch() {
        endDrag();
        primaryId = -1;
        GameActivity.nativeCameraDrag(0,0,0);
    }

    private void endDrag() {
        if (!dragging) return;
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_UP, mouseX, mouseY, false);
        dragging = false;
    }

    TouchMouseSurface(Context context) {
        super(context);
        float slop = ViewConfiguration.get(context).getScaledTouchSlop();
        slopSquared = slop * slop;
    }

    private static float distanceSquared(float x, float y, float originX, float originY) {
        float dx = x - originX, dy = y - originY;
        return dx * dx + dy * dy;
    }

    private void move(float x, float y) {
        // MotionEvent uses view coordinates; SDL uses the surface buffer.
        // Android can resize these separately during fullscreen/inset changes.
        mouseX = getWidth() > 0 ? x * mWidth / getWidth() : x;
        mouseY = getHeight() > 0 ? y * mHeight / getHeight() : y;
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_HOVER_MOVE, mouseX, mouseY, false);
    }

    boolean forwardTouch(View source, MotionEvent event) {
        // The virtual pad covers the activity, which can have different bounds
        // from SDL's SurfaceView while Android applies fullscreen insets.
        Matrix transform = new Matrix();
        source.transformMatrixToGlobal(transform);
        transformMatrixToLocal(transform);
        MotionEvent local = MotionEvent.obtain(event);
        try {
            local.transform(transform);
            return onTouch(this, local);
        } finally { local.recycle(); }
    }

    private void click(int button) {
        SDLActivity.onNativeMouse(button, MotionEvent.ACTION_DOWN, mouseX, mouseY, false);
        // SDL expects the remaining held-button mask, not the released button.
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_UP, mouseX, mouseY, false);
    }

    @Override public boolean onTouch(View view, MotionEvent event) {
        // Physical mice and pens retain SDL's normal handling.
        int tool = event.getToolType(event.getActionIndex());
        if (tool != MotionEvent.TOOL_TYPE_FINGER && tool != MotionEvent.TOOL_TYPE_UNKNOWN)
            return super.onTouch(view, event);

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            primaryId = event.getPointerId(0);
            secondaryId = -1;
            startX = event.getX();
            startY = event.getY();
            dragX=startX;dragY=startY;
            cameraDragMode=GameActivity.nativeCameraMode();
            if (cameraDragMode==2 && startX<getWidth()*.5f) cameraDragMode=0;
            started = event.getEventTime();
            moved = multiple = cancelled = false;
            move(startX, startY);
            return true;
        }
        if (primaryId == -1) return true;
        if (action == MotionEvent.ACTION_CANCEL) {
            cancelTouch();
            return true;
        }
        if (action == MotionEvent.ACTION_POINTER_DOWN) {
            endDrag();
            GameActivity.nativeCameraDrag(0,0,0);
            if (multiple || event.getPointerCount() != 2) cancelled = true;
            multiple = true;
            int index = event.getActionIndex();
            secondaryId = event.getPointerId(index);
            secondX = event.getX(index);
            secondY = event.getY(index);
        }
        int primary = event.findPointerIndex(primaryId);
        if (primary >= 0) {
            float x = event.getX(primary), y = event.getY(primary);
            moved |= distanceSquared(x, y, startX, startY) > slopSquared;
            if (!multiple) {
                if (moved && !dragging && cameraDragMode == 0) {
                    move(startX, startY);
                    SDLActivity.onNativeMouse(MotionEvent.BUTTON_TERTIARY, MotionEvent.ACTION_DOWN,
                            mouseX, mouseY, false);
                    dragging = true;
                }
                if (cameraDragMode!=0 && moved && !cancelled && getWidth()>0 && getHeight()>0) {
                    GameActivity.nativeCameraDrag(cameraDragMode,(x-dragX)/getWidth(),(y-dragY)/getHeight());
                    dragX=x;dragY=y;
                }
                move(x, y);
            }
        }
        int secondary = event.findPointerIndex(secondaryId);
        if (secondary >= 0)
            moved |= distanceSquared(event.getX(secondary), event.getY(secondary),
                    secondX, secondY) > slopSquared;

        if (action == MotionEvent.ACTION_UP) {
            endDrag();
            if (!cancelled && !moved && event.getEventTime() - started <= ViewConfiguration.getLongPressTimeout())
                click(multiple ? MotionEvent.BUTTON_SECONDARY : MotionEvent.BUTTON_PRIMARY);
            primaryId = -1;
        }
        // Consume fingers so SDL does not synthesize a second left click.
        return true;
    }
}
