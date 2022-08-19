#include "CTextParser.h"

CTextParser::CTextParser()
    : CBaseParser()
{
}

CTextParser::~CTextParser()
{
}

void CTextParser::LoadFile(const wchar_t* filename)
{
    CBaseParser::LoadFile(filename);
    Parse();
}

void CTextParser::Parse()
{
    size_t buffer_index;

    for (size_t i = 0; mText[i] != NULL_CHAR; ++i)
    {
        // { (LEFTBRACE) 영역 안에 들어 왔다면 (데이터가 유효한 영역)
        if (mText[i] == LEFTBRACE)
        {
            ++i;
            while (mText[i] != RIGHTBRACE)
            {
                // 주석 판별
                if (mText[i] == SLASH)
                {
                    if (mText[i + 1] == SLASH)
                    {
                        i += 2;

                        while (mText[i] != NULL_CHAR)
                        {
                            if (mText[i] == CR || mText[i] == LF)
                            {
                                ++i;
                                break;
                            }

                            ++i;
                        }
                    }
                    else if (mText[i + 1] == ASTERISK)
                    {
                        i += 2;

                        while (mText[i] != NULL_CHAR)
                        {
                            if (mText[i] == SLASH)
                            {
                                ++i;
                                break;
                            }

                            ++i;
                        }
                    }
                }

                // key, value 판별
                if (mText[i] != TAP && mText[i] != SPACE && mText[i] != EQUAL && mText[i] != CR && mText[i] != LF && mText[i] != NULL_CHAR)
                {
                    data_t data;

                    /************ key ************/
                    buffer_index = 0;

                    while (mText[i] != NULL_CHAR)
                    {
                        if (mText[i] == DOUBLEQUOTES)
                        {
                            ++i;

                            while (mText[i] != DOUBLEQUOTES)
                            {
                                data.key[buffer_index++] = mText[i];
                                ++i;

                                if (mText[i] == DOUBLEQUOTES)
                                {
                                    ++i;
                                    break;
                                }
                            }

                            break;
                        }
                        else
                        {
                            if (mText[i] == TAP || mText[i] == SPACE || mText[i] == EQUAL || mText[i] == CR || mText[i] == LF || mText[i] == NULL_CHAR)
                            {
                                break;
                            }

                            data.key[buffer_index++] = mText[i];
                            ++i;
                        }
                    }

                    data.key[buffer_index] = NULL_CHAR;

                    /********* value까지 ++ *********/
                    while (mText[i] == TAP || mText[i] == SPACE || mText[i] == EQUAL || mText[i] == CR || mText[i] == LF)
                    {
                        ++i;

                        if (mText[i] == NULL_CHAR)
                        {
                            break;
                        }
                    }

                    /************ value ************/
                    buffer_index = 0;

                    while (mText[i] != NULL_CHAR)
                    {
                        if (mText[i] == DOUBLEQUOTES)
                        {
                            ++i;

                            while (mText[i] != DOUBLEQUOTES)
                            {
                                data.value[buffer_index++] = mText[i];
                                ++i;

                                if (mText[i] == DOUBLEQUOTES)
                                {
                                    ++i;
                                    break;
                                }
                            }

                            break;
                        }
                        else
                        {
                            if (mText[i] == TAP || mText[i] == SPACE || mText[i] == EQUAL || mText[i] == CR || mText[i] == LF || mText[i] == NULL_CHAR)
                            {
                                break;
                            }

                            data.value[buffer_index++] = mText[i];
                            ++i;
                        }
                    }

                    data.value[buffer_index] = NULL_CHAR;

                    mData.push_back(data);
                }

                ++i;
            }
        }
    }
}