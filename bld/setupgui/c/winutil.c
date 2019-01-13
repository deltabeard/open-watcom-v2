/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Windows specific utility routines.
*
****************************************************************************/


#define INCLUDE_COMMDLG_H
#define INCL_WINSHELLDATA
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include "wio.h"
#include "watcom.h"
#include "setup.h"
#if !defined( _UI ) && ( defined( __NT__ ) || defined( __WINDOWS__ ) )
    #include "fontstr.h"
#endif
#include "setupinf.h"
#include "utils.h"
#include "genvbl.h"


#define FILE_SIZE       _MAX_PATH
#define VALUE_SIZE      MAXVALUE

// *********************** Function for writing to WIN.INI *******************

#if defined( __NT__ )

static void CreateRegEntry( const char *hive_key, const char *app_name, const char *key_name,
                     const char *value, const char *file_name, bool add )
{
    char                buf[_MAX_PATH];
    long                rc;
    HKEY                hkey1;
    DWORD               disposition;
    DWORD               type;
    DWORD               old_type;
    size_t              len;
    long                dword_val;
    HKEY                key;
    unsigned char       *bin_buf;
    int                 i;

    strcpy( buf, file_name );
    len = strlen( buf );
    if( buf[len - 1] != '\\' ) {
        strcat( buf, "\\" );
    }
    strcat( buf, app_name );
    if( stricmp( hive_key, "local_machine" ) == 0 ) {
        key = HKEY_LOCAL_MACHINE;
    } else if( stricmp( hive_key, "current_user" ) == 0 ) {
        key = HKEY_CURRENT_USER;
    } else {
        key = HKEY_LOCAL_MACHINE;
    }
    if( add ) {
        rc = RegCreateKeyEx( key, buf, 0, NULL, REG_OPTION_NON_VOLATILE,
                             KEY_WRITE, NULL, &hkey1, &disposition );
        if( key_name[0] != '\0' ) {
            if( value[0] == '#' ) {     // dword
                dword_val = atoi( value + 1 );
                rc = RegSetValueEx( hkey1, key_name, 0, REG_DWORD, (LPBYTE)&dword_val, sizeof( long ) );
            } else if( value[0] == '%' ) {      // binary
                ++value;
                len = strlen( value );
                bin_buf = malloc( len / 2 );
                if( bin_buf != NULL ) {
                    for( i = 0; i < len / 2; ++i ) {
                        if( tolower( value[0] ) >= 'a' ) {
                            bin_buf[i] = value[0] - 'a' + 10;
                        } else {
                            bin_buf[i] = value[0] - '0';
                        }
                        bin_buf[i] = bin_buf[i] * 16;
                        if( tolower( value[1] ) >= 'a' ) {
                            bin_buf[i] += value[1] - 'a' + 10;
                        } else {
                            bin_buf[i] += value[1] - '0';
                        }
                        value += 2;
                    }
                    rc = RegSetValueEx( hkey1, key_name, 0, REG_BINARY, (LPBYTE)bin_buf, (DWORD)( len / 2 ) );
                    free( bin_buf );
                }
            } else {
                rc = RegQueryValueEx( hkey1, key_name, NULL, &old_type, NULL, NULL );
                if( rc == 0 ) {
                    type = old_type;
                } else {
                    type = REG_SZ;
                }
                rc = RegSetValueEx( hkey1, key_name, 0, type, (LPBYTE)value, (DWORD)( strlen( value ) + 1 ) );
            }
        }
    } else {
        rc = RegDeleteKey( key, buf );
    }
}


bool GetRegString( HKEY hive, const char *section, const char *value, VBUF *str )
/*******************************************************************************/
{
    HKEY                hkey;
    LONG                rc;
    DWORD               type;
    bool                ret;
    char                buffer[MAXBUF];
    DWORD               buff_size;

    VbufRewind( str );
    buff_size = sizeof( buffer );
    ret = false;
    rc = RegOpenKeyEx( hive, section, 0L, KEY_ALL_ACCESS, &hkey );
    if( rc == ERROR_SUCCESS ) {
        // get the value
        rc = RegQueryValueEx( hkey, value, NULL, &type, (LPBYTE)buffer, &buff_size );
        RegCloseKey( hkey );
        VbufConcStr( str, buffer );
        ret = ( rc == ERROR_SUCCESS );
    }
    return( ret );
}

