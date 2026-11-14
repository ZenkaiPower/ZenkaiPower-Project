package com.zenkaipower.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.graphics.Typeface;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;

/** Checks published releases without blocking setup or installing an update. */
final class SetupUpdates {
    static void show(Activity activity) {
        String current;
        try { current=activity.getPackageManager().getPackageInfo(activity.getPackageName(),0).versionName; }
        catch(android.content.pm.PackageManager.NameNotFoundException error) { current="unknown"; }
        final String installed=current;
        int padding=Math.round(20*activity.getResources().getDisplayMetrics().density);
        LinearLayout content=new LinearLayout(activity);content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(padding,padding,padding,padding);content.setBackgroundColor(SetupScreen.PAPER);
        TextView heading=new TextView(activity);heading.setText("Check for updates");heading.setTextSize(22);
        heading.setTypeface(null,Typeface.BOLD);heading.setTextColor(SetupScreen.INK);content.addView(heading);
        TextView message=new TextView(activity);message.setTextSize(15);message.setTextColor(SetupScreen.DIM);
        message.setPadding(0,padding,0,padding);message.setText("Installed version: "+installed+"\n\nChecking the latest release…");
        message.setAccessibilityLiveRegion(android.view.View.ACCESSIBILITY_LIVE_REGION_POLITE);content.addView(message);
        String[] releasePage={activity.getString(R.string.release_url)+"/latest"};
        AlertDialog dialog=new AlertDialog.Builder(activity).setView(content)
                .setNegativeButton("Close",null).setPositiveButton("Latest release",(d,which) -> {
                    try { activity.startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(releasePage[0]))); }
                    catch(android.content.ActivityNotFoundException ignored) { }
                }).create();
        dialog.show();
        // The platform theme supplies a separate background behind the buttons.
        if(dialog.getWindow()!=null)
            dialog.getWindow().setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(SetupScreen.PAPER));
        android.view.View buttonPanel=(android.view.View)dialog.getButton(AlertDialog.BUTTON_POSITIVE).getParent();
        buttonPanel.setBackgroundColor(SetupScreen.PAPER);
        for(int which:new int[]{AlertDialog.BUTTON_POSITIVE,AlertDialog.BUTTON_NEGATIVE}) {
            android.widget.Button button=dialog.getButton(which);
            button.setAllCaps(false);
            if(which==AlertDialog.BUTTON_POSITIVE) {
                android.view.ViewGroup.LayoutParams params=button.getLayoutParams();
                if(params instanceof android.view.ViewGroup.MarginLayoutParams) {
                    ((android.view.ViewGroup.MarginLayoutParams)params).setMarginStart(
                            Math.round(12*activity.getResources().getDisplayMetrics().density));
                    button.setLayoutParams(params);
                }
            }
            boolean primary=which==AlertDialog.BUTTON_POSITIVE;
            button.setTextColor(primary?android.graphics.Color.WHITE:SetupScreen.BLUE);
            android.graphics.drawable.StateListDrawable background=new android.graphics.drawable.StateListDrawable();
            for(int state:new int[]{android.R.attr.state_pressed,0}) {
                android.graphics.drawable.GradientDrawable shape=new android.graphics.drawable.GradientDrawable();
                shape.setColor(state==0?(primary?SetupScreen.BLUE:0xfff0f0e8):(primary?0xff245398:0xffbdd4ef));
                shape.setStroke(Math.round(activity.getResources().getDisplayMetrics().density),
                        primary||state!=0?SetupScreen.BLUE:SetupScreen.BORDER);
                background.addState(state==0?new int[]{}:new int[]{state},shape);
            }
            button.setBackground(background);
        }
        new Thread(() -> {
            String result;
            try {
                byte[] bytes=MatchmakingHttp.request(activity.getString(R.string.release_api_url),null,
                        new String[]{"Accept: application/vnd.github+json"},256*1024);
                if(bytes==null) throw new java.io.IOException();
                JSONObject release=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
                String latest=release.getString("tag_name");
                int comparison=compare(latest,installed);
                result="Installed version: "+installed+"\nLatest release: "+latest+"\n\n"+
                        (comparison>0 ? "A new version is available." : comparison==0 ? "You have the latest version." : "Your installed version is newer than the latest published release.");
                String page=release.optString("html_url");
                if(page.startsWith(activity.getString(R.string.release_url)+"/tag/")) releasePage[0]=page;
            } catch(Exception error) {
                result="Installed version: "+installed+"\n\nCould not check for updates. Check your internet connection, or open the latest release page below.";
            }
            String text=result;
            activity.runOnUiThread(() -> {
                if(!activity.isDestroyed() && !activity.isFinishing() && dialog.isShowing()) message.setText(text);
            });
        },"setup-update-check").start();
    }

    static int compare(String left,String right) {
        String[] a=left.replaceFirst("^[vV]","").split("\\+",2)[0].split("-",2);
        String[] b=right.replaceFirst("^[vV]","").split("\\+",2)[0].split("-",2);
        String[] x=a[0].split("\\."), y=b[0].split("\\.");
        for(int i=0;i<Math.max(x.length,y.length);i++) {
            int difference=new java.math.BigInteger(i<x.length?x[i]:"0").compareTo(new java.math.BigInteger(i<y.length?y[i]:"0"));
            if(difference!=0) return difference;
        }
        if(a.length!=b.length) return a.length==1?1:-1;
        return a.length==1?0:a[1].compareTo(b[1]);
    }
}
