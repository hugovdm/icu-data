/*
 * Oddities that you will find while converting Unicode between codepages on
 * Windows.  You can search for Oddity for messages to reenable.
 * 
 * - ConvertStringToUnicodeEx ignores the fallback parameter so we can't properly
 *  get the reverse fallback. The default fallback seems to be \u30FB for mapping
 *  codepage bytes to Unicode when the mapping is undefined.  For example,
 *  windows-932 "<U30FB> \x81\xAD |3"
 *  See http://www.microsoft.com/globaldev/reference/dbcs/932.htm
 * - ConvertStringToUnicodeEx and MultiByteToWideChar sometimes convert differently
 *  These seem to convert differently:
 *  28599 - Turkish (ISO) - iso-8859-9, 1-1 bytes
 *  28591 - Western European (ISO) - iso-8859-1, 1-1 bytes
 *  51949 - Korean (EUC) - euc-kr, 1-2 bytes
 *  Some byte sequences aren't even converted by the C API, but that is expected.
 * - The whole 51xxxx series seem to be auto-select
 * - codepages larger than 2 bytes don't seem to be collected correctly.
 *  For example, windows-51932 euc-jp
 *  <U4E08> \xBE\xE6 |0
 *  <U4E08> \x8F\xE7 |3 # 3 byte lead byte converted as 2 byte reverse fallback
 *  <U4E08> \x8F\xE8 |3 # 3 byte lead byte converted as 2 byte reverse fallback
 *  <U4E08> \xBD\x44 |3 # 3 byte lead byte converted as 2 byte reverse fallback
 * - Some reverse fallback mappings do not have a roundtrip mapping.
 *  For example, windows-51949 is one that has many of these mappings.
 * - Some PUA characters are not converted without a larger buffer
 *  For example, in windows-708 <UF8C1> \xA0 |0
 * - windows-20932 (JIS X 0208-1990 & 0212-1990) has single byte reverse
 *  fallbacks for invalid state ranges (80-8D,8F-9F). These are not collected.
 * - windows-51932 had too many reverse fallbacks. It looks like only the last
 *  byte was consumed.
 *  For example:
 *  <U0040> \x8E\x40 |3
 *  <U0041> \x8E\x41 |3
 *  <U0042> \x8E\x42 |3
 *  <U0043> \x8E\x43 |3
 *  <U0044> \x8E\x44 |3
 *  <U0045> \x8E\x45 |3
 *  windows-51949 also looks suspicious for reverse fallbacks.
 * - windows-51932 will convert \x8f\xa2\xc3\xc3 to <U6F64><UFF81><U5E33>,
 *  it will convert \x8f\xa2\xc3 to <U6F64><UF8F2> and it will convert
 *  \x8f\xa2 to <U6F64><UF8F2>
 * - Windows doesn't support surrogates by default, but this support could be enabled
 *  by using regedit.
 *  [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\LanguagePack] 
 *  SURROGATE=(REG_DWORD)0x00000002
 *  Enableing it has no affect on conversion. Surrogate mappings need to be collected
 *  from Windows XP.
 * - Some of the Windows codepage identifiers are just aliases to other codepages,
 *  but the different number _may_ be used to denote how strict it is for importing
 *  or exporting data.
 * - Parts of gb18030 have a reverse fallback to \uFFFF. These are ignored.
 * - 51936 had differences from the original file for some unknown reason.
 */



#ifdef _WIN32
#pragma warning( disable : 4710 ) // Silly inline warning level 4 warning
//#pragma warning( push )
#pragma warning( disable : 4786 ) // disable the warning "identifier was truncated to '255' characters in the debug information" generated by STL
//#pragma warning( disable : 4100 4663 4511 4512 4018 ) // Silly warning level 4 warnings
#endif // _WIN32

#include "convert.h"
#include <stdio.h>






// OLE safe release (Win32)

#define safe_release(ptr) {if (ptr){ptr -> Release();ptr = NULL; }}