static DWORD ConvertDataToDWORD( BYTE *data, DWORD num_bytes, DWORD type )
/************************************************************************/
{
    DWORD                       i;
    DWORD                       temp;

    if( type == REG_DWORD || type == REG_DWORD_LITTLE_ENDIAN || type  == REG_BINARY ) {
        return( (DWORD)(*data) );
    } else if( type == REG_DWORD_BIG_ENDIAN ) {
        temp = 0;
        for( i = 0; i < num_bytes; i++ ) {
            temp |= ((DWORD)data[num_bytes - 1 - i]) << (i * 8);
        }
        return( temp );
    }
    return( 0 );
}

static BYTE *ConvertDWORDToData( DWORD number, DWORD type )
/**************************************************/
{
    int                         i;
    static BYTE                 buff[5];

    memset( buff, 0, sizeof( buff ) );
    if( type == REG_DWORD || type == REG_DWORD_LITTLE_ENDIAN || type  == REG_BINARY ) {
        memcpy( buff, &number, sizeof( number ) );
    } else if( type == REG_DWORD_BIG_ENDIAN ) {
        for( i = 0; i < sizeof( number ); i++ ) {
            buff[i] = ((BYTE *)(&number))[sizeof( number ) - 1 - i];
        }
    }
    return( buff );
}

static signed int AddToUsageCount( const char *path, signed int value )
/*********************************************************************/
{
    HKEY                        key_handle;
    LONG                        result;
    DWORD                       value_type;
    DWORD                       orig_value;
    BYTE                        buff[5];
    DWORD                       buff_size = sizeof( buff );
    signed int                  return_value;
    LONG                        new_value;

    result = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SharedDLLs",
                           0, KEY_ALL_ACCESS, &key_handle );
    if( result != ERROR_SUCCESS ) {
        return( -1 );
    }

    result = RegQueryValueEx( key_handle, path, 0, &value_type, buff, &buff_size );
    if( result != ERROR_SUCCESS ) {
        orig_value = 0;
        value_type = REG_DWORD;
    } else {
        orig_value = ConvertDataToDWORD( buff, buff_size, value_type );
    }

    // Don't increment if reinstalling and file already has a nonzero count
    if( GetVariableBoolVal( "ReInstall" ) && orig_value != 0 && value > 0 ) {
        value = 0;
    }

    new_value = (long)orig_value + value;

    if( new_value > 0 ) {
        memcpy( buff, ConvertDWORDToData( new_value, value_type ), sizeof( new_value ) );
        result = RegSetValueEx( key_handle, path, 0, value_type, buff, sizeof( new_value ) );
    } else if( new_value == 0 ) {
        result = RegDeleteValue( key_handle, path );
    }

    return_value = new_value;

    if( new_value >= 0 && result != ERROR_SUCCESS ) {
        return_value = -1;
    }

    if( RegFlushKey( key_handle ) != ERROR_SUCCESS ) {
        return_value = -1;
    }

    if( RegCloseKey( key_handle ) != ERROR_SUCCESS ) {
        return_value = -1;
    }

    return( return_value );
}

signed int IncrementDLLUsageCount( const char *path )
/***************************************************/
{
    return AddToUsageCount( path, 1 );
}

signed int DecrementDLLUsageCount( const char *path )
/***************************************************/
{
    return AddToUsageCount( path, -1 );
}

#endif

#if defined( __WINDOWS__ ) || defined( __NT__ )

static bool ZapKey( const char *app_name, const char *old, const char *new,
                                const char *file, const char *hive, int pos )
/***************************************************************************/
{
    FILE        *io;
    char        buff[MAXVALUE];
    size_t      app_len;
    size_t      old_len;
    int         num = 0;
    bool        in_sect = false;

    /* invalidate cache copy of INI file */
    app_len = strlen( app_name );
    old_len = strlen( old );
    WritePrivateProfileString( NULL, NULL, NULL, file );
    io = fopen( hive, "r+t" );
    if( io == NULL )
        return( false );
    while( fgets( buff, sizeof( buff ), io ) ) {
        if( buff[0] == '[' ) {
            if( in_sect )
                break;
            if( strncmp( app_name, buff + 1, app_len ) == 0 && buff[app_len + 1] == ']' ) {
                in_sect = true;
            }
        } else if( in_sect ) {
            if( strncmp( old, buff, old_len ) == 0 && buff[old_len] == '=' ) {
                if( num++ == pos ) {
                    memcpy( buff, new, old_len );
                    fseek( io, -(long)(strlen( buff ) + 1), SEEK_CUR );
                    fputs( buff, io );
                    fclose( io );
                    return( true );
                }
            }
        }
    }
    fclose( io );
    WritePrivateProfileString( NULL, NULL, NULL, file );
    return( false );
}

