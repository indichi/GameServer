#include <iostream>
#include "CRedBlackTree.h"

#define NODE_X_DISTANCE   (50)
#define NODE_Y_DISTANCE   (100)

CRedBlackTree::CRedBlackTree()
    : mRoot(nullptr)
    , Nil{ nullptr, nullptr, nullptr, NODE_COLOR::BLACK, 0 }
    , mDepth(0)
{
}

CRedBlackTree::~CRedBlackTree()
{
    DeleteAllData(mRoot);
}

bool CRedBlackTree::Insert(int data)
{
    if (nullptr == mRoot)
    {
        mRoot = new stNODE;
        mRoot->color = NODE_COLOR::BLACK;
        mRoot->data = data;
        mRoot->left = &Nil;
        mRoot->right = &Nil;
        mRoot->parent = nullptr;
    }
    else
    {
        stNODE* node = mRoot;

        while (true)
        {
            // 중복키 x
            if (data == node->data)
            {
                return false;
            }
            // 작으면 left
            else if (data < node->data)
            {
                if (&Nil == node->left)
                {
                    node->left = new stNODE;
                    node->left->color = NODE_COLOR::RED;
                    node->left->data = data;
                    node->left->left = &Nil;
                    node->left->right = &Nil;
                    node->left->parent = node;

                    node = node->left;
                    break;
                }

                node = node->left;
            }
            // 크면 right
            else
            {
                if (&Nil == node->right)
                {
                    node->right = new stNODE;
                    node->right->color = NODE_COLOR::RED;
                    node->right->data = data;
                    node->right->left = &Nil;
                    node->right->right = &Nil;
                    node->right->parent = node;

                    node = node->right;
                    break;
                }

                node = node->right;
            }
        }

        BalancingInsert(node);
    }

    SetDepth(mRoot);

    return true;
}

bool CRedBlackTree::Delete(int data)
{
    if (nullptr == mRoot)
    {
        return false;
    }

    stNODE* node = mRoot;

    // 삭제 노드 찾기
    while (node != &Nil)
    {
        if (data == node->data)
        {
            break;
        }
        else if (data < node->data)
        {
            node = node->left;
        }
        else
        {
            node = node->right;
        }
    }

    if (&Nil == node)
    {
        return false;
    }

    // 삭제 노드 자식이 두개라면 -> 대체할 노드 찾아서 데이터 교체
    if (&Nil != node->left && &Nil != node->right)
    {
        stNODE* change = node->left;

        while (change->right != &Nil)
        {
            change = change->right;
        }

        node->data = change->data;

        node = change;
    }

    stNODE* kid = node->left != &Nil ? node->left : node->right;

    if (node == mRoot)
    {
        if (&Nil == kid)
        {
            mRoot = nullptr;
        }
        else
        {
            kid->parent = nullptr;
            kid->color = NODE_COLOR::BLACK;
            mRoot = kid;
        }

        delete node;
        return true;
    }

    if (NODE_COLOR::BLACK == node->color)
    {
        BalancingDelete(node);
    }

    if (node->parent->left == node)
    {
        node->parent->left = kid;
    }
    else
    {
        node->parent->right = kid;
    }

    if (kid != &Nil)
    {
        kid->parent = node->parent;
    }

    delete node;

    SetDepth(mRoot);

    return true;
}

void CRedBlackTree::RotateLeft(stNODE* target)
{
    if (&Nil == target || &Nil == target->right)
    {
        return;
    }

    stNODE* kid = target->right;

    if (target->parent != nullptr)
    {
        if (target->parent->left == target)
        {
            target->parent->left = kid;
        }
        else
        {
            target->parent->right = kid;
        }
    }
    else
    {
        mRoot = kid;
    }

    kid->parent = target->parent;

    if (&Nil != kid->left)
    {
        kid->left->parent = target;
    }

    target->right = kid->left;

    target->parent = kid;
    kid->left = target;
}