// global for enum proc interface
//static map<cp_id, encoding_info_ptr>* p_map_encoding_info;
static UHashtable *pg_map_encoding_info;
//static vector<cp_id>* p_encodings;
static UVector *pg_encodings;

// global for codepage names for CodePageEnumProc
//static map<cp_id, std::string> misc_cp_names;

const char *get_misc_cp_name(int32_t id)
{
    switch (id) {
//    case 437: return "MS-DOS United States";
    case 708: return "Arabic (ASMO 708)";
    case 709: return "Arabic (ASMO 449+, BCON V4)";
    case 710: return "Arabic (Transparent Arabic)";
//    case 720: return "Arabic (Transparent ASMO)";
//    case 737: return "Greek (formerly 437G)";
//    case 775: return "Baltic";
//    case 850: return "MS-DOS Multilingual (Latin I)";
//    case 852: return "MS-DOS Slavic (Latin II)";
    case 855: return "IBM Cyrillic (primarily Russian)";
//    case 857: return "IBM Turkish";
    case 858: return "OEM - Multilingual Latin 1 + Euro";
    case 860: return "Portuguese (DOS)";
//    case 861: return "MS-DOS Icelandic";
    case 862: return "Hebrew";
    case 863: return "French Canadian (DOS)";
    case 864: return "Arabic (864)";
    case 865: return "Nordic (DOS)";
//    case 866: return "MS-DOS Russian (former USSR)";
//    case 869: return "IBM Modern Greek";
//    case 874: return "Thai";
//    case 932: return "Japan";
//    case 936: return "Chinese (PRC, Singapore)";
    case 949: return "Korean";
//    case 950: return "Chinese (Taiwan; Hong Kong SAR, PRC)";
    case 1047: return "IBM EBCDIC - Latin-1/Open System";
    case 1200: return "Unicode (BMP of ISO 10646)";
    case 1250: return "Windows 3.1 Eastern European";
    case 1251: return "Windows 3.1 Cyrillic";
    case 1252: return "Windows 3.1 Latin 1 (US, Western Europe)";
    case 1253: return "Windows 3.1 Greek";
    case 1254: return "Windows 3.1 Turkish";
    case 1255: return "Hebrew";
    case 1256: return "Arabic";
    case 1257: return "Baltic";
    case 1258: return "Latin 1 (ANSI)";
    case 1361: return "Korean (Johab)";
//    case 10000: return "(MAC - Roman)";
//    case 10001: return "(MAC - Japanese)";
//    case 10002: return "(MAC - Traditional Chinese Big5)";
//    case 10003: return "(MAC - Korean)";
//    case 10004: return "(MAC - Arabic)";
//    case 10005: return "(MAC - Hebrew)";
//    case 10006: return "(MAC - Greek I)";
//    case 10007: return "(MAC - Cyrillic)";
//    case 10008: return "(MAC - Simplified Chinese GB 2312)";
    case 10010: return "Romanian (Mac)";
    case 10017: return "Ukrainian (Mac)";
//    case 10029: return "(MAC - Latin II)";
//    case 10079: return "(MAC - Icelandic)";
//    case 10081: return "(MAC - Turkish)";
    case 10082: return "Croatian (Mac)";
    case 20000: return "CNS - Taiwan";
    case 20001: return "TCA - Taiwan";
    case 20002: return "Eten - Taiwan";
    case 20003: return "IBM5550 - Taiwan";
    case 20004: return "TeleText - Taiwan";
    case 20005: return "Wang - Taiwan";
    case 20127: return "US ASCII";
    case 20261: return "T.61";
    case 20269: return "ISO-6937";
    case 20866: return "Ukrainian - KOI8-U";
    case 20924: return "IBM EBCDIC - Latin-1/Open System(1047 + Euro)";
    case 20932: return "Japanese (JIS 0208-1990 and 0212-1990)";
    case 20936: return "Chinese Simplified (GB2312-80)";
    case 20949: return "Korean Wansung";
    case 21027: return "Ext Alpha Lowercase";
    case 21866: return "Russian - KOI8";
    case 28591: return "ISO 8859-1 Latin I";
    case 28592: return "ISO 8859-2 Eastern Europe";
    case 28593: return "ISO 8859-3 Turkish";
    case 28594: return "ISO 8859-4 Baltic";
    case 28595: return "ISO 8859-5 Cyrillic";
    case 28596: return "ISO 8859-6 Arabic";
    case 28597: return "ISO 8859-7 Greek";
    case 28598: return "ISO 8859-8 Hebrew";
    case 28599: return "ISO 8859-9 Latin Alphabet No.5";
    case 29001: return "Europa 3";
    case 50227: return "Chinese Simplified (ISO-2022)";
    };
    return "";
}


