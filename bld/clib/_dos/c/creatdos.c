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
* Description:  DOS implementation of _dos_creat / _dos_creatnew (LFN-enabled)
*
****************************************************************************/


#include "variety.h"
#include <string.h>
#include <fcntl.h>
#include "seterrno.h"
#include "_doslfn.h"

#ifdef _M_I86
    #define INIT_VALUE
  #ifdef __BIG_DATA__
    #define SAVE_VALUE  "mov es:[bx],ax"
    #define AUX_INFO \
        __parm __caller     [__dx __ax] [__cx] [__es __bx] \
        __value             [__ax] \
        __modify __exact    [__ax __dx]
  #else
    #define SAVE_VALUE  "mov [bx],ax"
    #define AUX_INFO \
        __parm __caller     [__dx] [__cx] [__bx] \
        __value             [__ax] \
        __modify __exact    [__ax]
  #endif
#else
    #define INIT_VALUE  "xor eax,eax"
    #define SAVE_VALUE  "mov [ebx],eax"
    #define AUX_INFO \
        __parm __caller     [__edx] [__ecx] [__ebx] \
        __value             [__eax] \
        __modify __exact    [__eax]
#endif

extern unsigned __dos_create_sfn( const char *name, unsigned attrib, int *handle );
#pragma aux __dos_create_sfn = \
        _SET_DSDX           \
        INIT_VALUE          \
        _MOV_AH DOS_CREAT   \
        _INT_21             \
        _RST_DS             \
        RETURN_VALUE        \
        "call __doserror_"  \
    AUX_INFO

extern unsigned __dos_create_new_sfn( const char *name, unsigned attrib, int *handle );
#pragma aux __dos_create_new_sfn = \
        _SET_DSDX           \
        INIT_VALUE          \
        _MOV_AH DOS_CREATE_NEW \
        _INT_21             \
        _RST_DS             \
        RETURN_VALUE        \
        "call __doserror_"  \
    AUX_INFO

#ifdef __WATCOM_LFN__
static tiny_ret_t _dos_create_ex_lfn( const char *path, unsigned attrib, unsigned style )
/***************************************************************************************/
{
  #ifdef _M_I86
    return( __dos_create_ex_lfn( path, O_WRONLY, attrib, style ) );
  #else
    call_struct     dpmi_rm;

    strcpy( RM_TB_PARM1_LINEAR, path );
    memset( &dpmi_rm, 0, sizeof( dpmi_rm ) );
    dpmi_rm.ds  = RM_TB_PARM1_SEGM;
    dpmi_rm.esi = RM_TB_PARM1_OFFS;
    dpmi_rm.edx = style;
    dpmi_rm.ecx = attrib;
    dpmi_rm.ebx = O_WRONLY;
    dpmi_rm.eax = 0x716C;
    dpmi_rm.flags = 1;
    if( __dpmi_dos_call( &dpmi_rm ) ) {
        return( -1 );
    }
    if( dpmi_rm.flags & 1 ) {
        return( TINY_RET_ERROR( dpmi_rm.ax ) );
    }
    return( dpmi_rm.ax );
  #endif
}
#endif

_WCRTLINK unsigned _dos_creat( const char *path, unsigned attrib, int *handle )
/*****************************************************************************/
{
#ifdef __WATCOM_LFN__
    tiny_ret_t  rc = 0;

    if( _RWD_uselfn && TINY_OK( rc = _dos_create_ex_lfn( path, attrib, EX_LFN_CREATE ) ) ) {
        *handle = TINY_INFO( rc );
        return( 0 );
    }
    if( IS_LFN_ERROR( rc ) ) {
        return( __set_errno_dos_reterr( TINY_INFO( rc ) ) );
    }
#endif
    return( __dos_create_sfn( path, attrib, handle ) );
}

_WCRTLINK unsigned _dos_creatnew( const char *path, unsigned attrib, int *handle )
/********************************************************************************/
{
#ifdef __WATCOM_LFN__
    tiny_ret_t  rc = 0;

    if( _RWD_uselfn && TINY_OK( rc = _dos_create_ex_lfn( path, attrib, EX_LFN_CREATE_NEW ) ) ) {
        *handle = TINY_INFO( rc );
        return( 0 );
    }
    if( IS_LFN_ERROR( rc ) ) {
        return( __set_errno_dos_reterr( TINY_INFO( rc ) ) );
    }
#endif
    return( __dos_create_new_sfn( path, attrib, handle ) );
}
