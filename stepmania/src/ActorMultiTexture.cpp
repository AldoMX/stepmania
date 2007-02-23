#include "global.h"
#include <cassert>

#include "ActorMultiTexture.h"
#include "RageTextureManager.h"
#include "XmlFile.h"
#include "RageLog.h"
#include "RageDisplay.h"
#include "RageTexture.h"
#include "RageUtil.h"
#include "ActorUtil.h"
#include "Foreach.h"
#include "LuaBinding.h"
#include "LuaManager.h"

REGISTER_ACTOR_CLASS( ActorMultiTexture )


ActorMultiTexture::ActorMultiTexture()
{
	m_EffectMode = EffectMode_Normal;
}


ActorMultiTexture::~ActorMultiTexture()
{
	ClearTextures();
}

ActorMultiTexture::ActorMultiTexture( const ActorMultiTexture &cpy ):
	Actor( cpy )
{
#define CPY(a) a = cpy.a
	CPY( m_Rect );
	CPY( m_aTextureUnits );
#undef CPY

	FOREACH( TextureUnitState, m_aTextureUnits, tex )
		tex->m_pTexture = TEXTUREMAN->CopyTexture( tex->m_pTexture );
}

void ActorMultiTexture::SetTextureCoords( const RectF &r )
{
	m_Rect = r;
}

void ActorMultiTexture::LoadFromNode( const XNode* pNode )
{
	m_Rect = RectF( 0, 0, 1, 1 );
	Actor::LoadFromNode( pNode );
}

void ActorMultiTexture::SetSizeFromTexture( RageTexture *pTexture )
{
	ActorMultiTexture::m_size.x = pTexture->GetSourceWidth();
	ActorMultiTexture::m_size.y = pTexture->GetSourceHeight();
}

void ActorMultiTexture::ClearTextures()
{
	FOREACH( TextureUnitState, m_aTextureUnits, tex )
		TEXTUREMAN->UnloadTexture( tex->m_pTexture );
	m_aTextureUnits.clear();
}

int ActorMultiTexture::AddTexture( RageTexture *pTexture )
{
	ASSERT( pTexture != NULL );
	LOG->Trace( "ActorMultiTexture::AddTexture( %s )", pTexture->GetID().filename.c_str() );

	m_aTextureUnits.push_back( TextureUnitState() );
	m_aTextureUnits.back().m_pTexture = TEXTUREMAN->CopyTexture( pTexture );
	return m_aTextureUnits.size() - 1;
}

void ActorMultiTexture::SetTextureMode( int iIndex, TextureMode tm )
{
	ASSERT( iIndex < (int) m_aTextureUnits.size() );
	m_aTextureUnits[iIndex].m_TextureMode = tm;
}