void CRedBlackTree::RotateRight(stNODE* target)
{
    if (&Nil == target || &Nil == target->left)
    {
        return;
    }

    stNODE* kid = target->left;

    if (nullptr != target->parent)
    {
        if (target->parent->left == target)
        {
            target->parent->left = kid;
        }
        else
        {
            target->parent->right = kid;
        }
    }
    else
    {
        mRoot = kid;
    }

    kid->parent = target->parent;

    if (&Nil != kid->right)
    {
        kid->right->parent = target;
    }

    target->left = kid->right;

    target->parent = kid;
    kid->right = target;
}

void CRedBlackTree::BalancingInsert(stNODE* target)
{
    if (&Nil == target)
    {
        return;
    }

    stNODE* parent;
    stNODE* grand;
    stNODE* uncle;

    while (true)
    {
        if (nullptr == target->parent)
        {
            target->color = NODE_COLOR::BLACK;
            return;
        }

        parent = target->parent;
        grand = parent->parent;

        if (NODE_COLOR::BLACK == parent->color)
        {
            return;
        }

        // 부모가 할아버지의 left, right인지에 따른 분기
        if (grand->left == parent)
        {
            uncle = grand->right;

            if (NODE_COLOR::RED == parent->color)
            {
                if (NODE_COLOR::RED == uncle->color)
                {
                    parent->color = NODE_COLOR::BLACK;
                    uncle->color = NODE_COLOR::BLACK;
                    grand->color = NODE_COLOR::RED;

                    target = grand;
                }
                // 삼촌 블랙
                else if (NODE_COLOR::BLACK == uncle->color)
                {
                    // 내가 오른 레드
                    if (parent->right == target)
                    {
                        target = parent;
                        RotateLeft(target);
                    }
                    else
                    {
                        parent->color = NODE_COLOR::BLACK;
                        grand->color = NODE_COLOR::RED;
                        RotateRight(grand);

                        return;
                    }
                }
            }
        }
        else
        {
            uncle = grand->left;

            if (NODE_COLOR::RED == parent->color)
            {
                if (NODE_COLOR::RED == uncle->color)
                {
                    parent->color = NODE_COLOR::BLACK;
                    uncle->color = NODE_COLOR::BLACK;
                    grand->color = NODE_COLOR::RED;

                    target = grand;
                }
                // 삼촌 블랙
                else if (NODE_COLOR::BLACK == uncle->color)
                {
                    // 내가 왼 레드
                    if (parent->left == target)
                    {
                        target = parent;
                        RotateRight(target);
                    }
                    else
                    {
                        parent->color = NODE_COLOR::BLACK;
                        grand->color = NODE_COLOR::RED;
                        RotateLeft(grand);

                        return;
                    }
                }
            }
        }
    }
}

