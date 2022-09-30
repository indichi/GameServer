#include "CBaseParser.h"

CBaseParser::CBaseParser()
    : mText()
    , mData()
{
}

CBaseParser::~CBaseParser()
{
    delete mText;
}

void CBaseParser::LoadFile(const wchar_t* filename)
{
    long file_length;
    size_t read_length;

    FILE* stream;
    errno_t err = _wfopen_s(&stream, filename, L"r");

    if (err != 0 || stream == 0)
    {
        return;
    }

    fseek(stream, 0, SEEK_END);
    file_length = ftell(stream);

    mText = new wchar_t[file_length];
    fseek(stream, 0, SEEK_SET);

    read_length = fread_s(mText, file_length, sizeof(wchar_t), file_length, stream);
    mText[read_length] = L'\0';

    fclose(stream);
}

bool CBaseParser::GetData(const wchar_t* key, int* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            *out = _wtoi(i->value);
            return true;
        }
    }

    return false;
}

bool CBaseParser::GetData(const wchar_t* key, short* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            *out = (short)_wtoi(i->value);
            return true;
        }
    }

    return false;
}

bool CBaseParser::GetData(const wchar_t* key, unsigned short* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            *out = (unsigned short)_wtoi(i->value);
            return true;
        }
    }

    return false;
}

bool CBaseParser::GetData(const wchar_t* key, float* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            *out = _wtof(i->value);
            return true;
        }
    }

    return false;
}

bool CBaseParser::GetData(const wchar_t* key, wchar_t* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            wcscpy_s(out, DATA_WCHAR_LENGTH, i->value);
            return true;
        }
    }

    return false;
}

bool CBaseParser::GetData(const wchar_t* key, bool* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            if (wcscmp(i->value, L"true") == 0)
            {
                *out = true;
                return true;
            }
            else if (wcscmp(i->value, L"false") == 0)
            {
                *out = false;
                return true;
            }

            return false;
        }
    }

    return false;
}

bool CBaseParser::GetDataHex(const wchar_t* key, unsigned char* out)
{
    if (mData.empty())
    {
        return false;
    }

    if (out == nullptr)
    {
        return false;
    }

    for (auto i = mData.begin(); i != mData.end(); ++i)
    {
        if (wcscmp(key, i->key) == 0)
        {
            *out = (unsigned char)wcstol(i->value, NULL, 16);
            return true;
        }
    }

    return false;
}