#define DEVICE_STRING "device"
#define ALT_DEVICE    "ecived"

static void AddDevice( const char *app_name, const char *value, const char *file,
                                                    const char *hive, bool add )
/*********************************************************************************/
{
    int         i;
    char        old_name[_MAX_FNAME];
    char        new_name[_MAX_FNAME];
    char        old_ext[_MAX_EXT];
    char        new_ext[_MAX_EXT];
    bool        done = false;

    _splitpath( value, NULL, NULL, new_name, new_ext );
    for( i = 0; ZapKey( app_name, DEVICE_STRING, ALT_DEVICE, file, hive, i ); ++i ) {
        char    value_buf[_MAX_PATH];

        GetPrivateProfileString( app_name, ALT_DEVICE, "", value_buf, sizeof( value_buf ), file );
        _splitpath( value_buf, NULL, NULL, old_name, old_ext );
        if( stricmp( old_name, new_name ) == 0 && stricmp( old_ext, new_ext ) == 0 ) {
            WritePrivateProfileString( app_name, ALT_DEVICE, add ? value : NULL, file );
            done = true;
        }
        ZapKey( app_name, ALT_DEVICE, DEVICE_STRING, file, hive, 0 );
        if( done ) {
            break;
        }
    }
    if( !done && add ) {
        WritePrivateProfileString( app_name, ALT_DEVICE, value, file );
        ZapKey( app_name, ALT_DEVICE, DEVICE_STRING, file, hive, 0 );
    }
}

static void WindowsWriteProfile( const char *app_name, const char *key_name,
                            const char *value, const char *file_name, bool add )
/******************************************************************************/
{
    const char          *key;
    char                *substr;
    size_t              len;

    key = key_name;
    switch( key[0] ) {
    case '+':
      {
        char    value_buf[VALUE_SIZE];
        VBUF    vbuf;

        ++key;
        VbufInit( &vbuf );
        GetPrivateProfileString( app_name, key, "", value_buf, sizeof( value_buf ), file_name );
        VbufConcStr( &vbuf, value_buf );
        len = strlen( value );
        substr = stristr( VbufString( &vbuf ), value, len );
        if( substr != NULL ) {
            if( !add ) {
                VBUF    tmp;

                VbufInit( &tmp );
                VbufConcStr( &tmp, substr + len );
                VbufSetLen( &vbuf, substr - VbufString( &vbuf ) );
                VbufConcVbuf( &vbuf, &tmp );
                VbufFree( &tmp );
            }
        } else {
            if( add ) {
                if( VbufLen( &vbuf ) > 0 )
                    VbufConcChr( &vbuf, ' ' );
                VbufConcStr( &vbuf, value );
            }
        }
        WritePrivateProfileString( app_name, key, VbufString( &vbuf ), file_name );
        VbufFree( &vbuf );
        break;
      }
    case '*':
      {
        VBUF    hive;

        VbufInit( &hive );
        if( strpbrk( file_name, "\\/:" ) == NULL ) {
            char    windir[_MAX_PATH];

            GetWindowsDirectory( windir, sizeof( windir ) );
            VbufConcStr( &hive, windir );
            VbufConcChr( &hive, '\\' );
        }
        VbufConcStr( &hive, file_name );
        AddDevice( app_name, value, file_name, VbufString( &hive ), add );
        VbufFree( &hive );
        break;
      }
    default:
        if( add ) {
            WritePrivateProfileString( app_name, key, value, file_name );
        } else {
            // if file doesn't exist, Windows creates 0-length file
            if( access( file_name, F_OK ) == 0 ) {
                WritePrivateProfileString( app_name, key, NULL, file_name );
            }
        }
        break;
    }
}

#endif

#if defined( __OS2__ )

static void OS2WriteProfile( const char *app_name, const char *key_name,
                      const char *value, const char *file_name, bool add )
{
    HAB                 hab;
    HINI                hini;
    PRFPROFILE          profile;
    char                userfname[1], drive[_MAX_DRIVE], dir[_MAX_DIR];
    char                inifile[_MAX_PATH];

    // get an anchor block
    hab = WinQueryAnchorBlock( HWND_DESKTOP );
    // find location of os2.ini the file we want should be there too
    profile.cchUserName = 1;
    profile.pszUserName = userfname;
    profile.cchSysName = _MAX_PATH - 1;
    profile.pszSysName = inifile;
    if( !PrfQueryProfile( hab, &profile ) ) {
        return;
    }
    // replace os2.ini with filename
    _splitpath( inifile, drive, dir, NULL, NULL );
    _makepath( inifile, drive, dir, file_name, NULL );
    // now we can open the correct ini file
    hini = PrfOpenProfile( hab, inifile );
    if( hini != NULLHANDLE ) {
        PrfWriteProfileString( hini, app_name, key_name, add ? value : NULL );
        PrfCloseProfile( hini );
    }
}

