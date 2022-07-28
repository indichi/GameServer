#include <algorithm>
#include "CAstar.h"

stTILE  g_Tile[TILE_Y_COUNT][TILE_X_COUNT];

bool CompareList(const stNODE* node1, const stNODE* node2)
{
    return node1->F < node2->F;
}

CAstar::CAstar()
    : mOpenList()
    , mCloseList()
    , mStartTile(nullptr)
    , mEndTile(nullptr)

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
}

CAstar::~CAstar()
{
    ReleaseList();
}

void CAstar::Find(HDC hdc)
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
            if (g_Tile[i][j].state == TILE_STATE::WENT || g_Tile[i][j].state == TILE_STATE::PATH || g_Tile[i][j].state == TILE_STATE::RESERVE)
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
    bool exist;
    int add_value;

    stROWCOL dir[8] =
    {
        { 0 , 1 },
        { 0 , -1 },
        { 1 , 0 },
        { -1 , 0 },
        { 1 , 1 },
        { -1 , 1 },
        { -1 , -1 },
        { 1 , -1 },
    };

    while (false == mOpenList.empty())
    {
        for (int i = 0; i < 8; i++)
        {
            exist = false;

            int row = curNode->row + dir[i].row;
            int col = curNode->col + dir[i].col;

            if (row < 0 || col < 0 || row >= TILE_X_COUNT || col >= TILE_Y_COUNT)
            {
                continue;
            }

            if (g_Tile[col][row].state == TILE_STATE::BLOCK)
            {
                continue;
            }

            // 대각선인지 확인하고 가중치 설정
            if (i > 3)
            {
                add_value = DIAGONAL_VALUE;
            }
            else
            {
                add_value = STRAIGHT_VALUE;
            }

            for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
            {
                if ((*iter)->row == row && (*iter)->col == col)
                {
                    exist = true;

                    if ((*iter)->G > curNode->G + add_value)
                    {
                        (*iter)->parent = curNode;
                        (*iter)->G = curNode->G + add_value;
                        (*iter)->F = (*iter)->G + (*iter)->H;
                    }
                    
                    break;
                }
            }

            for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
            {
                if ((*iter)->row == row && (*iter)->col == col)
                {
                    exist = true;
                    break;
                }
            }

            if (false == exist)
            {
                stNODE* newNode = new stNODE;
                newNode->parent = curNode;
                newNode->row = row;
                newNode->col = col;

                newNode->G = curNode->G + add_value;
                newNode->H = (abs(row - mEndTile->row) + abs(col - mEndTile->col)) * STRAIGHT_VALUE;
                newNode->F = newNode->G + newNode->H;

                mOpenList.push_back(newNode);
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

        // 랜더링 때문에 state설정
        // 가기로 예정된 길들
        for (auto iter = mOpenList.begin(); iter != mOpenList.end(); ++iter)
        {
            if (g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::START && g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::END)
            {
                g_Tile[(*iter)->col][(*iter)->row].state = TILE_STATE::RESERVE;
            }
        }

        // 갔던 길들
        for (auto iter = mCloseList.begin(); iter != mCloseList.end(); ++iter)
        {
            if (g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::START && g_Tile[(*iter)->col][(*iter)->row].state != TILE_STATE::END)
            {
                g_Tile[(*iter)->col][(*iter)->row].state = TILE_STATE::WENT;
            }
        }

        // 도착지 찾음
        if (curNode->H == 0)
        {
            // 실제 경로
            while (curNode->parent != nullptr)
            {
                if (g_Tile[curNode->col][curNode->row].state != TILE_STATE::START && g_Tile[curNode->col][curNode->row].state != TILE_STATE::END)
                {
                    g_Tile[curNode->col][curNode->row].state = TILE_STATE::PATH;
                }

                curNode = curNode->parent;
            }

            return;
        }

        Render(hdc);
    }
}

void CAstar::Render(HDC hdc)
{
    HBRUSH old;

    HBRUSH gray = (HBRUSH)GetStockObject(GRAY_BRUSH);
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);

    HBRUSH green = (HBRUSH)CreateSolidBrush(RGB(0, 255, 0));
    HBRUSH blue = (HBRUSH)CreateSolidBrush(RGB(0, 0, 255));
    HBRUSH red = (HBRUSH)CreateSolidBrush(RGB(255, 0, 0));
    HBRUSH yellow = (HBRUSH)CreateSolidBrush(RGB(255, 255, 0));

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
            case TILE_STATE::RESERVE:
            {
                old = (HBRUSH)SelectObject(hdc, blue);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
            }
            break;
            case TILE_STATE::WENT:
            {
                old = (HBRUSH)SelectObject(hdc, yellow);
                Rectangle(hdc, g_Tile[i][j].left, g_Tile[i][j].top, g_Tile[i][j].right, g_Tile[i][j].bottom);
                SelectObject(hdc, old);
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
        case TILE_STATE::RESERVE:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
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
        case TILE_STATE::RESERVE:
        {
            SetTextColor(hdc, RGB(255, 255, 255));
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
}

bool CAstar::GetTilePosition(int x, int y, int& out_row, int& out_col) const
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

void CAstar::ReleaseList()
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

void CAstar::SetTileState(int x, int y, TILE_STATE state)
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

void CAstar::InitTile()
{
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
