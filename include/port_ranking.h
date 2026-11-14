#ifndef PORT_RANKING_H
#define PORT_RANKING_H
#define PORT_RANKING_ROWS 3
typedef struct PortRankingEntry {
    int rank, points, character;
    char display_name[20];
} PortRankingEntry;
typedef struct PortRankingPage {
    int state; /* 0 loading, 1 ready, -1 unavailable */
    int offset, total, count;
    PortRankingEntry entries[PORT_RANKING_ROWS];
} PortRankingPage;
void Port_RankingRead(int offset, int refresh, PortRankingPage *out);
#endif