#endif


void WriteProfileStrings( bool uninstall )
/****************************************/
{
    int                 num;
    int                 i;
    int                 sign;
    int                 end;
    VBUF                app_name;
    VBUF                key_name;
    VBUF                file_name;
    VBUF                hive_name;
    VBUF                value;
    bool                add;


    add = false;
    num = SimNumProfile();
    if( uninstall ) {
        i = num - 1;
        sign = -1;
        end = -1;
    } else {
        i = 0;
        sign = 1;
        end = num;
    }
    VbufInit( &app_name );
    VbufInit( &key_name );
    VbufInit( &hive_name );
    VbufInit( &value );
    VbufInit( &file_name );
    for( ; i != end; i += sign ) {
        SimProfInfo( i, &app_name, &key_name, &value, &file_name, &hive_name );
        ReplaceVars( &value, NULL );
        ReplaceVars( &file_name, NULL );
        if( !uninstall ) {
            add = SimCheckProfCondition( i );
            if( !add ) {
                continue;
            }
        }
#if defined( __WINDOWS__ )
        WindowsWriteProfile( VbufString( &app_name ), VbufString( &key_name ),
            VbufString( &value ), VbufString( &file_name ), add );
#elif defined( __NT__ )
        if( VbufLen( &hive_name ) > 0 ) {
            CreateRegEntry( VbufString( &hive_name ), VbufString( &app_name ),
                VbufString( &key_name ), VbufString( &value ), VbufString( &file_name ), add );
        } else {
            WindowsWriteProfile( VbufString( &app_name ), VbufString( &key_name ),
                VbufString( &value ), VbufString( &file_name ), add );
        }
#elif defined( __OS2__ )
        OS2WriteProfile( VbufString( &app_name ), VbufString( &key_name ),
                            VbufString( &value ), VbufString( &file_name ), add );
#endif
    }
    VbufFree( &file_name );
    VbufFree( &value );
    VbufFree( &hive_name );
    VbufFree( &key_name );
    VbufFree( &app_name );
}

void SetDialogFont()
{
#if (defined( __NT__ ) || defined( __WINDOWS__ )) && !defined( _UI )

    char            *fontstr;
    LOGFONT         lf;
    char            dlgfont[100];
  #if defined( __NT__ ) && !defined( _M_X64 )
    DWORD   ver;
  #endif

    if( !GetVariableBoolVal( "IsJapanese" ) ) {
        fontstr = GUIGetFontInfo( MainWnd );
        GetLogFontFromString( &lf, fontstr );
//      following line removed - has no effect on line spacing, it only
//      causes the dialog boxes to be too narrow in Win 4.0
//      lf.lfHeight = (lf.lfHeight * 8)/12;
  #if defined( __NT__ ) && defined( _M_X64 )
        lf.lfWeight = FW_NORMAL;
  #elif defined( __NT__ )
        ver = GetVersion();
        if( ver < 0x80000000 && LOBYTE( LOWORD( ver ) ) >= 4 ) {
            lf.lfWeight = FW_NORMAL;
        } else {
            lf.lfWeight = FW_BOLD;
        }
  #else
        lf.lfWeight = FW_BOLD;
  #endif
        strcpy(lf.lfFaceName, "MS Sans Serif");
        GetFontFormatString( &lf, dlgfont );
        GUISetFontInfo( MainWnd, dlgfont );
        GUIMemFree( fontstr );
    }
#endif
}

#if defined( __NT__ ) && !defined( _M_X64 )
typedef BOOL (WINAPI *ISWOW64PROCESS_FN)( HANDLE, PBOOL );

bool IsWOW64( void )
{
    ISWOW64PROCESS_FN   fn;
    BOOL                retval;
    HANDLE              h;
    bool                rc;

    rc = false;
    h = GetModuleHandle( "kernel32" );
    fn = (ISWOW64PROCESS_FN)GetProcAddress( h, "IsWow64Process" );
    if( fn != NULL ) {
        if( fn( GetCurrentProcess(), &retval ) && retval ) {
            rc = true;
        }
    }
    return( rc );
}
#endif
