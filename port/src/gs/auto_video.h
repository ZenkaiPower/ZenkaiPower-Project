#ifndef GS_AUTO_VIDEO_H
#define GS_AUTO_VIDEO_H
/* Match the renderer's supported aspect interval and integer internal scales. */
static inline int AutoVideo_Aspect(int width, int height) {
    if (width <= 0 || height <= 0) return 1333;
    long long aspect = ((long long)width * 1000 + height / 2) / height;
    return aspect < 1333 ? 1333 : aspect > 4000 ? 4000 : (int)aspect;
}
static inline int AutoVideo_Scale(int width, int height, int aspect) {
    if (width <= 0 || height <= 0 || aspect <= 0) return 1;
    long long pictureHeight = (long long)width * 1000 / aspect;
    if (pictureHeight > height) pictureHeight = height;
    long long scale = (pictureHeight + 447) / 448;
    return scale < 1 ? 1 : scale > 8 ? 8 : (int)scale;
}
#endif
