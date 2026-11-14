#ifndef MENU_PORT_RENDER_SCALE_H
#define MENU_PORT_RENDER_SCALE_H
/* Integer settings bridge: whole multipliers or percentages for half steps. */
static int Port_RenderScaleNormalize(int value) {
#ifdef __ANDROID__
    if(value==50 || value==150 || value==250) return value;
    return value<1 ? 1:value>3 ? 3:value;
#else
    return value<0 ? 0:value>8 ? 8:value;
#endif
}
static float Port_RenderScaleFactor(int value) {
    value=Port_RenderScaleNormalize(value);
    return value==50 || value==150 || value==250 ? value/100.0f:(float)value;
}
static int Port_RenderScaleEncode(float value) {
    int whole=(int)value;
    return value==(float)whole ? whole:(int)(value*100.0f+0.5f);
}
#endif
