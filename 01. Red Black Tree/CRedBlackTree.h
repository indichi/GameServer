#pragma once

#include <Windows.h>
#include <vector>

enum class NODE_COLOR {
    BLACK = 0,
    RED
};

struct stNODE {
    stNODE*     parent;
    stNODE*     left;
    stNODE*     right;

    NODE_COLOR  color;
    int         data;
};

class CRedBlackTree
{
public:
    CRedBlackTree();
    ~CRedBlackTree();

    bool Insert(int data);
    bool Delete(int data);
    /*bool Search(int data) const;*/

    void PrintPreorder(stNODE* node) const;    // 전위
    void PrintInorder(stNODE* node) const;     // 중위
    void PrintPostorder(stNODE* node) const;   // 후위

    void Test(stNODE* node, std::vector<int>& out_Data, int black = 0);

    stNODE* GetRootorNull() const { return mRoot; }
    int GetDepth() const { return mDepth; }
    void SetDepth(stNODE* node, bool isFirst = true);
    void Render(HDC hdc, stNODE* node, int x, int y, int depth) const;
private:
    void RotateLeft(stNODE* target);
    void RotateRight(stNODE* target);

    void BalancingInsert(stNODE* target);
    void BalancingDelete(stNODE* target);

    void DeleteAllData(stNODE* node);

    stNODE*     mRoot;
    stNODE      Nil;
    int         mDepth;
};