BOOL CALLBACK CodePageEnumProc(LPTSTR lpCodePageString)
{
    int32_t cp;
    
    sscanf(lpCodePageString,"%d", &cp);
    
    void *ptr = uhash_iget(pg_map_encoding_info, cp);
    
    if (ptr == NULL)
    {
        UErrorCode status = U_ZERO_ERROR;
        encoding_info *enc_info = new encoding_info;

        strcpy(enc_info->web_charset_name, "");

//        map<cp_id, std::string>::iterator iname = misc_cp_names.find(cp);

//        if ( iname != misc_cp_names.end() ) {
//           strcpy(enc_info->charset_description, (*iname).second.c_str());
//        }
//        else {
//           strcpy(enc_info->charset_description, "No description available");
           strcpy(enc_info->charset_description, get_misc_cp_name(cp));
//        }
        
        pg_encodings->addElement(cp, status);
        uhash_iput(pg_map_encoding_info, cp, enc_info, &status);
        if (U_FAILURE(status)) {
            printf("Error %s:%d %s", __FILE__, __LINE__, u_errorName(status));
        }
    }
    
    return TRUE;
}

// get encodings supported by the system
int
converter::get_supported_encodings(UVector *p_encodings, UHashtable *p_map_encoding_info,
                                   int argc, const char* const argv[])
{
#ifdef _WIN32
    IMultiLanguage2 *multilanguage;
    IEnumCodePage *mlang_enum_codepage;
    IMLangConvertCharset* mlang_convert_charset;
    
    cp_id cnum = 0;
    PMIMECPINFO mime_cp_info;
    unsigned long count_cp_info;
    HRESULT hr;
    char description[100];
    char web_charset_name[100];
    UINT src_len;
    UINT dst_len;
    UBool collectMoreInfo = TRUE;
    
    hr = CoInitialize(NULL);
    
    if (FAILED(hr)) {
        fprintf(stderr, "Error initializing OLE subsystem: %d\n", hr);
        exit(1);
    }
    
    hr = CoCreateInstance(
        CLSID_CMultiLanguage,     //REFCLSID rclsid,     //Class identifier (CLSID) of the object
        NULL,                     //LPUNKNOWN pUnkOuter, //Pointer to controlling IUnknown
        CLSCTX_INPROC_SERVER,     //DWORD dwClsContext,  //Context for running executable code
        IID_IMultiLanguage2 ,     //REFIID riid,         //Reference to the identifier of the interface
        (void**) &multilanguage   //LPVOID * ppv         //Address of output variable that receives 
        // the interface pointer requested in riid
        );
    
    if ( NULL == multilanguage ) {
        DWORD err = GetLastError();
        fprintf(stderr, "Error initializing MLang subsystem: %u (%08X)\n", err, hr);
        return -1;
    }
    
    hr = multilanguage->EnumCodePages(
        MIMECONTF_VALID_NLS,  //    DWORD grfFlags,
        MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),   // [in]  LANGID LangId,
        &mlang_enum_codepage    //    IEnumCodePage **ppEnumCodePage
        );
    
    hr = multilanguage->CreateConvertCharset(1200, 1252, 0, &mlang_convert_charset);
    
    hr = multilanguage->GetNumberOfCodePageInfo((UINT *)&cnum);
    
    mime_cp_info = (PMIMECPINFO)CoTaskMemAlloc(sizeof(MIMECPINFO)*cnum);
    
    hr = mlang_enum_codepage->Next(cnum, mime_cp_info, &count_cp_info);
    
    for(uint32_t i = 0; i < count_cp_info; i++)
    {
        UErrorCode status = U_ZERO_ERROR;
        UBool collectEncoding = TRUE;

        dst_len = sizeof(description);
        src_len = wcslen(mime_cp_info[i].wszDescription);
        
        mlang_convert_charset->DoConversionFromUnicode(mime_cp_info[i].wszDescription,
            &src_len, description, &dst_len);
        description[dst_len] = '\0';
        
        dst_len = sizeof(web_charset_name);
        src_len = wcslen(mime_cp_info[i].wszWebCharset);
        
        mlang_convert_charset->DoConversionFromUnicode(mime_cp_info[i].wszWebCharset,
            &src_len, web_charset_name, &dst_len);
        web_charset_name[dst_len] = '\0';
        
        // printf("\"%s\", \"%s\", %d\n", description, web_charset_name, mime_cp_info[i].uiCodePage);
        
        // look for match for converter name/code page number on the command line
        for(int32_t j = 0; j < argc; j++)
        {
            collectEncoding = FALSE;
            if (0 != strstr(web_charset_name, argv[j]))
            {
                collectEncoding = TRUE;
            }
            else
            {
                cp_id converter_match = -1;
                sscanf(argv[j], "%d", &converter_match);
                if (converter_match == (cp_id)mime_cp_info[i].uiCodePage)
                {
                    collectEncoding = TRUE;
                }
            }
        }
        if (!collectEncoding)
        {
            collectMoreInfo = FALSE;
            continue;
        }
        encoding_info *pEncoding = new encoding_info;
        strcpy(pEncoding->web_charset_name, web_charset_name);
        strcpy(pEncoding->charset_description, description);
        
        p_encodings->addElement(mime_cp_info[i].uiCodePage, status);
        uhash_iput(p_map_encoding_info, mime_cp_info[i].uiCodePage, pEncoding, &status);
        if (U_FAILURE(status)) {
            printf("Error %s:%d %s", __FILE__, __LINE__, u_errorName(status));
            return -1;
        }
    }
    
    safe_release(multilanguage);
    safe_release(mlang_enum_codepage);
    safe_release(mlang_convert_charset);
    
    CoTaskMemFree(mime_cp_info);
    
    CoUninitialize();
    
    // now get any additional ones supported by the wctomb/bmtowc APIs
    
    // global for the enum proc
    pg_map_encoding_info = p_map_encoding_info;
    pg_encodings = p_encodings;
    
    // init names
