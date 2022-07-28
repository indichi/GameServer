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
    BLOCK,          // ��� ����
    START,          // ��� ����
    END,            // ���� ����
    RESERVE,        // ���� �� ��
    WENT,           // �̹� ���� ��
    PATH            // ���� ���
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
    int         row;      // tile �迭 ���� x
    int         col;      // tile �迭 ���� y
    stNODE*     parent;
    int         H;      // ���� ��� ��ġ���� ������������ �Ÿ�
    int         G;      // ��������κ��� ���� ��� ��ġ������ �Ÿ�
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

