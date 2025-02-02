/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2023 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  MIPS specific handling of registers saved across calls.
*
****************************************************************************/


#include "_cgstd.h"
#include "coderep.h"
#include "cgaux.h"
#include "cgauxcc.h"
#include "cgmem.h"
#include "data.h"
#include "utils.h"
#include "rgtbl.h"
#include "typemap.h"
#include "makeblk.h"
#include "bldcall.h"
#include "feprotos.h"


hw_reg_set SavedRegs( void )
/**************************/
{
    hw_reg_set          saved;

    HW_CAsgn( saved, HW_EMPTY );
    HW_CTurnOn( saved, HW_R16 );
    HW_CTurnOn( saved, HW_R17 );
    HW_CTurnOn( saved, HW_R18 );
    HW_CTurnOn( saved, HW_R19 );
    HW_CTurnOn( saved, HW_R20 );
    HW_CTurnOn( saved, HW_R21 );
    HW_CTurnOn( saved, HW_R22 );
    HW_CTurnOn( saved, HW_R23 );
    HW_CTurnOn( saved, HW_R30 );
    HW_CTurnOn( saved, HW_R31 );
    HW_CTurnOn( saved, HW_F20 );
    HW_CTurnOn( saved, HW_F21 );
    HW_CTurnOn( saved, HW_F22 );
    HW_CTurnOn( saved, HW_F23 );
    HW_CTurnOn( saved, HW_F24 );
    HW_CTurnOn( saved, HW_F25 );
    HW_CTurnOn( saved, HW_F26 );
    HW_CTurnOn( saved, HW_F27 );
    HW_CTurnOn( saved, HW_F28 );
    HW_CTurnOn( saved, HW_F29 );
    HW_CTurnOn( saved, HW_F30 );
    return( saved );
}


type_class_def CallState( aux_handle aux, type_def *tipe, call_state *state )
/***************************************************************************/
{
    type_class_def      type_class;
    byte                i;
    hw_reg_set          parms[20];
    hw_reg_set          *parm_src;
    hw_reg_set          *parm_dst;
    hw_reg_set          *pregs;
    call_class          cclass;
    byte_seq            *code;
    bool                have_aux_code = false;

    state->unalterable = FixedRegs();
    if( FEAttr( AskForLblSym( CurrProc->label ) ) & FE_VARARGS ) {
        HW_TurnOn( state->unalterable, VarargsHomePtr() );
    }

    // For code bursts only, query the #pragma aux instead of using
    // hardcoded calling convention. If it ever turns out that we need
    // to support more than a single calling convention, this will need
    // to change to work more like x86
    if( !AskIfRTLabel( CurrProc->label ) ) {
        code = FEAuxInfo( aux, FEINF_CALL_BYTES );
        if( code != NULL ) {
            have_aux_code = true;
        }
    }

    pregs = FEAuxInfo( aux, FEINF_SAVE_REGS );
    HW_CAsgn( state->modify, HW_FULL );
    if( have_aux_code ) {
        HW_TurnOff( state->modify, *pregs );
    } else {
        HW_TurnOff( state->modify, SavedRegs() );
    }
    HW_CTurnOff( state->modify, HW_UNUSED );
    state->used = state->modify;    /* anything not saved is used */
    state->attr = 0;
    cclass = (call_class)(pointer_uint)FEAuxInfo( aux, FEINF_CALL_CLASS );
    if( cclass & FECALL_GEN_SETJMP_KLUGE ) {
        state->attr |= ROUTINE_IS_SETJMP;
    }
    if( cclass & FECALL_GEN_ABORTS ) {
        state->attr |= ROUTINE_NEVER_RETURNS_ABORTS;
    }
    if( cclass & FECALL_GEN_NORETURN ) {
        state->attr |= ROUTINE_NEVER_RETURNS_NORETURN;
    }
    if( cclass & FECALL_GEN_NO_MEMORY_CHANGED ) {
        state->attr |= ROUTINE_MODIFIES_NO_MEMORY;
    }
    if( cclass & FECALL_GEN_NO_MEMORY_READ ) {
        state->attr |= ROUTINE_READS_NO_MEMORY;
    }
    if( have_aux_code ) {
        parm_src = FEAuxInfo( aux, FEINF_PARM_REGS );
    } else {
        parm_src = ParmRegs();
    }

    i = 0;
    parm_dst = &parms[0];
    for( ; !HW_CEqual( *parm_src, HW_EMPTY ); ++parm_src ) {
        *parm_dst = *parm_src;
        if( HW_Ovlap( *parm_dst, state->unalterable ) ) {
            FEMessage( MSG_BAD_SAVE, aux );
        }
        HW_CTurnOff( *parm_dst, HW_UNUSED );
        parm_dst++;
        i++;
    }
    *parm_dst = *parm_src;
    i++;
    state->parm.table = CGAlloc( i * sizeof( hw_reg_set ) );
    Copy( parms, state->parm.table, i * sizeof( hw_reg_set ) );
    HW_CAsgn( state->parm.used, HW_EMPTY );
    state->parm.curr_entry = state->parm.table;
    state->parm.offset  = 0;
    type_class = ReturnTypeClass( tipe, state->attr );
    UpdateReturn( state, tipe, type_class, aux );
    return( type_class );
}