//    load_misc_cp_names();

    if (collectMoreInfo)
    {
        EnumSystemCodePages(CodePageEnumProc, CP_INSTALLED);
    }
    
#endif // _WIN32
    
    return 0;
}

converter::converter(cp_id cp, encoding_info *enc_info)
{
    m_multilanguage = 0;
    m_hr = 0;
    m_err = 0;
    m_enc_num = cp;
    m_enc_info = enc_info;
    m_name[0] = 0;
    
#ifdef _WIN32
    sprintf(m_name, "%d", cp);

    m_hr = CoInitialize(NULL);
    
    if (FAILED(m_hr)) {
        m_err = GetLastError();
        fprintf(stderr, "Error initializing OLE subsystem: %d\n", m_err);
        return ;
    }
    
    m_hr = CoCreateInstance(
        CLSID_CMultiLanguage,      //REFCLSID rclsid,     //Class identifier (CLSID) of the object
        NULL,                      //LPUNKNOWN pUnkOuter, //Pointer to controlling IUnknown
        CLSCTX_INPROC_SERVER,      //DWORD dwClsContext,  //Context for running executable code
        IID_IMultiLanguage2,       //REFIID riid,         //Reference to the identifier of the interface
        (void**) &m_multilanguage  //LPVOID * ppv         //Address of output variable that receives 
        // the interface pointer requested in riid
        );
    
    if (NULL == m_multilanguage) {
        m_err = GetLastError();
        fprintf(stderr, "Error initializing MLang subsystem: %u (%08X)\n", m_err, m_hr);
        return ;
    }
    
#endif _WIN32
    
}

