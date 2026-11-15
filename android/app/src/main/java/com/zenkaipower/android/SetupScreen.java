package com.zenkaipower.android;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.util.concurrent.ExecutorService;

/** Android layout using the desktop setup's colors, disc cards and edition viewer. */
final class SetupScreen {
    static final int PAPER=0xffeeede6, CARD=0xfff8f7f3, INK=0xff141414,
            DIM=0xff61615c, BLUE=0xff143d80, GREEN=0xff197a29, BORDER=0xff9e9e94;
    static final String[] TITLES={"North America - PlayStation 2", "Europe - PlayStation 2", "Europe - Wii", "Japan - PlayStation 2"};
    static final String[] IDS={"SLUS-21678", "SLES-54945", "RDSPAF", "SLPS-25815"};
    private static final String[] DATES={"November 13, 2007", "November 9, 2007", "February 15, 2008", "October 4, 2007"};
    final LinearLayout layout;
    final TextView folder, status;
    final Button choose, play, cancel;
    final Button[] install=new Button[4];
    private final TextView[] states=new TextView[4];
    private final ImageView[] icons=new ImageView[4];
    private final View[] rims=new View[4];
    private final LinearLayout discList;
    private final Button discToggle;
    private boolean discsExpanded;
    private final SharedPreferences preferences;
    private final ImageView cover;
    private final LauncherActivity activity;
    private final ExecutorService artwork;
    private File root;
    private int mask;
    private int generation;
    private final Bitmap[] logoCovers=new Bitmap[2];
    private int selectedLogo;

