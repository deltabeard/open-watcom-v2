/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2022 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  Interface to Optimizing code generator for DATA
*
****************************************************************************/


#include "cvars.h"
#include "cg.h"
#include "cgdefs.h"
#include "cgswitch.h"
#include "cgprotos.h"
#include "feprotos.h"
#include "cgen.h"

#define ZEROS_BLOCK_SIZE    (8 * 1024)

void EmitZeros( target_size amount )
{
    for( ; amount > ZEROS_BLOCK_SIZE; amount -= ZEROS_BLOCK_SIZE ) {
        DGIBytes( ZEROS_BLOCK_SIZE, 0 );
    }
    DGIBytes( amount, 0 );
}

void AlignIt( TYPEPTR typ )
{
#if _INTEL_CPU
    if( OptSize == 0 ) {        /* optimize for time */
        DGAlign( GetTypeAlignment( typ ) );
    }
#else /* _RISC_CPU */
    align_type  align;

    align = GetTypeAlignment( typ );
    if( align < 4 )
        align = 4;
    DGAlign( align );
#endif
}

target_size GetDQuadPointerSize( enum quad_flags dq_flags )
{
    target_size         size;

    size = TARGET_POINTER;
#if _INTEL_CPU
    if( dq_flags & Q_NEAR_POINTER ) {
        size = TARGET_NEAR_POINTER;
    } else if( dq_flags & Q_FAR_POINTER ) {
        size = TARGET_FAR_POINTER;
    } else if( dq_flags & Q_CODE_POINTER ) {
        if( TargetSwitches & CGSW_X86_BIG_CODE ) {
            size = TARGET_FAR_POINTER;
        }
    } else {
        if( TargetSwitches & CGSW_X86_BIG_DATA ) {
            size = TARGET_FAR_POINTER;
        }
    }
#else /* _RISC_CPU */
    if( dq_flags & Q_NEAR_POINTER ) {
        size = TARGET_NEAR_POINTER;
    }
#endif
    return( size );
}

static cg_type GetDQuadPointerCGType( enum quad_flags dq_flags )
{
    cg_type             cgtype;

    if( dq_flags & Q_NEAR_POINTER ) {
        cgtype = TY_NEAR_POINTER;
    } else if( dq_flags & Q_FAR_POINTER ) {
        cgtype = TY_LONG_POINTER;
    } else if( dq_flags & Q_CODE_POINTER ) {
        cgtype = TY_CODE_PTR;
    } else {
        cgtype = TY_POINTER;
    }
    return( cgtype );
}

