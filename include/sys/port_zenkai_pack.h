#ifndef SYS_PORT_ZENKAI_PACK_H
#define SYS_PORT_ZENKAI_PACK_H
/* Reserved within the 0x4000-byte main save; outside native fields and the
 * legacy Options block at 0x3F00. No pointers or settings-companion ownership. */
#define PORT_ZENKAI_PACK_OFFSET 0x3EF0
#define PORT_ZENKAI_PACK_EARNED_ZENI 1
#define PORT_ZENKAI_PACK_DISABLED 2
static int Port_ZenkaiPackFlags(const void *save) {
    const unsigned char *p;
    if (!save) return PORT_ZENKAI_PACK_EARNED_ZENI;
    p = (const unsigned char *)save + PORT_ZENKAI_PACK_OFFSET;
    /* Old saves cannot prove that earned Zeni was never spent. Keep their
     * pack enabled and locked rather than permitting a zero-balance bypass. */
    if (p[0]!='S' || p[1]!='P' || p[2]!='K' || p[3]!=1)
        return PORT_ZENKAI_PACK_EARNED_ZENI;
    return p[4];
}
static void Port_ZenkaiPackWriteFlags(void *save, int flags) {
    unsigned char *p = (unsigned char *)save + PORT_ZENKAI_PACK_OFFSET;
    p[0]='S';p[1]='P';p[2]='K';p[3]=1;p[4]=(unsigned char)flags;
    p[5]=p[6]=p[7]=0;
}
#endif