void UpdateReturn( call_state *state, type_def *tipe, type_class_def type_class, aux_handle aux )
/***********************************************************************************************/
{
    /* unused parameters */ (void)tipe; (void)aux;

    state->return_reg = ReturnReg( type_class );
}

#if 0
hw_reg_set RAReg( void )
/**********************/
{
    return( HW_R31 );
}
#endif

hw_reg_set CallZap( call_state *state )
/*************************************/
{
    hw_reg_set  zap;
    hw_reg_set  tmp;

    zap = state->modify;
    if( (state->attr & ROUTINE_MODIFY_EXACT) == 0 ) {
        HW_TurnOn( zap, state->parm.used );
        HW_TurnOn( zap, state->return_reg );
        HW_TurnOn( zap, ReturnAddrReg() );
        tmp = ReturnReg( WD );
        HW_TurnOn( zap, tmp );
    }
    return( zap );
}


hw_reg_set MustSaveRegs( void )
/*****************************/
{
    hw_reg_set  save;
    hw_reg_set  tmp;

    HW_CAsgn( save, HW_FULL );
    HW_TurnOff( save, CurrProc->state.modify );
    HW_CTurnOff( save, HW_UNUSED );
    if( CurrProc->state.attr & ROUTINE_MODIFY_EXACT ) {
        HW_TurnOff( save, CurrProc->state.return_reg );
    } else {
        tmp = CurrProc->state.parm.used;
        HW_TurnOn( tmp, CurrProc->state.return_reg );
        HW_TurnOff( save, tmp );
    }
    tmp = StackReg();
    HW_TurnOff( save, tmp );
    if( HW_CEqual( CurrProc->state.return_reg, HW_EMPTY ) ) {
        tmp = ReturnReg( WD );
        HW_TurnOff( save, tmp );
    }
    tmp = CurrProc->state.unalterable;
    HW_TurnOff( tmp, DisplayReg() );
    HW_TurnOff( tmp, StackReg() );
    HW_TurnOff( save, tmp );
    return( save );
}


hw_reg_set SaveRegs( void )
/*************************/
{
    hw_reg_set   save;

    save = MustSaveRegs();
    HW_OnlyOn( save, CurrProc->state.used );
    return( save );
}


bool IsStackReg( name *n )
/************************/
{
    if( n == NULL )
        return( false );
    if( n->n.class != N_REGISTER )
        return( false );
    if( !HW_CEqual( n->r.reg, HW_R29 ) && !HW_CEqual( n->r.reg, HW_D29 ) )
        return( false );
    return( true );
}


hw_reg_set HighOffsetReg( hw_reg_set regs )
/*****************************************/
{
    /* unused parameters */ (void)regs;

    return( HW_EMPTY );
}


hw_reg_set LowOffsetReg( hw_reg_set regs )
/****************************************/
{
    /* unused parameters */ (void)regs;

    return( HW_EMPTY );
}