converter::~converter()
{
#ifdef _WIN32
    safe_release(m_multilanguage);
    
    CoUninitialize();
#endif
}

size_t converter::from_unicode(char* target, char* target_limit, const UChar* source, const UChar* source_limit, unsigned int flags)
{
#ifdef _WIN32
    
    UINT srcSize, targ_size_bytes;
    
    srcSize = source_limit - source;
    targ_size_bytes = target_limit - target;
    
    // Setting this to 0 before each conversion means that we lose the conversion context
    DWORD mode_flags = 0;
    m_hr = m_multilanguage->ConvertStringFromUnicodeEx(   &mode_flags,
        m_enc_num,
        (WCHAR*) source, 
        &srcSize,
        target, 
        &targ_size_bytes,
        flags,
        L"");
    
    if (FAILED(m_hr))
    {
//        fprintf(stderr, "ConvertStringToUnicodeEx failed on <U%04X>: %d\n", (uint16_t)source[0], m_hr);
        targ_size_bytes = 0;
    }
    if (*source && 0 == *target && (1 == targ_size_bytes || (2 == targ_size_bytes && UTF_IS_SURROGATE(*source))))
    {
        targ_size_bytes = 0;
    }
    
    return (size_t)targ_size_bytes;
    
#endif // _WIN32
}

size_t converter::to_unicode(UChar* target, UChar* target_limit, const char* source, const char* source_limit)
{
#ifdef _WIN32
    
    UChar unibuffTest[16];

    UINT src_size_bytes, targ_size;
    
    src_size_bytes = source_limit - source;
    targ_size = target_limit - target;
    
    // Setting this to 0 before each conversion means that we lose the conversion context
    DWORD mode_flags = 0;
    m_hr = m_multilanguage->ConvertStringToUnicodeEx(  &mode_flags,
        m_enc_num,
        (char*)source,
        &src_size_bytes,
        target,
        &targ_size,
        0,
        NULL);
    
    if (FAILED(m_hr))
    {
//        fprintf(stderr, "ConvertStringToUnicodeEx failed on %X %X: %d\n", (uint8_t)source[0], (uint8_t)source[1], m_hr);
        targ_size = 0;
    }
    if ( *source && 1 == targ_size && 0 == *target )
    {
        targ_size = 0;
    }

//    if (targ_size == 1 && src_size_bytes > 1 && target[0] == (UChar)((uint8_t)source[0])) {
    if (targ_size > 0 && ((size_t)src_size_bytes != (size_t)(source_limit - source))) {
        /* It didn't really do the conversion. Ignore the results. */
        /* This usually happens for windows-20932 */
//        printf("The whole buffer wasn't consumed for <%04X> \\x%02X\\x%02X\n", target[0], (uint8_t)source[0], (uint8_t)source[1]);
        targ_size = 0;
    }

    int convertedSize = MultiByteToWideChar(m_enc_num, 0, source, src_size_bytes, unibuffTest, 16);
    if (source[0] != 0x3f && source[0] != 0x6f) {
        if (convertedSize > 0 && convertedSize != (int)targ_size) {
            // Oddity
//            printf("Consistency warning! %X %X %X %X isn't converted by both APIs\n", (uint32_t)source[0], (uint32_t)source[1], (uint32_t)source[2], (uint32_t)source[3]);
        }
        else if (convertedSize > 0 && unibuffTest[0] != target[0]) {
            // Oddity
//            printf("\nConsistency error! %X %X goes to %04X C API or %04X COM API", (uint8_t)source[0], (uint8_t)source[1], unibuffTest[0], target[0]);
        }
    }
    /* The last two parameters don't work right now. */
/*    mode_flags = 0;
    m_hr = m_multilanguage->ConvertStringToUnicodeEx(  &mode_flags,
        m_enc_num,
        (char*)source,
        &src_size_bytes,
        target,
        &targ_size,
        MLCONVCHARF_USEDEFCHAR,
        L"\xfffd");*/

#endif // _WIN32
    
    
    return (size_t) (targ_size);
}