static void EmitDQuad( DATA_QUAD *dq )
{
    cg_type             data_type;
    target_size         amount;
    SYM_ENTRY           sym;

    static segment_id   segid = SEG_NULL;
    static target_size  size = 0;

#if _CPU == 8086
    if( size >= 0x10000 ) {
        if( segid != SEG_CONST && segid != SEG_DATA ) {
            ++segid;
            BESetSeg( segid );
            size -= 0x10000;
        }
    }
#endif
    switch( dq->type ) {
    case QDT_STATIC:
        SymGet( &sym, dq->u.var.sym_handle );
        segid = sym.u.var.segid;
        BESetSeg( segid );
        AlignIt( sym.sym_type );
        DGLabel( FEBack( (CGSYM_HANDLE)dq->u.var.sym_handle ) );
        size = 0;
        break;
    case QDT_CHAR:
    case QDT_UCHAR:
    case QDT_BOOL:
        DGInteger( dq->u_long_value1, TY_UINT_1 );
        size += sizeof( char );
        if( dq->flags & Q_2_INTS_IN_ONE ) {
            DGInteger( dq->u_long_value2, TY_UINT_1 );
            size += sizeof( char );
        }
        break;
    case QDT_SHORT:
    case QDT_USHORT:
        DGInteger( dq->u_long_value1, TY_UINT_2 );
        size += TARGET_SHORT;
        if( dq->flags & Q_2_INTS_IN_ONE ) {
            DGInteger( dq->u_long_value2, TY_UINT_2 );
            size += TARGET_SHORT;
        }
        break;
    case QDT_INT:
    case QDT_UINT:
        DGInteger( dq->u_long_value1, TY_INTEGER );
        size += TARGET_INT;
        if( dq->flags & Q_2_INTS_IN_ONE ) {
            DGInteger( dq->u_long_value2, TY_INTEGER );
            size += TARGET_INT;
        }
        break;
    case QDT_LONG:
    case QDT_ULONG:
        DGInteger( dq->u_long_value1, TY_UINT_4 );
        size += TARGET_LONG;
        if( dq->flags & Q_2_INTS_IN_ONE ) {
            DGInteger( dq->u_long_value2, TY_UINT_4 );
            size += TARGET_LONG;
        }
        break;
    case QDT_LONG64:
    case QDT_ULONG64:
        DGInteger64( dq->u.long64, TY_UINT_8 );
        size += TARGET_LONG64;
        break;

    case QDT_FLOAT:
    case QDT_FIMAGINARY:
//      ftoa( dq->u.double_value, Buffer );
//      DGFloat( Buffer, TY_SINGLE );
        {
            DATA_QUAD   local_dq;
            float       float_value;

            // dq->u.double_value may not have proper alignment on Alpha
            // so copy pieces to local copy on stack which will have
            // proper alignment
            local_dq.u_long_value1 = dq->u_long_value1;
            local_dq.u_long_value2 = dq->u_long_value2;
            float_value = (float)local_dq.u.double_value;
            DGBytes( TARGET_FLOAT, (char *)&float_value );
        }
        size += TARGET_FLOAT;
        break;
    case QDT_DOUBLE:
    case QDT_DIMAGINARY:
//      ftoa( dq->u.double_value, Buffer );
//      DGFloat( Buffer, TY_DOUBLE );
        DGBytes( TARGET_DOUBLE, (char *)&dq->u.double_value );
        size += TARGET_DOUBLE;
        break;
    case QDT_LONG_DOUBLE:
    case QDT_LDIMAGINARY:
        DGBytes( TARGET_LDOUBLE, (char *)&dq->u.long_double_value );
        size += TARGET_LDOUBLE;
        break;
    case QDT_STRING:
        data_type = GetDQuadPointerCGType( dq->flags );
        EmitStrPtr( dq->u.string_leaf, data_type );
        size += GetDQuadPointerSize( dq->flags );
        break;
    case QDT_POINTER:
    case QDT_ID:
        data_type = GetDQuadPointerCGType( dq->flags );
        if( dq->u.var.sym_handle == SYM_NULL ) {
            DGInteger( dq->u.var.offset, data_type );
        } else {
            DGFEPtr( (CGSYM_HANDLE)dq->u.var.sym_handle, data_type, dq->u.var.offset );
        }
        size += GetDQuadPointerSize( dq->flags );
        break;
    case QDT_CONST:                       /* array of characters */
        size += EmitBytes( dq->u.string_leaf );
        break;
    case QDT_CONSTANT:
#if _CPU == 8086
        for( amount = dq->u_size; amount != 0; ) {
            if( amount + size >= 0x00010000 ) {
                EmitZeros( 0x10000 - size );
                amount -= ( 0x10000 - size );
                size = 0;
                if( segid != SEG_CONST && segid != SEG_DATA ) {
                    ++segid;
                    BESetSeg( segid );
                }
            } else {
                EmitZeros( amount );
                size += amount;
                amount = 0;
            }
        }
#else
        amount = dq->u_size;
        EmitZeros( amount );
        size += amount;
#endif
        break;
    }
}


void EmitDataQuads( void )
{
    DATA_QUAD   *dq;

    if( StartDataQuadAccess() != NULL ) {
        for( ; (dq = NextDataQuad()) != NULL; ) {
            do {
                EmitDQuad( dq );
            } while( (dq->flags & Q_REPEATED_DATA) && (dq->u_rpt_count)-- > 1 );
        }
    }
}
