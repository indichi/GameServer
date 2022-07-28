#pragma once

#include <Windows.h>
#include <list>

// 8방향
enum class DIR {
    LL,
    RR,
    UU,
    DD,
    LU,
    LD,
    RU,
    RD,
    END
};

enum class TILE_STATE {
    NONE,
    BLOCK,          // 블락 상태
    START,          // 출발 지점
    END,            // 도착 지점
    OPEN,           // 오픈 리스트 노드
    CLOSE,          // 클로즈 리스트 노드
    WENT,           // 검사한 타일
    PATH,           // 실제 경로

    LINE            // 브레젠험 알고리즘용..
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
    int         row;        // tile 배열 내의 x
    int         col;        // tile 배열 내의 y
    stNODE*     parent;     // 부모 노드
    int         H;          // 현재 노드 위치에서 목적지까지의 거리
    int         G;          // 출발점으로부터 현재 노드 위치까지의 거리
    int         F;          // H + G
};

struct stCHECK_SET {
    int     my_wing_row;
    int     my_wing_col;

    int     front_wing_row;
    int     front_wing_col;
};


class CJumpPointer
{
public:
    CJumpPointer();
    ~CJumpPointer();

    void Find(HDC hdc, bool bCorrect);
    void Render(HDC hdc, stNODE* dest = nullptr);
    void SetTileState(int x, int y, TILE_STATE state);
    void SetDC(HDC dc) { mDC = dc; }

    bool GetTilePosition(int x, int y, int& out_row, int& out_col) const;
    void DrawLine(HDC hdc, int sX, int sY, int eX, int eY); // 브레젠험 알고리즘으로 직선그리기 전용..
    void InitTile();
private:
    bool CheckTile(int row, int col, DIR dir, bool isCreate); // 타일 체크 및 코너 발견 시 노드 생성
    bool IsExist(int row, int col);
    void ReleaseList();
    bool CorrectPath(stNODE* start, stNODE* dest);
    void SetWentTileColor(HDC hdc, int row, int col);

    std::list<stNODE*>  mOpenList;
    std::list<stNODE*>  mCloseList;

    stTILE*     mStartTile;
    stTILE*     mEndTile;
    HDC         mDC;
    HBRUSH      mPallet[7];
    HBRUSH      mOldBrush;
    int         mPalletIndex;
};

