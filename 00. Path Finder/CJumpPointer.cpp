#include <algorithm>
#include "CJumpPointer.h"
#include "macro.h"
#include "CLine.h"

stTILE  g_Tile[TILE_Y_COUNT][TILE_X_COUNT];

bool CompareList(const stNODE* node1, const stNODE* node2)
{
    return node1->F < node2->F;
}

CJumpPointer::CJumpPointer()
    : mOpenList()
    , mCloseList()
    , mStartTile(nullptr)
    , mEndTile(nullptr)
    , mDC(0)
    , mPallet()
    , mOldBrush(0)
    , mPalletIndex(0)

{
    for (int i = 0; i < TILE_Y_COUNT; i++)
    {
        for (int j = 0; j < TILE_X_COUNT; j++)
        {
            g_Tile[i][j].left = TILE_WIDTH * j;
            g_Tile[i][j].top = TILE_HEIGHT * i;

            g_Tile[i][j].right = g_Tile[i][j].left + TILE_WIDTH;
            g_Tile[i][j].bottom = g_Tile[i][j].top + TILE_HEIGHT;

            g_Tile[i][j].state = TILE_STATE::NONE;

            g_Tile[i][j].col = i;
            g_Tile[i][j].row = j;
        }
    }

    mPallet[0] = (HBRUSH)CreateSolidBrush(RGB(178, 102, 255));
    mPallet[1] = (HBRUSH)CreateSolidBrush(RGB(153, 255, 255));
    mPallet[2] = (HBRUSH)CreateSolidBrush(RGB(204, 255, 153));
    mPallet[3] = (HBRUSH)CreateSolidBrush(RGB(224, 224, 224));
    mPallet[4] = (HBRUSH)CreateSolidBrush(RGB(255, 204, 153));
    mPallet[5] = (HBRUSH)CreateSolidBrush(RGB(204, 229, 255));
    mPallet[6] = (HBRUSH)CreateSolidBrush(RGB(255, 255, 204));
}

CJumpPointer::~CJumpPointer()
{
    ReleaseList();

    if (mDC != 0)
    {
        SelectObject(mDC, mOldBrush);

        DeleteObject(mPallet[0]);
        DeleteObject(mPallet[1]);
        DeleteObject(mPallet[2]);
        DeleteObject(mPallet[3]);
        DeleteObject(mPallet[4]);
        DeleteObject(mPallet[5]);
        DeleteObject(mPallet[6]);
    }
}

