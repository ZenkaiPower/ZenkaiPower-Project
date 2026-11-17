package com.zenkaipower.android;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Icon;
import java.util.Collections;

/** Home-screen shortcuts use artwork prepared from the installed discs. */
final class LauncherShortcuts {
    static void publishSetup(Activity activity) {
        ShortcutManager manager=activity.getSystemService(ShortcutManager.class);
        Intent intent=new Intent(activity,LauncherActivity.class).setAction(Intent.ACTION_VIEW)
                .putExtra("showSetup",true);
        manager.setDynamicShortcuts(Collections.singletonList(new ShortcutInfo.Builder(activity,"setup")
                .setShortLabel("Setup").setLongLabel("Game setup and settings")
                .setIcon(Icon.createWithResource(activity,R.mipmap.ic_launcher)).setIntent(intent).build()));
    }
    static void pin(Activity activity,Bitmap logo,boolean japanese) {
        ShortcutManager manager=activity.getSystemService(ShortcutManager.class);
        if(!manager.isRequestPinShortcutSupported()) {
            android.widget.Toast.makeText(activity,"Your launcher does not support adding shortcuts.",android.widget.Toast.LENGTH_LONG).show();
            return;
        }
        int left=logo.getWidth(),top=logo.getHeight(),right=0,bottom=0;
        for(int y=0;y<logo.getHeight();y++) for(int x=0;x<logo.getWidth();x++) {
            if((logo.getPixel(x,y)>>>24)!=0) {
                left=Math.min(left,x);top=Math.min(top,y);
                right=Math.max(right,x+1);bottom=Math.max(bottom,y+1);
            }
        }
        if(right<=left || bottom<=top) return;
        int size=Math.max(48,Math.min(512,Math.max(manager.getIconMaxWidth(),manager.getIconMaxHeight())));
        Bitmap icon=Bitmap.createBitmap(size,size,Bitmap.Config.ARGB_8888);
        Canvas canvas=new Canvas(icon);
        float scale=size*.9f/Math.max(right-left,bottom-top);
        float w=(right-left)*scale,h=(bottom-top)*scale;
        canvas.drawBitmap(logo,new Rect(left,top,right,bottom),
                new RectF((size-w)*.5f,(size-h)*.5f,(size+w)*.5f,(size+h)*.5f),new Paint(3));
        Intent intent=new Intent(activity,LauncherActivity.class).setAction(Intent.ACTION_VIEW)
                .putExtra("startGame",true);
        String name=japanese ? "Sparking! METEOR" : "Budokai Tenkaichi 3";
        boolean requested=manager.requestPinShortcut(new ShortcutInfo.Builder(activity,japanese ? "meteor" : "tenkaichi3")
                .setShortLabel(name).setLongLabel("Dragon Ball Z: "+name)
                .setIcon(Icon.createWithBitmap(icon)).setIntent(intent).build(),null);
        icon.recycle();
        if(!requested) android.widget.Toast.makeText(activity,"Could not request a home-screen shortcut.",android.widget.Toast.LENGTH_LONG).show();
    }
}