char *
converter::get_default_char(UChar *default_uchar)
{
    static char buff1[80];
    static char buff2[80];
    UChar ubuff[8];
    UChar* source;
    size_t num_bytes1, num_bytes2;
    
    *default_uchar = 0;
    buff1[0] = 0;

    for (UChar u = 0x100 ; u <= MAX_UNICODE_VALUE ; u++ )
    {
        ubuff[0] = u;
        source = ubuff;
        
        // use converter's own default char
        num_bytes1 = from_unicode(buff1, buff1+80, source, source+1, CONVERTER_USE_SUBST_CHAR);
        
        // do not return a default char - string unconvertable characters
        num_bytes2 = from_unicode(buff2, buff2+80, source, source+1);
        
        if (0 == num_bytes2 && num_bytes1 != 0 )
        {
            buff1[num_bytes1] = 0;

            ubuff[0] = u;
            source = ubuff;
            num_bytes1 = to_unicode(source, source+1, buff1, NULL);
            *default_uchar = ubuff[0];
            break;
        }
    }
    
    return buff1;
}

int 
converter::get_cp_info(cp_id cp, cp_info& cp_inf)
{

    strcpy(cp_inf.default_char, get_default_char(&cp_inf.default_uchar));
    return 0;
}

UBool 
converter::is_lead_byte_probeable() const
{
    // Oddity
    /* These ids do a loose match */
    return (m_enc_num != 51932 && 
            m_enc_num != 51949);
}

UBool 
converter::is_ignorable() const
{

    if (m_enc_num == 52936 ||  // hz-gb-2312
            m_enc_num == 1200 ||   // utf-16
            m_enc_num == 1201 ||   // utf-16BE
            m_enc_num == 65000 ||  // utf-7
            m_enc_num == 65001 ||  // utf-8
            m_enc_num == 50000 ||  // x-user-defined
            m_enc_num == 50227 ||  // Chinese Simplified (ISO-2022)
//            m_enc_num == 51932 ||  // loose matching euc-jp. Don't probe.
//            m_enc_num == 54949 ||  // gb18030
            m_enc_num == 57006 ||  // x-iscii-as
            m_enc_num == 57003 ||  // x-iscii-be
            m_enc_num == 57002 ||  // x-iscii-de
            m_enc_num == 57010 ||  // x-iscii-gu
            m_enc_num == 57008 ||  // x-iscii-ka
            m_enc_num == 57009 ||  // x-iscii-ma
            m_enc_num == 57007 ||  // x-iscii-or
            m_enc_num == 57011 ||  // x-iscii-pa
            m_enc_num == 57004 ||  // x-iscii-ta
            m_enc_num == 57005)     // x-iscii-te
    {
        return TRUE;
    }
    return m_enc_info->web_charset_name[0] == '_' ||   // pseudo-charsets used for auto-detect
            strstr(m_enc_info->web_charset_name, "2022");
}

