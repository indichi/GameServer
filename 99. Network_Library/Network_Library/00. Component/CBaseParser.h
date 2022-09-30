#ifndef IPARSER_H
#define IPARSER_H

#include <vector>
#include "../00. Component/define.h"

class CBaseParser
{
public:
    CBaseParser();
    virtual ~CBaseParser();

    virtual void LoadFile(const wchar_t* filename);
    virtual void Parse() = 0;

    bool GetData(const wchar_t* key, int* out);
    bool GetData(const wchar_t* key, short* out);
    bool GetData(const wchar_t* key, unsigned short* out);
    bool GetData(const wchar_t* key, float* out);
    bool GetData(const wchar_t* key, wchar_t* out);
    bool GetData(const wchar_t* key, bool* out);
    bool GetDataHex(const wchar_t* key, unsigned char* out);
private:
    enum {
        DATA_WCHAR_LENGTH = 64,
    };

protected:
    typedef struct {
        wchar_t    key[DATA_WCHAR_LENGTH];
        wchar_t    value[DATA_WCHAR_LENGTH];
    } data_t;

    wchar_t*                mText;
    std::vector<data_t>     mData;
};

#endif /* IPARSER_H */