void CRedBlackTree::BalancingDelete(stNODE* target)
{
    stNODE* kid = target->left != &Nil ? target->left : target->right;
    stNODE* bro;

    while (true)
    {
        if (kid->color == NODE_COLOR::RED)
        {
            kid->color = NODE_COLOR::BLACK;
            return;
        }

        // 부모로부터 left, right 분기
        if (target->parent->left == target)
        {
            bro = target->parent->right;

            if (NODE_COLOR::RED == bro->color)
            {
                bro->color = NODE_COLOR::BLACK;
                target->parent->color = NODE_COLOR::RED;
                RotateLeft(target->parent);
            }
            else
            {
                if (NODE_COLOR::BLACK == bro->left->color && NODE_COLOR::BLACK == bro->right->color)
                {
                    bro->color = NODE_COLOR::RED;

                    if (target->parent == mRoot)
                    {
                        return;
                    }

                    kid = target->parent;
                    target = target->parent;
                }
                else if (NODE_COLOR::RED == bro->left->color && NODE_COLOR::BLACK == bro->right->color)
                {
                    bro->left->color = NODE_COLOR::BLACK;
                    bro->color = NODE_COLOR::RED;
                    RotateRight(bro);
                }
                else if (NODE_COLOR::RED == bro->right->color)
                {
                    bro->color = bro->parent->color;
                    target->parent->color = NODE_COLOR::BLACK;
                    bro->right->color = NODE_COLOR::BLACK;
                    RotateLeft(target->parent);
                    return;
                }
            }
        }
        else
        {
            bro = target->parent->left;

            if (NODE_COLOR::RED == bro->color)
            {
                bro->color = NODE_COLOR::BLACK;
                target->parent->color = NODE_COLOR::RED;
                RotateRight(target->parent);
            }
            else
            {
                if (NODE_COLOR::BLACK == bro->left->color && NODE_COLOR::BLACK == bro->right->color)
                {
                    bro->color = NODE_COLOR::RED;

                    if (target->parent == mRoot)
                    {
                        return;
                    }

                    kid = target->parent;
                    target = target->parent;
                }
                else if (NODE_COLOR::RED == bro->right->color && NODE_COLOR::BLACK == bro->left->color)
                {
                    bro->right->color = NODE_COLOR::BLACK;
                    bro->color = NODE_COLOR::RED;
                    RotateLeft(bro);
                }
                else if (NODE_COLOR::RED == bro->left->color)
                {
                    bro->color = bro->parent->color;
                    target->parent->color = NODE_COLOR::BLACK;
                    bro->left->color = NODE_COLOR::BLACK;
                    RotateRight(target->parent);
                    return;
                }
            }
        }
    }

}

void CRedBlackTree::DeleteAllData(stNODE* node)
{
    if (nullptr == node)
    {
        return;
    }

    if (&Nil != node->left)
    {
        DeleteAllData(node->left);
    }

    if (&Nil != node->right)
    {
        DeleteAllData(node->right);
    }

    delete node;
}

void CRedBlackTree::Test(stNODE* node, std::vector<int>& out_Data, int black)
{
    static bool isFirst = true;
    static int blackCount = 0;

    if (nullptr == node)
    {
        return;
    }

    if (mRoot == node)
    {
        // 루트노드 black확인
        if (mRoot->color != NODE_COLOR::BLACK)
        {
            FILE* stream;
            _wfopen_s(&stream, L"Test_Log.txt", L"a");

            if (stream != 0)
            {
                fwprintf_s(stream, L"root node is not black\n");
                fclose(stream);
            }
        }
    }
    else
    {
        //// leaf 노드 black확인
        //if (node->left == &Nil && node->right == &Nil)
        //{
        //    if (node->color != NODE_COLOR::BLACK)
        //    {
        //        FILE* stream;
        //        _wfopen_s(&stream, L"Test_Log.txt", L"a");

        //        if (stream != 0)
        //        {
        //            fwprintf_s(stream, L"leaf node is not black\n");
        //            fclose(stream);
        //        }
        //    }
        //}

        // 노드가 red일때 부모 black, 자식 black 확인
        if (node->color == NODE_COLOR::RED)
        {
            if (node->parent->color != NODE_COLOR::BLACK)
            {
                FILE* stream;
                _wfopen_s(&stream, L"Test_Log.txt", L"a");

                if (stream != 0)
                {
                    fwprintf_s(stream, L"red node parent is not black\n");
                    fclose(stream);
                }
            }

            if (node->left->color != NODE_COLOR::BLACK)
            {
                FILE* stream;
                _wfopen_s(&stream, L"Test_Log.txt", L"a");

                if (stream != 0)
                {
                    fwprintf_s(stream, L"red node left child is not black\n");
                    fclose(stream);
                }
            }

            if (node->right->color != NODE_COLOR::BLACK)
            {
                FILE* stream;
                _wfopen_s(&stream, L"Test_Log.txt", L"a");

                if (stream != 0)
                {
                    fwprintf_s(stream, L"red node right child is not black\n");
                    fclose(stream);
                }
            }
        }
    }

    if (node != mRoot && node->color == NODE_COLOR::BLACK)
    {
        ++black;
    }

    if (&Nil != node->left)
    {
        Test(node->left, out_Data, black);
    }

    out_Data.push_back(node->data);

    if (&Nil != node->right)
    {
        Test(node->right, out_Data, black);
    }

    // leaf 노드일 때 경로의 black 총 개수 확인
    if (node->left == &Nil && node->right == &Nil)
    {
        if (isFirst)
        {
            blackCount = black;
            isFirst = false;
        }

        if (blackCount != black)
        {
            FILE* stream;
            _wfopen_s(&stream, L"Test_Log.txt", L"a");

            if (stream != 0)
            {
                fwprintf_s(stream, L"leaf node 까지 경로의 총 black 총 갯수 다름\n");
                fclose(stream);
            }
        }
    }

    if (node == mRoot)
    {
        isFirst = true;
    }
}