    SetupScreen(LauncherActivity activity, ExecutorService artwork, String[] descriptions) {
        this.activity=activity; this.artwork=artwork;
        preferences=activity.getSharedPreferences("setup",Context.MODE_PRIVATE);
        selectedLogo=preferences.getBoolean("japanese-logo",true) ? 1 : 0;
        layout=column(); layout.setPadding(dp(20), dp(20), dp(20), dp(20));
        layout.setBackgroundColor(PAPER);
        LinearLayout header=new LinearLayout(activity); header.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout heading=column();
        TextView name=text(activity.getString(R.string.app_name),28,INK); name.setTypeface(null,android.graphics.Typeface.BOLD); heading.addView(name);
        heading.addView(text("Setup",14,DIM));
        header.addView(heading,new LinearLayout.LayoutParams(0,-2,1));
        ImageView logo=new ImageView(activity); logo.setImageResource(R.mipmap.ic_launcher);
        logo.setContentDescription(activity.getString(R.string.app_name)); header.addView(logo,new LinearLayout.LayoutParams(dp(56),dp(56)));
        layout.addView(header);
        status=text("",16,DIM); status.setTextIsSelectable(true);
        status.setPadding(dp(12),dp(12),dp(12),dp(12));
        status.setVisibility(View.GONE); layout.addView(status);
        section("Game folder");
        folder=text("",12,DIM); folder.setTextIsSelectable(true); folder.setPadding(dp(10),dp(8),dp(10),dp(8));
        folder.setBackground(box(Color.WHITE,BORDER)); layout.addView(folder);
        choose=button("Choose game folder",false); layout.addView(choose);
        discToggle=button("",false); layout.addView(discToggle);
        discList=column(); layout.addView(discList);
        discsExpanded=preferences.getBoolean("discs-expanded",true);
        discToggle.setOnClickListener(v -> {
            discsExpanded=!discsExpanded;
            preferences.edit().putBoolean("discs-expanded",discsExpanded).apply();
            updateDiscExpansion();
        });
        updateDiscExpansion();
        for(int i=0;i<4;i++) {
            LinearLayout card=new LinearLayout(activity); card.setBackgroundColor(CARD);
            View rim=new View(activity); rim.setBackgroundColor(BORDER); rims[i]=rim;
            card.addView(rim,new LinearLayout.LayoutParams(dp(3),-1));
            LinearLayout content=column(); content.setPadding(dp(10),dp(10),dp(10),dp(8));
            card.addView(content,new LinearLayout.LayoutParams(0,-2,1));
            LinearLayout top=new LinearLayout(activity); top.setGravity(Gravity.CENTER_VERTICAL);
            ImageView icon=new ImageView(activity); icon.setImageDrawable(new Disc()); icon.setPadding(dp(4),dp(4),dp(4),dp(4));
            icon.setContentDescription("View edition details: "+TITLES[i]); icons[i]=icon;
            final int disc=i; icon.setOnClickListener(v -> showEdition(disc));
            top.addView(icon,new LinearLayout.LayoutParams(dp(48),dp(48)));
            LinearLayout title=column(); title.setPadding(dp(8),0,0,0);
            TextView edition=text(TITLES[i],15,INK); edition.setTypeface(null,android.graphics.Typeface.BOLD); title.addView(edition);
            states[i]=text("Not installed  |  "+IDS[i],12,DIM); title.addView(states[i]);
            top.addView(title,new LinearLayout.LayoutParams(0,-2,1));
            content.addView(top);
            LinearLayout details=column(); content.addView(details);
            TextView description=text(descriptions[i],13,DIM); description.setPadding(0,dp(5),0,dp(3)); details.addView(description);
            install[i]=button("Install",false); details.addView(install[i]);
            LinearLayout.LayoutParams row=new LinearLayout.LayoutParams(-1,-2); row.bottomMargin=dp(10); discList.addView(card,row);
        }
        play=button("Play",true); play.setTextSize(18); layout.addView(play);
        // The cover always follows Play, including when the screen is scrolled.
        cover=new ImageView(activity); cover.setAdjustViewBounds(true); cover.setMaxHeight(dp(600));
        cover.setScaleType(ImageView.ScaleType.FIT_CENTER); cover.setContentDescription("Game cover"); cover.setVisibility(View.GONE);
        LinearLayout.LayoutParams art=new LinearLayout.LayoutParams(-1,-2); art.topMargin=dp(12); layout.addView(cover,art);
        cover.setOnClickListener(v -> {
            if(logoCovers[0]==null || logoCovers[1]==null) return;
            selectedLogo=1-selectedLogo;
            preferences.edit().putBoolean("japanese-logo",selectedLogo==1).apply();
            showLogoCover();
        });
        cancel=button("Cancel installation",false); layout.addView(cancel);
        TextView version=text("",12,DIM);
        try { version.setText("Version "+activity.getPackageManager().getPackageInfo(activity.getPackageName(),0).versionName); }
        catch(android.content.pm.PackageManager.NameNotFoundException ignored) { }
        LinearLayout footer=new LinearLayout(activity); footer.setGravity(Gravity.TOP);
        TextView updates=text("",12,BLUE);
        android.text.SpannableString updateLink=new android.text.SpannableString("Check for a new version here.");
        updateLink.setSpan(new android.text.style.ClickableSpan() {
            @Override public void onClick(View view) { SetupUpdates.show(activity); }
        },0,updateLink.length(),android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        updates.setText(updateLink);updates.setLinkTextColor(BLUE);
        updates.setMovementMethod(android.text.method.LinkMovementMethod.getInstance());
        version.setGravity(Gravity.LEFT); version.setPadding(0,0,dp(12),0);
        footer.addView(version,new LinearLayout.LayoutParams(-2,-2));
        updates.setGravity(Gravity.RIGHT);
        footer.addView(updates,new LinearLayout.LayoutParams(0,-2,1));
        layout.addView(footer);
        TextView website=linkedText("", new String[]{activity.getString(R.string.app_name)},
                new String[]{activity.getString(R.string.project_page_url)}, "");
        website.setPadding(0,dp(10),0,0); layout.addView(website);
        TextView projects=linkedText("Also check out the ",
                new String[]{"Tenkaichi3Decomp","Tenkaichi3DecompAndroid"},
                new String[]{activity.getString(R.string.original_project_url),"https://github.com/dfdx047/Tenkaichi3DecompAndroid"}," projects.");
        projects.setPadding(0,dp(10),0,0); layout.addView(projects);
        ScrollView scroll=new ScrollView(activity); scroll.setFillViewport(true); scroll.setBackgroundColor(PAPER); scroll.addView(layout);
        activity.setContentView(scroll);
        activity.getWindow().setStatusBarColor(PAPER); activity.getWindow().setNavigationBarColor(PAPER);
        activity.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR|View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
    }
    private void updateDiscExpansion() {
        discList.setVisibility(discsExpanded ? View.VISIBLE : View.GONE);
        discToggle.setText((discsExpanded ? "\u25be " : "\u25b8 ")+"Disc images");
        discToggle.setContentDescription((discsExpanded ? "Hide" : "Show")+" all disc images");
    }
    void clearError() {
        status.setBackgroundColor(Color.TRANSPARENT);
        status.setTextColor(DIM); status.setTypeface(null,android.graphics.Typeface.NORMAL);
        status.setVisibility(View.VISIBLE);
    }
    void showError(String message) {
        status.setText("Installation failed\n"+message);
        status.setTextColor(0xff8b1515); status.setTypeface(null,android.graphics.Typeface.BOLD);
        status.setBackground(box(0xffffe8e5,0xffb3261e)); status.setVisibility(View.VISIBLE);
        layout.post(() -> { if(layout.getParent() instanceof ScrollView) ((ScrollView)layout.getParent()).smoothScrollTo(0,0); });
        new AlertDialog.Builder(activity).setTitle("Installation failed").setMessage(message)
            .setPositiveButton("OK",null).show();
    }
    private int dp(int n) { return Math.round(n*activity.getResources().getDisplayMetrics().density); }
    private TextView linkedText(String prefix,String[] labels,String[] urls,String suffix) {
        android.text.SpannableStringBuilder content=new android.text.SpannableStringBuilder(prefix);
        for(int i=0;i<labels.length;i++) {
            if(i>0) content.append(" and ");
            int start=content.length();content.append(labels[i]);
            content.setSpan(new android.text.style.URLSpan(urls[i]),start,content.length(),android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        content.append(suffix);
        TextView view=text("",12,DIM);view.setText(content);view.setLinkTextColor(BLUE);
        view.setMovementMethod(android.text.method.LinkMovementMethod.getInstance());return view;
    }
    private LinearLayout column() { LinearLayout l=new LinearLayout(activity); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private TextView text(String s,int size,int color) { TextView v=new TextView(activity); v.setText(s); v.setTextSize(size); v.setTextColor(color); return v; }
    private void section(String title) { TextView t=text(title,18,INK); t.setTypeface(null,android.graphics.Typeface.BOLD); t.setPadding(0,dp(18),0,dp(9)); layout.addView(t); }
    private GradientDrawable box(int fill,int edge) { GradientDrawable b=new GradientDrawable(); b.setColor(fill); b.setStroke(dp(1),edge); return b; }
    private Button button(String title,boolean primary) {
        Button b=new Button(activity); b.setText(title); b.setAllCaps(false); b.setMinHeight(dp(44));
        b.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{0xff999992,primary?Color.WHITE:BLUE}));
        android.graphics.drawable.StateListDrawable backgrounds=new android.graphics.drawable.StateListDrawable();
        backgrounds.addState(new int[]{-android.R.attr.state_enabled},box(0xffe1e0d9,BORDER));
        backgrounds.addState(new int[]{android.R.attr.state_pressed},box(primary?0xff245398:0xffbdd4ef,BLUE));
        backgrounds.addState(new int[]{},box(primary?BLUE:0xfff0f0e8,primary?BLUE:BORDER));
        b.setBackground(backgrounds); b.setPadding(dp(12),dp(6),dp(12),dp(6));
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2); p.topMargin=dp(7); b.setLayoutParams(p); return b;
    }
    void states(boolean[] installed, int verified) {
        mask=verified;
        for(int i=0;i<4;i++) {
            boolean ok=(mask&(1<<i))!=0;
            states[i].setText((ok ? installed[i]?"Installed":"Verified" : installed[i]?"Not verified":"Not installed")+"  |  "+IDS[i]);
            states[i].setTextColor(ok?GREEN:DIM); rims[i].setBackgroundColor(ok?GREEN:BORDER);
            icons[i].setEnabled(ok); icons[i].setClickable(ok);
            install[i].setText(installed[i]?"Reinstall":"Install");
        }
    }
    private void showLogoCover() {
        boolean both=logoCovers[0]!=null && logoCovers[1]!=null;
        Bitmap bitmap=logoCovers[selectedLogo];
        if(bitmap==null) bitmap=logoCovers[1-selectedLogo];
        cover.setImageBitmap(bitmap);
        cover.setVisibility(bitmap==null ? View.GONE : View.VISIBLE);
        cover.setClickable(both);
        cover.setContentDescription(both ? "Game logo. Tap to switch logo" : "Game cover");
    }
    void artwork(File selected,int verified) {
        root=selected; mask=verified; final int ticket=++generation;
        cover.setImageDrawable(null); cover.setVisibility(View.GONE);
        logoCovers[0]=logoCovers[1]=null; cover.setClickable(false);
        for(ImageView icon:icons) icon.setImageDrawable(new Disc());
        if(selected==null) return;
        artwork.execute(() -> {
            Bitmap[] banners=new Bitmap[2];
            for(int i=0;i<2;i++) {
                if(i==1 && (verified&8)==0) continue;
                Bitmap banner=SetupArtwork.load(selected,0,0);
                banners[i]=(verified&1)!=0 ? coverWithLogo(banner,selected,i==1) : banner;
            }
            if(banners[0]==null && banners[1]==null) banners[0]=SetupArtwork.load(selected,0,0);
            Bitmap[] discs=new Bitmap[4];
            for(int i=0;i<4;i++) if((verified&(1<<i))!=0) discs[i]=SetupArtwork.load(selected,i,1);
            activity.runOnUiThread(() -> {
                if(ticket!=generation || activity.isDestroyed()) return;
                logoCovers[0]=banners[0]; logoCovers[1]=banners[1];
                showLogoCover();
                for(int i=0;i<4;i++) if(discs[i]!=null) icons[i].setImageBitmap(discs[i]);
            });
        });
    }
    private static Bitmap coverWithLogo(Bitmap banner,File root,boolean japanese) {
        Bitmap logo=null;
        for(String name:japanese ? new String[]{"title-sm.png"} : new String[]{"title-zenkaipower.png"}) {
            try {
                byte[] bytes=GamePak.read(GameIO.file(root,"gamedata/game-text.pak"),name);
                try { logo=android.graphics.BitmapFactory.decodeByteArray(bytes,0,bytes.length); }
                finally { java.util.Arrays.fill(bytes,(byte)0); }
            } catch(java.io.IOException ignored) { }
            if(logo!=null) break;
        }
        if(logo==null) {
            if(banner!=null) banner.recycle();
            return null;
        }
        int left=logo.getWidth(),top=logo.getHeight(),right=0,bottom=0;
        for(int y=0;y<logo.getHeight();y++) for(int x=0;x<logo.getWidth();x++) {
            if((logo.getPixel(x,y)>>>24)!=0) {
                left=Math.min(left,x);top=Math.min(top,y);
                right=Math.max(right,x+1);bottom=Math.max(bottom,y+1);
            }
        }
        if(right<=left || bottom<=top) {logo.recycle();return banner;}
        // Disc logos also stand alone when no optional cover is supplied.
        boolean standalone=banner==null;
        if(standalone) banner=Bitmap.createBitmap(512,240,Bitmap.Config.ARGB_8888);
        Bitmap result=banner.copy(Bitmap.Config.ARGB_8888,true);
        if(result==null) {logo.recycle();return banner;}
        Canvas canvas=new Canvas(result);
        float width=result.getWidth(),height=result.getHeight();
        Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        if(!standalone) {
            paint.setShader(new android.graphics.LinearGradient(0,height*.72f,0,height,
                    Color.TRANSPARENT,0xdd000000,android.graphics.Shader.TileMode.CLAMP));
            canvas.drawRect(0,height*.72f,width,height,paint);paint.setShader(null);
        }
        float scale=Math.min(width*.94f/(right-left),height*(standalone ? .80f : .24f)/(bottom-top));
        float w=(right-left)*scale,h=(bottom-top)*scale,margin=height*.025f;
        float logoTop=standalone ? (height-h)*.5f : height-margin-h;
        canvas.drawBitmap(logo,new android.graphics.Rect(left,top,right,bottom),
                new android.graphics.RectF((width-w)*.5f,logoTop,(width+w)*.5f,logoTop+h),paint);
        logo.recycle();banner.recycle();return result;
    }
    private void showEdition(int disc) {
        if(root==null || (mask&(1<<disc))==0) return;
        File selected=root;
        LinearLayout content=column(); content.setPadding(dp(16),dp(12),dp(16),dp(12)); content.setBackgroundColor(PAPER);
        String title=disc==3?"Dragon Ball Z: Sparking! METEOR":"Dragon Ball Z: Budokai Tenkaichi 3";
        content.addView(text(title,20,INK));
        TextView metadata=text("Release date: "+DATES[disc]+"\nGame ID: "+IDS[disc],14,DIM); metadata.setPadding(0,dp(10),0,dp(16)); content.addView(metadata);
        LinearLayout pictures=new LinearLayout(activity); String[] names={"Front","Back","Disc"}; ImageView[] slots=new ImageView[3];
        TextView[] loading=new TextView[3];
        for(int i=0;i<3;i++) {
            LinearLayout column=column(); column.setPadding(dp(4),0,dp(4),0); column.addView(text(names[i],14,INK));
            ImageView view=new ImageView(activity); view.setScaleType(ImageView.ScaleType.FIT_CENTER); view.setContentDescription(names[i]+" artwork");
            column.addView(view,new LinearLayout.LayoutParams(-1,dp(170))); slots[i]=view;
            loading[i]=text(disc==2 && i==1?"Back artwork unavailable":"Loading artwork...",12,DIM); column.addView(loading[i]);
            pictures.addView(column,new LinearLayout.LayoutParams(0,-2,1));
        }
        content.addView(pictures); content.addView(text("Tap artwork to enlarge. Pinch to zoom.",12,DIM));
        ScrollView scroll=new ScrollView(activity); scroll.addView(content);
        AlertDialog dialog=new AlertDialog.Builder(new android.view.ContextThemeWrapper(activity,android.R.style.Theme_Material_Light_Dialog_Alert))
                .setTitle(TITLES[disc]).setView(scroll).setPositiveButton("Back",null).create(); dialog.show();
        artwork.execute(() -> {
            for(int i=0;i<3;i++) {
                final Bitmap bitmap=SetupArtwork.load(selected,disc,i+2); final ImageView slot=slots[i]; final TextView caption=loading[i];
                activity.runOnUiThread(() -> {
                    if(!dialog.isShowing()) return;
                    if(bitmap!=null) { slot.setImageBitmap(bitmap); slot.setOnClickListener(v -> enlarge(bitmap)); caption.setVisibility(View.GONE); }
                    else caption.setText("Artwork unavailable");
                });
            }
        });
    }
    private void enlarge(Bitmap bitmap) {
        Dialog dialog=new Dialog(activity,android.R.style.Theme_Material_Light_NoActionBar_Fullscreen);
        LinearLayout body=column(); body.setBackgroundColor(PAPER);
        Button back=button("Back",false); back.setOnClickListener(v -> dialog.dismiss()); body.addView(back);
        ZoomImage view=new ZoomImage(activity,bitmap); body.addView(view,new LinearLayout.LayoutParams(-1,0,1));
        dialog.setContentView(body); dialog.show(); Window window=dialog.getWindow(); if(window!=null) window.setLayout(-1,-1);
    }
    private static final class Disc extends Drawable {
        private final Paint paint=new Paint(3);
        @Override public void draw(Canvas canvas) {
            float x=getBounds().exactCenterX(),y=getBounds().exactCenterY(),r=Math.min(getBounds().width(),getBounds().height())*.46f;
            paint.setColor(0xffdde1e6); paint.setStyle(Paint.Style.FILL); canvas.drawCircle(x,y,r,paint);
            paint.setColor(BORDER); paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(Math.max(1,r*.04f)); canvas.drawCircle(x,y,r,paint);
            paint.setColor(Color.WHITE); paint.setStrokeWidth(r*.13f); canvas.drawArc(x-r*.75f,y-r*.75f,x+r*.75f,y+r*.75f,210,85,false,paint);
            paint.setStyle(Paint.Style.FILL); paint.setColor(BORDER); canvas.drawCircle(x,y,r*.31f,paint);
            paint.setColor(CARD); canvas.drawCircle(x,y,r*.16f,paint);
        }
        @Override public void setAlpha(int alpha) { paint.setAlpha(alpha); }
        @Override public void setColorFilter(android.graphics.ColorFilter f) { paint.setColorFilter(f); }
        @Override public int getOpacity() { return android.graphics.PixelFormat.TRANSLUCENT; }
    }
    private static final class ZoomImage extends View {
        private final Bitmap bitmap;
        private final Paint paint=new Paint(3);
        private final android.graphics.Matrix matrix=new android.graphics.Matrix();
        private final android.view.ScaleGestureDetector scale;
        private final android.view.GestureDetector gestures;
        ZoomImage(android.content.Context context,Bitmap bitmap) {
            super(context); this.bitmap=bitmap; setContentDescription("Edition artwork. Pinch to zoom; double tap to fit.");
            scale=new android.view.ScaleGestureDetector(context,new android.view.ScaleGestureDetector.SimpleOnScaleGestureListener() {
                @Override public boolean onScale(android.view.ScaleGestureDetector detector) {
                    float[] values=new float[9]; matrix.getValues(values);
                    float base=Math.min((float)getWidth()/bitmap.getWidth(),(float)getHeight()/bitmap.getHeight());
                    float current=values[android.graphics.Matrix.MSCALE_X];
                    float next=Math.max(base,Math.min(base*8,current*detector.getScaleFactor()));
                    matrix.postScale(next/current,next/current,detector.getFocusX(),detector.getFocusY()); invalidate(); return true;
                }
            });
            gestures=new android.view.GestureDetector(context,new android.view.GestureDetector.SimpleOnGestureListener() {
                @Override public boolean onDown(android.view.MotionEvent event) { return true; }
                @Override public boolean onScroll(android.view.MotionEvent first,android.view.MotionEvent last,float x,float y) {
                    if(!scale.isInProgress()) { matrix.postTranslate(-x,-y); invalidate(); } return true;
                }
                @Override public boolean onDoubleTap(android.view.MotionEvent event) { fit(); return true; }
            });
        }
        private void fit() {
            matrix.setRectToRect(new android.graphics.RectF(0,0,bitmap.getWidth(),bitmap.getHeight()),
                    new android.graphics.RectF(0,0,getWidth(),getHeight()),android.graphics.Matrix.ScaleToFit.CENTER); invalidate();
        }
        @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) { fit(); }
        @Override protected void onDraw(Canvas canvas) { canvas.drawBitmap(bitmap,matrix,paint); }
        @Override public boolean onTouchEvent(android.view.MotionEvent event) {
            scale.onTouchEvent(event); gestures.onTouchEvent(event);
            if(event.getActionMasked()==android.view.MotionEvent.ACTION_UP) performClick();
            return true;
        }
        @Override public boolean performClick() { super.performClick(); return true; }
    }
}