const char *
converter::get_premade_state_table() const
{
#ifdef WIN32
    // Oddity
    /* Some of these state tables are unverified */
    /*
    These state tables are a combination between the IBM state tables,
    and what was generated by this program.  States that are valid in the
    IBM encodings should also be valid in microsoft.
    */
    static const struct encoding_state_table_entry {
        int32_t cp;
        const char *state_table;
    } encoding_state_table[] = {
        {932,   // Shift-JIS
        "<icu:state>                   0-80, 81-9f:1, a0-df, e0-fc:1, fd-ff\n"
        "<icu:state>                   40-7e, 80-fc\n"
        },
        {936,   // gb2312
        "<icu:state>                   0-80, 81-fe:1, ff\n"
        "<icu:state>                   40-7e, 80-fe\n"
        },
        {949,   // ks_c_5601-1987
        "<icu:state>                   0-80, 81-fe:1, ff\n"
        "<icu:state>                   40-7e, 80-fe\n"
        },
        {950,   // Big-5
        "<icu:state>                   0-80, 81-fe:1, ff\n"
        "<icu:state>                   40-7e, 80-fe\n"
        },
        {1361, // Korean (Johab)
        "<icu:state>                   0-83, 84-d3:1, d4-d7, d8-de:1, df, e0-f9:1, fa-ff\n"
        "<icu:state>                   31-7e, 81-fe\n"
        },
        {10001, // x-mac-japanese   (Shift-JIS?)
        "<icu:state>                   0-80, 81-9f:1, a0-df, e0-fc:1, fd-ff\n"
        "<icu:state>                   40-7e, 80-fc\n"
        },
        {10002, // x-mac-chinesetrad
        "<icu:state>                   0-80, 81-fc:1, fd-ff\n"
        "<icu:state>                   40-7e, a1-fe\n"
        },
        {10003, // x-mac-korean
        "<icu:state>                   0-a0, a1-ac:1, ad-af, b0-c8:1, ca-fd:1, fe-ff\n"
        "<icu:state>                   a1-fe\n"
        },
        {10008, // Mac euc-cn
        "<icu:state>                   0-a0, a1-f7:1, f8-ff\n"
        "<icu:state>                   a1-fe\n"
        },
        {20000, // x-Chinese-CNS?
        "<icu:state>                   0-a0, a1-fe:1, ff\n"
        "<icu:state>                   21-7e, a1-fe\n"
        },
        {20001, // TCA - Taiwan?
        "<icu:state>                   0-80, 81-84:1, 85-90, 91-d8:1, d9-de, df-fc:1, fd-ff\n"
        "<icu:state>                   30-39, 41-5a, 61-7a, 80-fd\n"
        },
        {20002, // x-Chinese-Eten?
        "<icu:state>                   0-80, 81-af:1, b0-dc, dd-fe:1, ff\n"
        "<icu:state>                   30-39, 41-5a, 61-7a, 80-fd\n"
        },
        {20003, // IBM5550 - Taiwan?
        "<icu:state>                   0-80, 81-84:1, 85-86, 87:1, 88, 89-e8:1, e9-f8, f9-fb:1, fc-ff\n"
        "<icu:state>                   40-7e, 80-fc\n"
        },
        {20004, // TeleText - Taiwan?
        "<icu:state>                   0-a0, a1-fe:1, ff\n"
        "<icu:state>                   21-7e, a1-fe\n"
        },
        {20005, // Wang - Taiwan?
        "<icu:state>                   0-8c, 8d-f5:1, f6-f8, f9-fc:1, fd-ff\n"
        "<icu:state>                   30-39, 41-5a, 61-7a, 8d-ee, f0-fc\n"
        },
        {20932, // EUC-JP Japanese (JIS 0208-1990 and 0212-1990)?
        "<icu:state>                   0-7f, 8e:1, a1-ab:1, ad:1, b0-fe:1\n"
        "<icu:state>                   21-7e, a1-fe\n"
        },
        {20936, // Also Mac euc-cn (Simplified Chinese GB2312)?
        "<icu:state>                   0-a0, a1-f7:1, f8-ff\n"
        "<icu:state>                   a1-fe\n"
        },
        {20949, // euc-kr like 51949
        "<icu:state>                   0-a0, a1-ac:1, ad-af, b0-c8:1, ca-fd:1, fe-ff\n"
        "<icu:state>                   a1-fe\n"
        },
        {51932, // euc-jp
        "# This is not a 3-4 byte euc-jp like ibm-33722\n"
        "<icu:state>                   0-80, 8e:1, a0, a1-a8:1, ad:1, b0-fc:1, fd-ff\n"
        "<icu:state>                   40-7e, 80-fe\n"
        },
        {51936, // GBK
        "# This is not euc-cn. This is an alias to windows-932\n"
        "<icu:state>                   0-80, 81-fe:1, ff\n"
        "<icu:state>                   40-7e, 80-fe\n"
        },
        {51949, // euc-kr
        "<icu:state>                   0-9f, a1-fe:1, ff\n"
        "<icu:state>                   a1-fe\n"
        },
/*        {, // 
        "\n"
        "\n"
        }*/
    };
    const int state_table_size = sizeof(encoding_state_table)/sizeof(encoding_state_table_entry);

    for (int idx = 0; idx < state_table_size; idx++) {
        if (m_enc_num == encoding_state_table[idx].cp) {
            return encoding_state_table[idx].state_table;
        }
    }
#endif

    return NULL;
}