void CJumpPointer::Find(HDC hdc, bool bCorrect)
{
    if (nullptr == mStartTile || nullptr == mEndTile)
    {
        return;
    }

    ReleaseList();

    // 랜더링 부분 초기화
    for (int i = 0; i < TILE_Y_COUNT; i++)
    {
        for (int j = 0; j < TILE_X_COUNT; j++)
        {
            if (false == (g_Tile[i][j].state == TILE_STATE::START || g_Tile[i][j].state == TILE_STATE::END || g_Tile[i][j].state == TILE_STATE::BLOCK))
            {
                g_Tile[i][j].state = TILE_STATE::NONE;
            }
        }
    }

    Render(hdc);

    // start node 설정
    stNODE* start = new stNODE;
    start->parent = nullptr;
    start->row = mStartTile->row;
    start->col = mStartTile->col;
    start->G = 0;
    start->H = abs(mStartTile->row - mEndTile->row) + abs(mStartTile->col - mEndTile->col);
    start->F = start->H + start->G;

    mOpenList.push_back(start);

    stNODE* curNode = start;
    DIR dir;

    int row;
    int col;

    stCHECK_SET check_set[2];

    // TODO 코너인지 확인하고 체크할 타일 방향 설정 (수직, 수평이면 최대 3개, 대각이면 최대 5개)


    while (mOpenList.empty() == false)
    {
        if (++mPalletIndex >= 7)
        {
            mPalletIndex = 0;
        }

        // start 노드 8방향 check
        if (nullptr == curNode->parent)
        {
            CheckTile(start->row, start->col, DIR::LL, true);       // LL
            CheckTile(start->row, start->col, DIR::RR, true);       // RR
            CheckTile(start->row, start->col, DIR::UU, true);       // UU
            CheckTile(start->row, start->col, DIR::DD, true);       // DD
            CheckTile(start->row, start->col, DIR::LU, true);       // LU
            CheckTile(start->row, start->col, DIR::LD, true);       // LD
            CheckTile(start->row, start->col, DIR::RU, true);       // RU
            CheckTile(start->row, start->col, DIR::RD, true);       // RD
        }
        else
        {
            // 부모기준으로 방향 구하기
            if (curNode->parent->row > row)
            {
                if (curNode->parent->col == col)
                {
                    dir = DIR::LL;
                }
                else if (curNode->parent->col > col)
                {
                    dir = DIR::LU;
                }
                else if (curNode->parent->col < col)
                {
                    dir = DIR::LD;
                }
            }
            else if (curNode->parent->row < row)
            {
                if (curNode->parent->col == col)
                {
                    dir = DIR::RR;
                }
                else if (curNode->parent->col > col)
                {
                    dir = DIR::RU;
                }
                else if (curNode->parent->col < col)
                {
                    dir = DIR::RD;
                }
            }
            else
            {
                if (curNode->parent->col > col)
                {
                    dir = DIR::UU;
                }
                else
                {
                    dir = DIR::DD;
                }
            }

            switch (dir)
            {
            case DIR::LL:
            {
                CheckTile(row, col, DIR::LL, true);

                check_set[0].my_wing_col = col - 1;
                check_set[0].my_wing_row = row;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row - 1;

                check_set[1].my_wing_col = col + 1;
                check_set[1].my_wing_row = row;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row - 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::LU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::LD, true);
                }
            }
            break;
            case DIR::RR:
            {
                CheckTile(row, col, DIR::RR, true);

                check_set[0].my_wing_col = col - 1;
                check_set[0].my_wing_row = row;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row + 1;

                check_set[1].my_wing_col = col + 1;
                check_set[1].my_wing_row = row;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row + 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::RU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::RD, true);
                }
            }
            break;
            case DIR::UU:
            {
                CheckTile(row, col, DIR::UU, true);

                check_set[0].my_wing_col = col;
                check_set[0].my_wing_row = row - 1;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row - 1;

                check_set[1].my_wing_col = col;
                check_set[1].my_wing_row = row + 1;
                check_set[1].front_wing_col = col - 1;
                check_set[1].front_wing_row = row + 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::LU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::RU, true);
                }
            }
            break;
            case DIR::DD:
            {
                CheckTile(row, col, DIR::DD, true);

                check_set[0].my_wing_col = col;
                check_set[0].my_wing_row = row - 1;
                check_set[0].front_wing_col = col + 1;
                check_set[0].front_wing_row = row - 1;

                check_set[1].my_wing_col = col;
                check_set[1].my_wing_row = row + 1;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row + 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::LD, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::RD, true);
                }
            }
            break;
            case DIR::LU:
            {
                CheckTile(row, col, DIR::LL, true);
                CheckTile(row, col, DIR::UU, true);
                CheckTile(row, col, DIR::LU, true);

                check_set[0].my_wing_col = col;
                check_set[0].my_wing_row = row + 1;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row + 1;

                check_set[1].my_wing_col = col + 1;
                check_set[1].my_wing_row = row;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row - 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::RU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::LD, true);
                }
            }
            break;
            case DIR::LD:
            {
                CheckTile(row, col, DIR::LL, true);
                CheckTile(row, col, DIR::DD, true);
                CheckTile(row, col, DIR::LD, true);

                check_set[0].my_wing_col = col - 1;
                check_set[0].my_wing_row = row;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row - 1;

                check_set[1].my_wing_col = col;
                check_set[1].my_wing_row = row + 1;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row + 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::LU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::RD, true);
                }
            }
            break;
            case DIR::RU:
            {
                CheckTile(row, col, DIR::RR, true);
                CheckTile(row, col, DIR::UU, true);
                CheckTile(row, col, DIR::RU, true);

                check_set[0].my_wing_col = col;
                check_set[0].my_wing_row = row - 1;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row - 1;

                check_set[1].my_wing_col = col + 1;
                check_set[1].my_wing_row = row;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row + 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::LU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::RD, true);
                }
            }
            break;
            case DIR::RD:
            {
                CheckTile(row, col, DIR::RR, true);
                CheckTile(row, col, DIR::DD, true);
                CheckTile(row, col, DIR::RD, true);

                check_set[0].my_wing_col = col - 1;
                check_set[0].my_wing_row = row;
                check_set[0].front_wing_col = col - 1;
                check_set[0].front_wing_row = row + 1;

                check_set[1].my_wing_col = col;
                check_set[1].my_wing_row = row - 1;
                check_set[1].front_wing_col = col + 1;
                check_set[1].front_wing_row = row - 1;

                if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
                {
                    CheckTile(row, col, DIR::RU, true);
                }

                if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
                {
                    CheckTile(row, col, DIR::LD, true);
                }
            }
            break;
            }
        }

        mCloseList.push_back(curNode);
        mOpenList.pop_front();

        if (mOpenList.empty() == true)
        {
            break;
        }

        mOpenList.sort(CompareList);
        curNode = mOpenList.front();

        row = curNode->row;
        col = curNode->col;

        // 랜더링 때문에 state설정
        // 가기로 예정된 길들
        for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
        {
            if (g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::START && g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::END)
            {
                g_Tile[(*iter)->col][(*iter)->row].state = TILE_STATE::OPEN;
            }
        }

        // 갔던 길들
        for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
        {
            if (g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::START && g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::END)
            {
                g_Tile[(*iter)->col][(*iter)->row].state = TILE_STATE::CLOSE;
            }
        }

        // 도착지 찾음
        if (curNode->H == 0)
        {
            stNODE* dest = curNode;

            // 실제 경로
            while (curNode->parent != nullptr)
            {
                if (g_Tile[curNode->col][curNode->row].state != TILE_STATE::START && g_Tile[curNode->col][curNode->row].state != TILE_STATE::END)
                {
                    g_Tile[curNode->col][curNode->row].state = TILE_STATE::PATH;
                }

                curNode = curNode->parent;
            }

            Render(hdc, dest);

            // TODO 브레젠험 잘 된건지 한번 확인하고
            // 도착지 보정 작업
            if (bCorrect)
            {
                curNode = dest;
                
                stNODE* cmpNode;

                while (true)
                {
                    if (curNode == nullptr || curNode->parent == nullptr)
                    {
                        break;
                    }

                    cmpNode = curNode->parent->parent;

                    if (cmpNode == nullptr)
                    {
                        break;
                    }

                    while (cmpNode != nullptr)
                    {
                        if (CorrectPath(curNode, cmpNode) == true)
                        {
                            curNode->parent = cmpNode;
                            cmpNode = cmpNode->parent;
                        }
                        else
                        {
                            curNode = curNode->parent;
                            break;
                        }
                    }
                }

                curNode = dest;

                HPEN red_pen = (HPEN)CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                HPEN old = (HPEN)SelectObject(hdc, red_pen);

                while (curNode->parent != nullptr)
                {
                    MoveToEx(hdc, TILE_CENTER_X(g_Tile[curNode->col][curNode->row].left), TILE_CENTER_X(g_Tile[curNode->col][curNode->row].top), NULL);
                    LineTo(hdc, TILE_CENTER_X(g_Tile[curNode->parent->col][curNode->parent->row].left), TILE_CENTER_X(g_Tile[curNode->parent->col][curNode->parent->row].top));

                    curNode = curNode->parent;
                }

                SelectObject(hdc, old);
                DeleteObject(red_pen);
            }

            return;
        }

        Render(hdc);
        Sleep(300);
    }
}

