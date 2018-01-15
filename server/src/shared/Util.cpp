/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Util.h"
#include "logging.h"
#include "Timer.h"
#include "mersennetwister/MersenneTwister.h"
#include "utf8cpp/utf8.h"
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/tss.hpp>

static boost::thread_specific_ptr<MTRand> mtRand;

static boost::posix_time::ptime g_startup_time =
    boost::posix_time::microsec_clock::universal_time();

uint32 WorldTimer::m_iTime = 0;
uint32 WorldTimer::m_iPrevTime = 0;
time_t WorldTimer::curr_sys_time = 0;

uint32 WorldTimer::tickTime()
{
    return m_iTime;
}
uint32 WorldTimer::tickPrevTime()
{
    return m_iPrevTime;
}

uint32 WorldTimer::tick()
{
    // save previous world tick time
    m_iPrevTime = m_iTime;

    // get the new one and don't forget to persist current system time in
    // m_SystemTickTime
    m_iTime = WorldTimer::getMSTime_internal(true);

    // return tick diff
    return getMSTimeDiff(m_iPrevTime, m_iTime);
}

uint32 WorldTimer::getMSTime()
{
    return getMSTime_internal();
}

// returns the time passed in ms since the server started up
uint32 WorldTimer::getMSTime_internal(bool /*savetime*/ /*= false*/)
{
    boost::posix_time::ptime now =
        boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration diff = now - g_startup_time;

    long ms_diff = diff.total_milliseconds();
    if (ms_diff < 0)
        return 0;

    return static_cast<uint32>(ms_diff);

    // FIXME: The original code had the following comment but it means nothing
    // to me at the moment because there is never
    //        a check for old_time and the old_time and curr_time variables
    //        didn't even exist. I'll leave the comment in in case it
    //        ends up being significant and someone else can understand it:
    //
    // special case: curr_time < old_time - we suppose that our time has not
    // ticked at all
    // this should be constant value otherwise it is possible that our time can
    // start ticking backwards until next world tick!!!
}

//////////////////////////////////////////////////////////////////////////
int32 irand(int32 min, int32 max)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return int32(mtRand->randInt(max - min)) + min;
}

uint32 urand(uint32 min, uint32 max)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return mtRand->randInt(max - min) + min;
}

float frand(float min, float max)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return mtRand->randExc(max - min) + min;
}

int32 rand32()
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return mtRand->randInt();
}

double rand_norm(void)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return mtRand->randExc();
}

float rand_norm_f(void)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return (float)mtRand->randExc();
}

double rand_chance(void)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return mtRand->randExc(100.0);
}

float rand_chance_f(void)
{
    if (mtRand.get() == NULL)
        mtRand.reset(new MTRand);
    return (float)mtRand->randExc(100.0);
}

Tokens StrSplit(const std::string& src, const std::string& sep)
{
    Tokens r;
    std::string s;
    for (const auto& elem : src)
    {
        if (sep.find(elem) != std::string::npos)
        {
            if (s.length())
                r.push_back(s);
            s = "";
        }
        else
        {
            s += elem;
        }
    }
    if (s.length())
        r.push_back(s);
    return r;
}

uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index)
{
    if (index >= data.size())
        return 0;

    return (uint32)atoi(data[index].c_str());
}

float GetFloatValueFromArray(Tokens const& data, uint16 index)
{
    float result;
    uint32 temp = GetUInt32ValueFromArray(data, index);
    memcpy(&result, &temp, sizeof(result));

    return result;
}

void stripLineInvisibleChars(std::string& str)
{
    static std::string invChars = " \t\7\n";

    size_t wpos = 0;

    bool space = false;
    for (size_t pos = 0; pos < str.size(); ++pos)
    {
        if (invChars.find(str[pos]) != std::string::npos)
        {
            if (!space)
            {
                str[wpos++] = ' ';
                space = true;
            }
        }
        else
        {
            if (wpos != pos)
                str[wpos++] = str[pos];
            else
                ++wpos;
            space = false;
        }
    }

    if (wpos < str.size())
        str.erase(wpos, str.size());
}