const char *
converter::get_OS_vendor()
{
#ifdef _WIN32
    return "windows";
#endif
}

const char *
converter::get_OS_variant()
{
#ifdef _WIN32
    OSVERSIONINFO os_info;
    os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os_info);
    if (5 == os_info.dwMajorVersion)
    {
        if (0 == os_info.dwMinorVersion)
        {
            return "2000";
        }
        else if (1 == os_info.dwMinorVersion)
        {
            return "xp";
        }
        else if (2 == os_info.dwMinorVersion)
        {
            return "dot_net_server";
        }
    }
    else if (4 == os_info.dwMajorVersion)
    {
        if (0 == os_info.dwMinorVersion)
        {
            if (VER_PLATFORM_WIN32_NT == os_info.dwPlatformId)
            {
                return "nt";
            }
            else if (VER_PLATFORM_WIN32_WINDOWS == os_info.dwPlatformId)
            {
                return "95";
            }
        }
        else if (10 == os_info.dwMinorVersion)
        {
            return "98";
        }
        else if (90 == os_info.dwMinorVersion)
        {
            return "me";
        }
    }
    return "unknown";
#endif
}

const char *
converter::get_OS_interface()
{
#ifdef _WIN32
    static char interface_buffer[256];
    static char *pinterface = "IMultiLanguage";  // 5.50.4522.1800?
    if (interface_buffer != pinterface) {
        char mlang_dll[256] = "";
        GetSystemDirectory(mlang_dll, sizeof(mlang_dll));
        strcat(mlang_dll, "\\mlang.dll");
        DWORD dwSize;
        DWORD dwIgnore;
        LPVOID pBuf;
        VS_FIXEDFILEINFO* pLocalInfo;
        
        dwSize = GetFileVersionInfoSize(mlang_dll, &dwIgnore);
        pBuf = malloc(dwSize);
        if (pBuf && GetFileVersionInfo(mlang_dll, 0, dwSize, pBuf))
        {
            if (VerQueryValue( pBuf, "\\", (void**)&pLocalInfo, (UINT*)&dwIgnore))
            {
                sprintf(interface_buffer, "IMultiLanguage %d.%d.%d.%d",
                    HIWORD(pLocalInfo->dwFileVersionMS),
                    LOWORD(pLocalInfo->dwFileVersionMS),
                    HIWORD(pLocalInfo->dwFileVersionLS),
                    LOWORD(pLocalInfo->dwFileVersionLS));
                pinterface = interface_buffer;
            }
        }
        
        free(pBuf);
    }

    return pinterface;
#endif
}