void CJumpPointer::Render(HDC hdc, stNODE* dest)
{
    HBRUSH old;
    HPEN old_pen;

    HBRUSH gray = (HBRUSH)GetStockObject(GRAY_BRUSH);
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);

    HBRUSH green = (HBRUSH)CreateSolidBrush(RGB(0, 255, 0));
    HBRUSH blue = (HBRUSH)CreateSolidBrush(RGB(0, 0, 255));
    HBRUSH red = (HBRUSH)CreateSolidBrush(RGB(255, 0, 0));
    HBRUSH yellow = (HBRUSH)CreateSolidBrush(RGB(255, 255, 0));
    HBRUSH light_gray = (HBRUSH)CreateSolidBrush(RGB(211, 211, 211));
    HBRUSH pink = (HBRUSH)CreateSolidBrush(RGB(255, 192, 203));
    HBRUSH magenta = (HBRUSH)CreateSolidBrush(RGB(255, 0, 255));

    HPEN blue_pen = (HPEN)CreatePen(PS_SOLID, 2, RGB(0, 0, 255));

    WCHAR buffer[32];

    for (int i = 0; i < TILE_Y_COUNT; i++)
    {
        for (int j = 0; j < TILE_X_COUNT; j++)
        {
            switch (g_Tile[i][j].state)
            {
            case TILE_STATE::NONE:
            {
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
            }
            break;
            case TILE_STATE::BLOCK:
            {
                old = (HBRUSH)SelectObject(hdc, gray);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::START:
            {
                old = (HBRUSH)SelectObject(hdc, red);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::END:
            {
                old = (HBRUSH)SelectObject(hdc, black);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::OPEN:
            {
                old = (HBRUSH)SelectObject(hdc, yellow);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::CLOSE:
            {
                old = (HBRUSH)SelectObject(hdc, pink);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::WENT:
            {
                /*old = (HBRUSH)SelectObject(hdc, light_gray);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);*/
            }
            break;
            case TILE_STATE::PATH:
            {
                old = (HBRUSH)SelectObject(hdc, green);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);

            }
            break;
            }
        }
    }

    if (dest != nullptr)
    {
        stNODE* target = dest;

        old_pen = (HPEN)SelectObject(hdc, blue_pen);

        while (target->parent != nullptr)
        {
            MoveToEx(hdc, TILE_CENTER_X(g_Tile[target->col][target->row].left), TILE_CENTER_Y(g_Tile[target->col][target->row].top), NULL);
            LineTo(hdc, TILE_CENTER_X(g_Tile[target->parent->col][target->parent->row].left), TILE_CENTER_Y(g_Tile[target->parent->col][target->parent->row].top));

            target = target->parent;
        }

        SelectObject(hdc, old_pen);
    }

#ifdef __SET_TEXT__
    SetBkMode(hdc, TRANSPARENT);

    for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
    {
        switch (g_Tile[(*iter)->col][(*iter)->row].state)
        {
        case TILE_STATE::BLOCK:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::START:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::END:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::OPEN:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::CLOSE:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::WENT:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::PATH:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        }

        swprintf_s(buffer, L"%d %d %d", (*iter)->H, (*iter)->G, (*iter)->F);
        TextOut(hdc, g_Tile[(*iter)->col][(*iter)->row].left + 5, g_Tile[(*iter)->col][(*iter)->row].top + 5, buffer, (int)wcslen(buffer));
    }

    for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
    {
        switch (g_Tile[(*iter)->col][(*iter)->row].state)
        {
        case TILE_STATE::BLOCK:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::START:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::END:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
        }
        break;
        case TILE_STATE::OPEN:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::CLOSE:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::WENT:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        case TILE_STATE::PATH:
        {
            SetTextColor(hdc, RGB(0, 0, 0));
        }
        break;
        }

        swprintf_s(buffer, L"%d %d %d", (*iter)->H, (*iter)->G, (*iter)->F);
        TextOut(hdc, g_Tile[(*iter)->col][(*iter)->row].left + 5, g_Tile[(*iter)->col][(*iter)->row].top + 5, buffer, (int)wcslen(buffer));
    }
#endif

    DeleteObject(green);
    DeleteObject(blue);
    DeleteObject(red);
    DeleteObject(yellow);
    DeleteObject(light_gray);
    DeleteObject(pink);
    DeleteObject(magenta);
    DeleteObject(blue_pen);
}

bool CJumpPointer::GetTilePosition(int x, int y, int& out_row, int& out_col) const
{
    if (g_Tile[0][0].left < 0 || g_Tile[0][0].top < 0 || x > g_Tile[TILE_Y_COUNT - 1][TILE_X_COUNT - 1].right || y > g_Tile[TILE_Y_COUNT - 1][TILE_X_COUNT - 1].bottom)
    {
        return false;
    }

    for (int i = 0; i < TILE_Y_COUNT; i++)
    {
        for (int j = 0; j < TILE_X_COUNT; j++)
        {
            if (g_Tile[i][j].left <= x && g_Tile[i][j].right >= x && g_Tile[i][j].top <= y && g_Tile[i][j].bottom >= y)
            {
                out_row = j;
                out_col = i;

                return true;
            }
        }
    }

    return false;
}

void CJumpPointer::DrawLine(HDC hdc, int sX, int sY, int eX, int eY)
{
    CLine liner;
    std::list<std::pair<int, int>> list;
    stTILE tile;
    int row;
    int col;

    // 빈 칸으로 초기화
    for (int i = 0; i < TILE_X_COUNT; i++)
    {
        for (int j = 0; j < TILE_Y_COUNT; j++)
        {
            tile = g_Tile[j][i];
            Rectangle(hdc, tile.left, tile.top, tile.right, tile.bottom);
        }
    }

    HBRUSH magenta = (HBRUSH)CreateSolidBrush(RGB(255, 0, 255));
    HBRUSH old = (HBRUSH)SelectObject(hdc, magenta);

    int eRow;
    int eCol;
    GetTilePosition(sX, sY, row, col);
    GetTilePosition(eX, eY, eRow, eCol);
    liner.DrawLine(row, col, eRow, eCol, list);

    for (auto i = list.begin(); i != list.end(); ++i)
    {
        tile = g_Tile[i->second][i->first];
        Rectangle(hdc, tile.left, tile.top, tile.right, tile.bottom);
    }

    int lineStart_X;
    int lineStart_Y;

    int lineEnd_X;
    int lineEnd_Y;

    lineStart_X = TILE_CENTER_X(g_Tile[col][row].left);
    lineEnd_X = TILE_CENTER_X(g_Tile[eCol][eRow].left);
    lineStart_Y = TILE_CENTER_Y(g_Tile[col][row].top);
    lineEnd_Y = TILE_CENTER_Y(g_Tile[eCol][eRow].top);

    MoveToEx(hdc, lineStart_X, lineStart_Y, NULL);
    LineTo(hdc, lineEnd_X, lineEnd_Y);
    
    SelectObject(hdc, old);
    DeleteObject(magenta);
}

void CJumpPointer::InitTile()
{
    // 랜더링 부분 초기화
    for (int i = 0; i < TILE_Y_COUNT; i++)
    {
        for (int j = 0; j < TILE_X_COUNT; j++)
        {
            g_Tile[i][j].state = TILE_STATE::NONE;
        }
    }

    mStartTile = nullptr;
    mEndTile = nullptr;
}

bool CJumpPointer::CheckTile(int row, int col, DIR dir, bool isCreate)
{
    int total_G = dir <= DIR::DD ? STRAIGHT_VALUE : DIAGONAL_VALUE;

    total_G += mOpenList.front()->G;

    stCHECK_SET check_set[2];

    switch (dir)
    {
    case DIR::LL:
    {
        while (row >= 0)
        {
            --row;

            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            check_set[0].my_wing_col = col - 1;
            check_set[0].my_wing_row = row;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row - 1;

            check_set[1].my_wing_col = col + 1;
            check_set[1].my_wing_row = row;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row - 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {

                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += STRAIGHT_VALUE;
        }
    }
    break;
    case DIR::RR:
    {
        while (row < TILE_X_COUNT)
        {
            ++row;

            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            check_set[0].my_wing_col = col - 1;
            check_set[0].my_wing_row = row;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row + 1;

            check_set[1].my_wing_col = col + 1;
            check_set[1].my_wing_row = row;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row + 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += STRAIGHT_VALUE;
        }
    }
    break;
    case DIR::UU:
    {
        while (col >= 0)
        {
            --col;
            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }


            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            check_set[0].my_wing_col = col;
            check_set[0].my_wing_row = row - 1;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row - 1;

            check_set[1].my_wing_col = col;
            check_set[1].my_wing_row = row + 1;
            check_set[1].front_wing_col = col - 1;
            check_set[1].front_wing_row = row + 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += STRAIGHT_VALUE;
        }
    }
    break;
    case DIR::DD:
    {
        while (col < TILE_Y_COUNT)
        {
            ++col;
            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            check_set[0].my_wing_col = col;
            check_set[0].my_wing_row = row - 1;
            check_set[0].front_wing_col = col + 1;
            check_set[0].front_wing_row = row - 1;

            check_set[1].my_wing_col = col;
            check_set[1].my_wing_row = row + 1;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row + 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += STRAIGHT_VALUE;
        }
    }
    break;
    case DIR::LU:
    {
        while (col >= 0 && row >= 0)
        {
            --row;
            --col;
            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            if (true == CheckTile(row, col, DIR::LL, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (true == CheckTile(row, col, DIR::UU, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            check_set[0].my_wing_col = col;
            check_set[0].my_wing_row = row + 1;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row + 1;

            check_set[1].my_wing_col = col + 1;
            check_set[1].my_wing_row = row;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row - 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }
            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += DIAGONAL_VALUE;
        }
    }
    break;
    case DIR::LD:
    {
        while (col < TILE_Y_COUNT && row >= 0)
        {
            --row;
            ++col;
            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            if (true == CheckTile(row, col, DIR::LL, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (true == CheckTile(row, col, DIR::DD, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            check_set[0].my_wing_col = col - 1;
            check_set[0].my_wing_row = row;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row - 1;

            check_set[1].my_wing_col = col;
            check_set[1].my_wing_row = row + 1;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row + 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += DIAGONAL_VALUE;
        }
    }
    break;
    case DIR::RU:
    {
        while (col >= 0 && row < TILE_X_COUNT)
        {
            ++row;
            --col;
            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }


            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            if (true == CheckTile(row, col, DIR::RR, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (true == CheckTile(row, col, DIR::UU, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            check_set[0].my_wing_col = col;
            check_set[0].my_wing_row = row - 1;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row - 1;

            check_set[1].my_wing_col = col + 1;
            check_set[1].my_wing_row = row;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row + 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += DIAGONAL_VALUE;
        }
    }
    break;
    case DIR::RD:
    {
        while (col < TILE_Y_COUNT && row < TILE_X_COUNT)
        {
            ++row;
            ++col;
            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                return false;
            }

            if (g_Tile[col][row].state == TILE_STATE::END)
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            g_Tile[col][row].state = TILE_STATE::WENT;
            SetWentTileColor(mDC, row, col);
            //Render(mDC);

            if (true == CheckTile(row, col, DIR::RR, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (true == CheckTile(row, col, DIR::DD, false))
            {
                if (false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            check_set[0].my_wing_col = col - 1;
            check_set[0].my_wing_row = row;
            check_set[0].front_wing_col = col - 1;
            check_set[0].front_wing_row = row + 1;

            check_set[1].my_wing_col = col;
            check_set[1].my_wing_row = row - 1;
            check_set[1].front_wing_col = col + 1;
            check_set[1].front_wing_row = row - 1;

            if (false == OUT_OF_RANGE(check_set[0]) && true == IS_CORNER(check_set[0]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            if (false == OUT_OF_RANGE(check_set[1]) && true == IS_CORNER(check_set[1]))
            {
                if (true == isCreate && false == IsExist(row, col))
                {
                    stNODE* newNode = new stNODE;
                    newNode->parent = mOpenList.front();
                    newNode->row = row;
                    newNode->col = col;
                    newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                    newNode->G = total_G;
                    newNode->F = newNode->H + newNode->G;

                    mOpenList.push_back(newNode);
                }

                return true;
            }

            total_G += DIAGONAL_VALUE;
        }
    }
    break;
    }

    return false;
}

bool CJumpPointer::IsExist(int row, int col)
{
    stNODE* cmpNode = mOpenList.front();

    for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
    {
        if ((*iter)->row == row && (*iter)->col == col)
        {
            if ((*iter)->G > cmpNode->G)
            {
                (*iter)->parent = cmpNode;
                (*iter)->G = cmpNode->G;
                (*iter)->F = (*iter)->G + (*iter)->H;
            }

            return true;
        }
    }

    for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
    {
        if ((*iter)->row == row && (*iter)->col == col)
        {
            return true;
        }
    }

    return false;
}

void CJumpPointer::ReleaseList()
{
    for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
    {
        delete* iter;
    }

    for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
    {
        delete* iter;
    }

    mOpenList.clear();
    mCloseList.clear();
}

bool CJumpPointer::CorrectPath(stNODE* start, stNODE* dest)
{
    if (nullptr == start || nullptr == dest)
    {
        return false;
    }

    std::list<std::pair<int, int>> list;
    CLine liner;
    int sX = TILE_CENTER_X(g_Tile[start->col][start->row].left);
    int sY = TILE_CENTER_Y(g_Tile[start->col][start->row].top);

    int eX = TILE_CENTER_X(g_Tile[dest->col][dest->row].left);
    int eY = TILE_CENTER_Y(g_Tile[dest->col][dest->row].top);
    liner.DrawLine(sX, sY, eX, eY, list);

    int row;
    int col;

    for (auto i = list.begin(); i != list.end(); ++i)
    {
        GetTilePosition(i->first, i->second, row, col);

        if (g_Tile[col][row].state == TILE_STATE::BLOCK)
        {
            return false;
        }
    }

    return true;
}

void CJumpPointer::SetWentTileColor(HDC hdc, int row, int col)
{
    mOldBrush = (HBRUSH)SelectObject(hdc, mPallet[mPalletIndex]);
    Rectangle(hdc, g_Tile[col][row].left, g_Tile[col][row].top, g_Tile[col][row].right, g_Tile[col][row].bottom);
    SelectObject(hdc, mOldBrush);
}

void CJumpPointer::SetTileState(int x, int y, TILE_STATE state)
{
    int row;
    int col;

    if (false == GetTilePosition(x, y, row, col))
    {
        return;
    }

    g_Tile[col][row].state = state;

    if (TILE_STATE::START == state)
    {
        if (mEndTile == &g_Tile[col][row])
        {
            mEndTile = nullptr;
        }

        if (mStartTile != nullptr && mStartTile != &g_Tile[col][row])
        {
            mStartTile->state = TILE_STATE::NONE;
        }

        mStartTile = &g_Tile[col][row];
    }
    else if (TILE_STATE::END == state)
    {
        if (mStartTile == &g_Tile[col][row])
        {
            mStartTile = nullptr;
        }

        if (mEndTile != nullptr && mEndTile != &g_Tile[col][row])
        {
            mEndTile->state = TILE_STATE::NONE;
        }

        mEndTile = &g_Tile[col][row];
    }
}