std::string secsToTimeString(time_t timeInSecs, bool shortText, bool hoursOnly)
{
    time_t secs = timeInSecs % MINUTE;
    time_t minutes = timeInSecs % HOUR / MINUTE;
    time_t hours = timeInSecs % DAY / HOUR;
    time_t days = timeInSecs / DAY;

    std::ostringstream ss;
    if (days)
        ss << days << (shortText ? "d" : " Day(s) ");
    if (hours || hoursOnly)
        ss << hours << (shortText ? "h" : " Hour(s) ");
    if (!hoursOnly)
    {
        if (minutes)
            ss << minutes << (shortText ? "m" : " Minute(s) ");
        if (secs || (!days && !hours && !minutes))
            ss << secs << (shortText ? "s" : " Second(s).");
    }

    return ss.str();
}

uint32 TimeStringToSecs(const std::string& timestring)
{
    uint32 secs = 0;
    uint32 buffer = 0;
    uint32 multiplier = 0;

    for (const auto& elem : timestring)
    {
        if (isdigit(elem))
        {
            buffer *= 10;
            buffer += (elem) - '0';
        }
        else
        {
            switch (elem)
            {
            case 'd':
                multiplier = DAY;
                break;
            case 'h':
                multiplier = HOUR;
                break;
            case 'm':
                multiplier = MINUTE;
                break;
            case 's':
                multiplier = 1;
                break;
            default:
                return 0; // bad format
            }
            buffer *= multiplier;
            secs += buffer;
            buffer = 0;
        }
    }

    return secs;
}

std::string TimeToTimestampStr(time_t t)
{
    tm* aTm = localtime(&t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d_%02d-%02d-%02d", aTm->tm_year + 1900,
        aTm->tm_mon + 1, aTm->tm_mday, aTm->tm_hour, aTm->tm_min, aTm->tm_sec);
    return std::string(buf);
}

/// Check if the string is a valid ip address representation
bool IsIPAddress(const char* ipaddress)
{
    if (!ipaddress)
        return false;

    boost::system::error_code ec;
    boost::asio::ip::address::from_string(ipaddress, ec);
    return !ec;
}

/// create PID file
uint32 CreatePIDFile(const std::string& filename)
{
    FILE* pid_file = fopen(filename.c_str(), "w");
    if (pid_file == NULL)
        return 0;

#ifdef WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif

    fprintf(pid_file, "%d", pid);
    fclose(pid_file);

    return (uint32)pid;
}

size_t utf8length(std::string& utf8str)
{
    try
    {
        return utf8::distance(
            utf8str.c_str(), utf8str.c_str() + utf8str.size());
    }
    catch (std::exception)
    {
        utf8str = "";
        return 0;
    }
}

void utf8truncate(std::string& utf8str, size_t len)
{
    try
    {
        size_t wlen =
            utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        if (wlen <= len)
            return;

        std::wstring wstr;
        wstr.resize(wlen);
        utf8::utf8to16(
            utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
        wstr.resize(len);
        char* oend = utf8::utf16to8(
            wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str[0]);
        utf8str.resize(oend - (&utf8str[0])); // remove unused tail
    }
    catch (std::exception)
    {
        utf8str = "";
    }
}

bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize)
{
    try
    {
        size_t len = utf8::distance(utf8str, utf8str + csize);
        if (len > wsize)
        {
            if (wsize > 0)
                wstr[0] = L'\0';
            wsize = 0;
            return false;
        }

        wsize = len;
        utf8::utf8to16(utf8str, utf8str + csize, wstr);
        wstr[len] = L'\0';
    }
    catch (std::exception)
    {
        if (wsize > 0)
            wstr[0] = L'\0';
        wsize = 0;
        return false;
    }

    return true;
}

bool Utf8toWStr(const std::string& utf8str, std::wstring& wstr)
{
    try
    {
        size_t len =
            utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        wstr.resize(len);

        if (len)
            utf8::utf8to16(
                utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
    }
    catch (std::exception)
    {
        wstr = L"";
        return false;
    }

    return true;
}

bool WStrToUtf8(wchar_t* wstr, size_t size, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(size * 4); // allocate for most long case

        char* oend = utf8::utf16to8(wstr, wstr + size, &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0])); // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

bool WStrToUtf8(std::wstring wstr, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(wstr.size() * 4); // allocate for most long case

        char* oend = utf8::utf16to8(
            wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0])); // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

typedef wchar_t const* const* wstrlist;

std::wstring GetMainPartOfName(std::wstring wname, uint32 declension)
{
    // supported only Cyrillic cases
    if (wname.size() < 1 || !isCyrillicCharacter(wname[0]) || declension > 5)
        return wname;

    // Important: end length must be <= MAX_INTERNAL_PLAYER_NAME-MAX_PLAYER_NAME
    // (3 currently)

    static wchar_t const a_End[] = {
        wchar_t(1), wchar_t(0x0430), wchar_t(0x0000)};
    static wchar_t const o_End[] = {
        wchar_t(1), wchar_t(0x043E), wchar_t(0x0000)};
    static wchar_t const ya_End[] = {
        wchar_t(1), wchar_t(0x044F), wchar_t(0x0000)};
    static wchar_t const ie_End[] = {
        wchar_t(1), wchar_t(0x0435), wchar_t(0x0000)};
    static wchar_t const i_End[] = {
        wchar_t(1), wchar_t(0x0438), wchar_t(0x0000)};
    static wchar_t const yeru_End[] = {
        wchar_t(1), wchar_t(0x044B), wchar_t(0x0000)};
    static wchar_t const u_End[] = {
        wchar_t(1), wchar_t(0x0443), wchar_t(0x0000)};
    static wchar_t const yu_End[] = {
        wchar_t(1), wchar_t(0x044E), wchar_t(0x0000)};
    static wchar_t const oj_End[] = {
        wchar_t(2), wchar_t(0x043E), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const ie_j_End[] = {
        wchar_t(2), wchar_t(0x0435), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const io_j_End[] = {
        wchar_t(2), wchar_t(0x0451), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const o_m_End[] = {
        wchar_t(2), wchar_t(0x043E), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const io_m_End[] = {
        wchar_t(2), wchar_t(0x0451), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const ie_m_End[] = {
        wchar_t(2), wchar_t(0x0435), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const soft_End[] = {
        wchar_t(1), wchar_t(0x044C), wchar_t(0x0000)};
    static wchar_t const j_End[] = {
        wchar_t(1), wchar_t(0x0439), wchar_t(0x0000)};

    static wchar_t const* const dropEnds[6][8] = {
        {&a_End[1], &o_End[1], &ya_End[1], &ie_End[1], &soft_End[1], &j_End[1],
         NULL, NULL},
        {&a_End[1], &ya_End[1], &yeru_End[1], &i_End[1], NULL, NULL, NULL,
         NULL},
        {&ie_End[1], &u_End[1], &yu_End[1], &i_End[1], NULL, NULL, NULL, NULL},
        {&u_End[1], &yu_End[1], &o_End[1], &ie_End[1], &soft_End[1], &ya_End[1],
         &a_End[1], NULL},
        {&oj_End[1], &io_j_End[1], &ie_j_End[1], &o_m_End[1], &io_m_End[1],
         &ie_m_End[1], &yu_End[1], NULL},
        {&ie_End[1], &i_End[1], NULL, NULL, NULL, NULL, NULL, NULL}};

    for (wchar_t const* const* itr = &dropEnds[declension][0]; *itr; ++itr)
    {
        size_t len = size_t((*itr)[-1]); // get length from string size field

        if (wname.substr(wname.size() - len, len) == *itr)
            return wname.substr(0, wname.size() - len);
    }

    return wname;
}

bool utf8ToConsole(const std::string& utf8str, std::string& conStr)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    if (!Utf8toWStr(utf8str, wstr))
        return false;

    conStr.resize(wstr.size());
    CharToOemBuffW(&wstr[0], &conStr[0], wstr.size());
#else
    // not implemented yet
    conStr = utf8str;
#endif

    return true;
}

bool consoleToUtf8(const std::string& conStr, std::string& utf8str)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    wstr.resize(conStr.size());
    OemToCharBuffW(&conStr[0], &wstr[0], conStr.size());

    return WStrToUtf8(wstr, utf8str);
#else
    // not implemented yet
    utf8str = conStr;
    return true;
#endif
}

bool Utf8FitTo(const std::string& str, std::wstring search)
{
    std::wstring temp;

    if (!Utf8toWStr(str, temp))
        return false;

    // converting to lower case
    wstrToLower(temp);

    if (temp.find(search) == std::wstring::npos)
        return false;

    return true;
}

void utf8printf(FILE* out, const char* str, ...)
{
    va_list ap;
    va_start(ap, str);
    vutf8printf(out, str, &ap);
    va_end(ap);
}

void vutf8printf(FILE* out, const char* str, va_list* ap)
{
#if PLATFORM == PLATFORM_WINDOWS
    char temp_buf[32 * 1024];
    wchar_t wtemp_buf[32 * 1024];

    size_t temp_len = vsnprintf(temp_buf, 32 * 1024, str, *ap);

    size_t wtemp_len = 32 * 1024 - 1;
    Utf8toWStr(temp_buf, temp_len, wtemp_buf, wtemp_len);

    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    fprintf(out, "%s", temp_buf);
#else
    vfprintf(out, str, *ap);
#endif
}

void hexEncodeByteArray(uint8* bytes, uint32 arrayLen, std::string& result)
{
    std::ostringstream ss;
    for (uint32 i = 0; i < arrayLen; ++i)
    {
        for (uint8 j = 0; j < 2; ++j)
        {
            unsigned char nibble = 0x0F & (bytes[i] >> ((1 - j) * 4));
            char encodedNibble;
            if (nibble < 0x0A)
                encodedNibble = '0' + nibble;
            else
                encodedNibble = 'A' + nibble - 0x0A;
            ss << encodedNibble;
        }
    }
    result = ss.str();
}

std::string ByteArrayToHexStr(uint8* bytes, uint32 length)
{
    std::ostringstream ss;
    for (uint32 i = 0; i < length; ++i)
    {
        char buffer[4];
        sprintf(buffer, "%02X ", bytes[i]);
        ss << buffer;
    }

    return ss.str();
}

std::string::size_type utf8findascii(
    const std::string& str, std::string::size_type offset, char ascii)
{
    // FIXME: Yes, this is ugly and should use the utf8 library. But I'm in a
    // hurry, sorry!
    while (offset < str.size())
    {
        unsigned char c = str[offset];
        if ((c & 0x80) == 0) // ASCII character
        {
            if ((char)c == ascii)
                return offset;
            ++offset;
        }
        else
        {
            if (c & 0xC0 && (c & 0x20) == 0) // 2 byte chararacter
                offset += 2;
            else if (c & 0xE0 && (c & 0x10) == 0) // 3 byte chararacter
                offset += 3;
            else if (c & 0xF0 && (c & 0x8) == 0) // 4 byte chararacter
                offset += 4;
            else
                return std::string::npos;
        }
    }

    return std::string::npos;
}

void scope_performance_timer::print_log()
{
    float microsec = std::chrono::duration_cast<std::chrono::microseconds>(
                         end_ - start_).count();

    if (write_to_)
    {
        std::stringstream ss;
        ss << pre_time_;
        if (!pre_time_.empty())
            ss << " ";
        if (microsec / 1000.0f < 0.1f)
            ss << microsec << " micro seconds";
        else
            ss << microsec / 1000.0f << " ms";
        *write_to_ = ss.str();
    }
    else
    {
        if (microsec / 1000.0f < 0.1f)
            printf("%s %f micro seconds\n", pre_time_.c_str(), microsec);
        else
            printf("%s %f ms\n", pre_time_.c_str(), microsec / 1000.0f);
    }
}