void ActorMultiTexture::DrawPrimitives()
{
	Actor::SetGlobalRenderStates();	// set Actor-specified render states

	RectF quadVerticies;

	switch( m_HorizAlign )
	{
	case HorizAlign_Left:	quadVerticies.left = 0;			quadVerticies.right = m_size.x;		break;
	case HorizAlign_Center:	quadVerticies.left = -m_size.x/2;	quadVerticies.right = m_size.x/2;	break;
	case HorizAlign_Right:	quadVerticies.left = -m_size.x;		quadVerticies.right = 0;		break;
	default:		ASSERT( false );
	}

	switch( m_VertAlign )
	{
	case VertAlign_Top:	quadVerticies.top = 0;			quadVerticies.bottom = m_size.y;	break;
	case VertAlign_Middle:	quadVerticies.top = -m_size.y/2;	quadVerticies.bottom = m_size.y/2;	break;
	case VertAlign_Bottom:	quadVerticies.top = -m_size.y;		quadVerticies.bottom = 0;		break;
	default:		ASSERT(0);
	}

	DISPLAY->ClearAllTextures();
	for( size_t i = 0; i < m_aTextureUnits.size(); ++i )
	{
		TextureUnit tu = enum_add2(TextureUnit_1, i);
		DISPLAY->SetTexture( tu, m_aTextureUnits[i].m_pTexture->GetTexHandle() );
		DISPLAY->SetTextureWrapping( tu, m_bTextureWrapping );
		DISPLAY->SetTextureMode( tu, m_aTextureUnits[i].m_TextureMode );
	}

	DISPLAY->SetEffectMode( m_EffectMode );

	static RageSpriteVertex v[4];
	v[0].p = RageVector3( quadVerticies.left,	quadVerticies.top,	0 );	// top left
	v[1].p = RageVector3( quadVerticies.left,	quadVerticies.bottom,	0 );	// bottom left
	v[2].p = RageVector3( quadVerticies.right,	quadVerticies.bottom,	0 );	// bottom right
	v[3].p = RageVector3( quadVerticies.right,	quadVerticies.top,	0 );	// top right

	const RectF *pTexCoordRect = &m_Rect;
	v[0].t = RageVector2( pTexCoordRect->left, pTexCoordRect->top );	// top left
	v[1].t = RageVector2( pTexCoordRect->left, pTexCoordRect->bottom );	// bottom left
	v[2].t = RageVector2( pTexCoordRect->right, pTexCoordRect->bottom );	// bottom right
	v[3].t = RageVector2( pTexCoordRect->right, pTexCoordRect->top );	// top right

	v[0].c = m_pTempState->diffuse[0];	// top left
	v[1].c = m_pTempState->diffuse[2];	// bottom left
	v[2].c = m_pTempState->diffuse[3];	// bottom right
	v[3].c = m_pTempState->diffuse[1];	// top right

	DISPLAY->DrawQuad( v );

	for( size_t i = 0; i < m_aTextureUnits.size(); ++i )
		DISPLAY->SetTexture( enum_add2(TextureUnit_1, i), 0 );

	DISPLAY->SetEffectMode( EffectMode_Normal );
}

bool ActorMultiTexture::EarlyAbortDraw() const
{
	return m_aTextureUnits.empty();
}


// lua start
#include "LuaBinding.h"

class LunaActorMultiTexture: public Luna<ActorMultiTexture>
{
public:
	static int ClearTextures( T* p, lua_State *L )
	{
		p->ClearTextures();
		return 0;
	}
	static int AddTexture( T* p, lua_State *L )
	{
		RageTexture *pTexture = Luna<RageTexture>::check(L, 1);
		int iRet = p->AddTexture( pTexture );
		lua_pushinteger( L, iRet );
		return 1;
	}
	static int SetTextureMode( T* p, lua_State *L )
	{
		int iIndex = IArg(1);
		TextureMode tm = Enum::Check<TextureMode>(L, 2);
		p->SetTextureMode( iIndex, tm );
		return 0;
	}
	static int SetTextureCoords( T* p, lua_State *L )
	{
		p->SetTextureCoords( RectF(FArg(1), FArg(2), FArg(3), FArg(4)) );
		return 0;
	}
	static int SetSizeFromTexture( T* p, lua_State *L )
	{
		RageTexture *pTexture = Luna<RageTexture>::check(L, 1);
		p->SetSizeFromTexture( pTexture );
		return 0;
	}
	static int SetEffectMode( T* p, lua_State *L )
	{
		EffectMode em = Enum::Check<EffectMode>(L, 1);
		p->SetEffectMode( em );
		return 0;
	}

	LunaActorMultiTexture()
	{
		ADD_METHOD( ClearTextures );
		ADD_METHOD( AddTexture );
		ADD_METHOD( SetTextureMode );
		ADD_METHOD( SetTextureCoords );
		ADD_METHOD( SetSizeFromTexture );
		ADD_METHOD( SetEffectMode );
	}
};

LUA_REGISTER_DERIVED_CLASS( ActorMultiTexture, Actor )
// lua end

/*
 * (c) 2001-2007 Glenn Maynard, Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */