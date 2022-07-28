#pragma once

#include <Windows.h>
#include <list>

// 8����
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
    BLOCK,          // ��� ����
    START,          // ��� ����
    END,            // ���� ����
    OPEN,           // ���� ����Ʈ ���
    CLOSE,          // Ŭ���� ����Ʈ ���
    WENT,           // �˻��� Ÿ��
    PATH,           // ���� ���

    LINE            // �극���� �˰����..
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
    int         row;        // tile �迭 ���� x
    int         col;        // tile �迭 ���� y
    stNODE*     parent;     // �θ� ���
    int         H;          // ���� ��� ��ġ���� ������������ �Ÿ�
    int         G;          // ��������κ��� ���� ��� ��ġ������ �Ÿ�
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
    void DrawLine(HDC hdc, int sX, int sY, int eX, int eY); // �극���� �˰������� �����׸��� ����..
    void InitTile();
private:
    bool CheckTile(int row, int col, DIR dir, bool isCreate); // Ÿ�� üũ �� �ڳ� �߰� �� ��� ����
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

