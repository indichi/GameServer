#ifndef CTEXTPARSER_H
#define CTEXTPARSER_H

#include "CBaseParser.h"
class CTextParser final : public CBaseParser
{
public:
    CTextParser();
    virtual ~CTextParser();

    virtual void LoadFile(const wchar_t* filename) override;
private:
    void Parse() override;
};

#endif /* CTEXTPARSER_H */