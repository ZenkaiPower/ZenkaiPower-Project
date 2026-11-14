package com.zenkaipower.android;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.view.Gravity;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

/** Installer progress displayed above SDL's surface while text is generated. */
final class PreparationScreen extends ScrollView {
    private final TextView message;
    private final TextView percentage;
    private final ProgressBar progress;
    private final Button button;
    PreparationScreen(Activity activity, Runnable cancel) {
        super(activity);
        setFillViewport(true);
        setBackgroundColor(SetupScreen.PAPER);
        LinearLayout page = new LinearLayout(activity);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(20), dp(20), dp(20), dp(20));
        addView(page);

        LinearLayout header = new LinearLayout(activity);
        header.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout heading = new LinearLayout(activity);
        heading.setOrientation(LinearLayout.VERTICAL);
        TextView title = text(activity, activity.getString(R.string.app_name), 28, SetupScreen.INK);
        title.setTypeface(null, Typeface.BOLD);
        heading.addView(title);
        heading.addView(text(activity, "Setup", 14, SetupScreen.DIM));
        header.addView(heading, new LinearLayout.LayoutParams(0, -2, 1));
        ImageView icon = new ImageView(activity);
        icon.setImageResource(R.mipmap.ic_launcher);
        icon.setContentDescription(activity.getString(R.string.app_name));
        header.addView(icon, new LinearLayout.LayoutParams(dp(56), dp(56)));
        page.addView(header);

        LinearLayout card = new LinearLayout(activity);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(20), dp(16), dp(20));
        card.setBackground(box(SetupScreen.CARD, SetupScreen.BORDER));
        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(-1, -2);
        cardParams.topMargin = dp(24);
        page.addView(card, cardParams);
        message = text(activity, "Preparing game text…", 20, SetupScreen.INK);
        message.setTypeface(null, Typeface.BOLD);
        message.setAccessibilityLiveRegion(ACCESSIBILITY_LIVE_REGION_POLITE);
        card.addView(message);
        progress = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        progress.setProgressTintList(ColorStateList.valueOf(SetupScreen.BLUE));
        progress.setIndeterminate(true);
        progress.setIndeterminateTintList(ColorStateList.valueOf(SetupScreen.BLUE));
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(-1, dp(12));
        progressParams.topMargin = dp(18);
        card.addView(progress, progressParams);
        percentage = text(activity, "", 16, SetupScreen.DIM);
        percentage.setGravity(Gravity.END);
        percentage.setVisibility(GONE);
        card.addView(percentage);

        button = new Button(activity);
        button.setText("Cancel installation");
        button.setAllCaps(false);
        button.setTextColor(SetupScreen.BLUE);
        button.setMinHeight(dp(44));
        button.setPadding(dp(12), dp(6), dp(12), dp(6));
        StateListDrawable background = new StateListDrawable();
        background.addState(new int[]{android.R.attr.state_pressed}, box(0xffbdd4ef, SetupScreen.BLUE));
        background.addState(new int[]{}, box(0xfff0f0e8, SetupScreen.BORDER));
        button.setBackground(background);
        button.setOnClickListener(v -> cancel.run());
        LinearLayout.LayoutParams buttonParams = new LinearLayout.LayoutParams(-1, -2);
        buttonParams.topMargin = dp(12);
        page.addView(button, buttonParams);
    }

    void update(String text, boolean cancellable) {
        update(text, cancellable, -1);
    }

    void update(String text, boolean cancellable, int percent) {
        message.setText(text);
        progress.setIndeterminate(percent < 0);
        percentage.setVisibility(percent < 0 ? GONE : VISIBLE);
        if (percent >= 0) {
            int value = Math.max(0, Math.min(100, percent));
            progress.setProgress(value);
            percentage.setText(value + "%");
        }
        button.setVisibility(cancellable ? VISIBLE : GONE);
        button.setEnabled(cancellable);
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private GradientDrawable box(int fill, int edge) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setStroke(dp(1), edge);
        return shape;
    }
    private static TextView text(Activity activity, String value, int size, int color) {
        TextView view = new TextView(activity);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }
}
