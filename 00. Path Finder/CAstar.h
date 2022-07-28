#pragma once

#include <Windows.h>
#include <list>

#define TILE_X_COUNT    (80)
#define TILE_Y_COUNT    (40)

#define TILE_WIDTH      (16)
#define TILE_HEIGHT     (16)

#define DIAGONAL_VALUE  (14)
#define STRAIGHT_VALUE  (10)

enum class TILE_STATE {
    NONE,
    BLOCK,          // 블락 상태
    START,          // 출발 지점
    END,            // 도착 지점
    RESERVE,        // 가야 할 길
    WENT,           // 이미 갔던 길
    PATH            // 실제 경로
};

struct stTILE {
    int         left;
    int         right;
    int         top;
    int         bottom;
    int         row;
    int         col;
    TILE_STATE  state;
};

struct stNODE {
    int         row;      // tile 배열 내의 x
    int         col;      // tile 배열 내의 y
    stNODE*     parent;
    int         H;      // 현재 노드 위치에서 목적지까지의 거리
    int         G;      // 출발점으로부터 현재 노드 위치까지의 거리
    int         F;      // H + G
};

struct stROWCOL {
    int         col;
    int         row;
};

class CAstar
{
public:
    CAstar();
    ~CAstar();

    void Find(HDC hdc);
    void Render(HDC hdc);

    void SetTileState(int x, int y, TILE_STATE state);
    void InitTile();
private:
    bool GetTilePosition(int x, int y, int& out_row, int& out_col) const;
    void ReleaseList();
    std::list<stNODE*>  mOpenList;
    std::list<stNODE*>  mCloseList;

    stTILE*     mStartTile;
    stTILE*     mEndTile;
};