void CRedBlackTree::SetDepth(stNODE* node, bool isFirst)
{
    static int left = 0;
    static int right = 0;

    if (nullptr == node)
    {
        mDepth = 0;
        return;
    }

    if (&Nil == node)
    {
        return;
    }

    if (&Nil != node->left)
    {
        ++left;
        SetDepth(node->left, false);
    }

    if (&Nil != node->right)
    {
        ++right;
        SetDepth(node->right, false);
    }

    if (isFirst)
    {
        mDepth = left > right ? left : right;

        left = 0;
        right = 0;
    }
}

void CRedBlackTree::Render(HDC hdc, stNODE* node, int x, int y, int depth) const
{
    if (nullptr == node)
    {
        return;
    }

    int nextX;
    int nextY;

    if (&Nil != node->left)
    {
        if (mDepth < 3)
        {
            nextX = x - (int)(pow(2, depth) / 2 * (NODE_X_DISTANCE / 2));
            nextY = y + NODE_Y_DISTANCE;
        }
        else
        {
            int distance = NODE_X_DISTANCE - (mDepth * 10);

            if (distance < 1)
            {
                distance = 10;
            }

            nextX = x - (int)(pow(2, depth) / 2 * (distance / 2));
            nextY = y + NODE_Y_DISTANCE + (mDepth * 10);
        }

        MoveToEx(hdc, x, y, NULL);
        LineTo(hdc, nextX, nextY);
        Render(hdc, node->left, nextX, nextY, depth - 1);
    }

    WCHAR str[5];
    wsprintf(str, L"%d", node->data);

    if (&Nil != node->right)
    {
        if (mDepth < 3)
        {
            nextX = x + (int)(pow(2, depth) / 2 * (NODE_X_DISTANCE / 2));
            nextY = y + NODE_Y_DISTANCE;
        }
        else
        {
            int distance = NODE_X_DISTANCE - (mDepth * 10);

            if (distance < 1)
            {
                distance = 10;
            }

            nextX = x + (int)(pow(2, depth) / 2 * (distance / 2));
            nextY = y + NODE_Y_DISTANCE + (mDepth * 10);
        }

        MoveToEx(hdc, x, y, NULL);
        LineTo(hdc, nextX, nextY);
        Render(hdc, node->right, nextX, nextY, depth - 1);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HBRUSH brush;
    HBRUSH old;

    if (node->color == NODE_COLOR::RED)
    {
        brush = (HBRUSH)CreateSolidBrush(RGB(255, 0, 0));
    }
    else
    {
        brush = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
    }

    old = (HBRUSH)SelectObject(hdc, brush);

    Ellipse(hdc, x - 20, y - 20, x + 20, y + 20);
    TextOut(hdc, x - 12, y - 10, str, (int)wcslen(str));

    DeleteObject(brush);
    SelectObject(hdc, old);
}