/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include "Framework.h"
#include "Action.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "MenuStrings.h"
#include "PlayerIntroduceDialog.h"
#include "gameinfo.h"
#include "AnimatedBanner.h"
#include "MovieBanner.h"
#include "Utils.h"


// ---------------------------------------------------------------------------
// CS2-style main menu: dark top bar, icons on the left, tabs in the centre.
// ---------------------------------------------------------------------------
#define ICON_HOME  "gfx/shell/ui_home"
#define ICON_TV    "gfx/shell/ui_tv"
#define ICON_GEAR  "gfx/shell/ui_gear"
#define ICON_POWER "gfx/shell/ui_power"

#define TOPBAR_H 56

// tab with an underline under the hovered one
class CTopTab : public CMenuAction
{
public:
	typedef CMenuAction BaseClass;
	void Draw() override
	{
		if( this == m_pParent->ItemAtCursor() && !FBitSet( iFlags, QMF_GRAYED ))
		{
			UI_FillRect( m_scPos.x + m_scSize.w * 0.15f, m_scPos.y + m_scSize.h - 5 * uiStatic.scaleY,
				m_scSize.w * 0.7f, 3 * uiStatic.scaleY, PackRGBA( 255, 255, 255, 235 ));
		}
		BaseClass::Draw();
	}
};

// round icon button, tinted on hover
class CTopIcon : public CMenuAction
{
public:
	typedef CMenuAction BaseClass;
	void SetIcon( const char *path ) { pic = path; }
	void Draw() override
	{
		unsigned int col = ( this == m_pParent->ItemAtCursor() ) ? (unsigned int)colorFocus : (unsigned int)colorBase;
		if( FBitSet( iFlags, QMF_GRAYED )) col = PackRGBA( 90, 90, 90, 255 );
		UI_DrawPic( m_scPos, m_scSize, col, pic, QM_DRAWTRANS );
	}
private:
	CImage pic;
};

class CMenuMain: public CMenuFramework
{
public:
	CMenuMain() : CMenuFramework( "CMenuMain" ) { }

	bool KeyDown( int key ) override;

	void QuitDialogCb();

private:
	void _Init() override;
	void _VidInit( ) override;
	void Think() override;

	void VidInit(bool connected);

	void DisconnectCb();
	void DisconnectDialogCb();
	void HazardCourseDialogCb();
	void HazardCourseCb();

	CMenuAnimatedBanner animatedBanner;
	CMenuMovieBanner movieBanner;

	// top bar
	CMenuAction topBar, topLine;
	CTopIcon iconHome, iconTv, iconGear, iconPower;
	CTopTab tabTraining, tabPlayer, tabPlay, tabCreate, tabMore;

	// in-game helpers (shown below the bar only when it makes sense)
	CMenuAction console, resumeGame, disconnect;

	// quit dialog
	CMenuYesNoMessageBox dialog;

	bool bTrainMap;
	bool bCustomGame;
};

void CMenuMain::QuitDialogCb()
{
	if( CL_IsActive() && EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
		dialog.SetMessage( L( "StringsList_235" ) );
	else
		dialog.SetMessage( L( "GameUI_QuitConfirmationText" ) );

	dialog.onPositive.SetCommand( false, "quit \"menu dialog\"\n" );
	dialog.Show();
}

void CMenuMain::DisconnectCb()
{
	EngFuncs::ClientCmd( false, "disconnect\n" );
	VidInit( false );
	CalcPosition();
	CalcSizes();
	VidInitItems();
}

void CMenuMain::DisconnectDialogCb()
{
	dialog.onPositive = VoidCb( &CMenuMain::DisconnectCb );
	dialog.SetMessage( L( "Really disconnect?" ) );
	dialog.Show();
}

void CMenuMain::HazardCourseDialogCb()
{
	dialog.onPositive = VoidCb( &CMenuMain::HazardCourseCb );;
	dialog.SetMessage( L( "StringsList_234" ) );
	dialog.Show();
}

bool CMenuMain::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		if ( CL_IsActive( ))
		{
			if( !dialog.IsVisible() )
				UI_CloseMenu();
		}
		else
		{
			QuitDialogCb( );
		}
		return true;
	}
	return CMenuFramework::KeyDown( key );
}

void CMenuMain::HazardCourseCb()
{
	if( EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) > 1 )
		EngFuncs::HostEndGame( "end of the game" );

	EngFuncs::CvarSetValue( "skill", 1.0f );
	EngFuncs::CvarSetValue( "deathmatch", 0.0f );
	EngFuncs::CvarSetValue( "teamplay", 0.0f );
	EngFuncs::CvarSetValue( "pausable", 1.0f ); // singleplayer is always allowing pause
	EngFuncs::CvarSetValue( "coop", 0.0f );
	EngFuncs::CvarSetValue( "maxplayers", 1.0f ); // singleplayer

	EngFuncs::PlayBackgroundTrack( NULL, NULL );

	EngFuncs::ClientCmd( false, "hazardcourse\n" );
}

static void SetupTab( CTopTab &t, const char *name, const char *status, EFontSizes size, bool bright )
{
	t.SetNameAndStatus( name, status );
	t.SetCharSize( size );
	t.eTextAlignment = QM_CENTER;
	t.eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	t.colorBase = bright ? PackRGBA( 255, 255, 255, 255 ) : PackRGBA( 196, 202, 210, 255 );
	t.colorFocus = PackRGBA( 255, 255, 255, 255 );
}

static void SetupIcon( CTopIcon &i, const char *path, const char *status )
{
	i.SetIcon( path );
	i.SetNameAndStatus( "", status );
	i.colorBase = PackRGBA( 196, 202, 210, 255 );
	i.colorFocus = PackRGBA( 255, 255, 255, 255 );
}

static void SetupPill( CMenuAction &p, const char *name, const char *status )
{
	p.SetNameAndStatus( name, status );
	p.SetCharSize( QM_SMALLFONT );
	p.eTextAlignment = QM_CENTER;
	p.eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	p.colorBase = PackRGBA( 255, 255, 255, 255 );
	p.colorFocus = PackRGBA( 255, 255, 255, 255 );
	p.SetBackground( PackRGBA( 18, 22, 26, 205 ), PackRGBA( 44, 112, 62, 235 ));
}

void CMenuMain::_Init( void )
{
	if( gMenu.m_gameinfo.trainmap[0] && stricmp( gMenu.m_gameinfo.trainmap, gMenu.m_gameinfo.startmap ) != 0 )
		bTrainMap = true;
	else bTrainMap = false;

	if( EngFuncs::GetCvarFloat( "host_allow_changegame" ))
		bCustomGame = true;
	else bCustomGame = false;

	// bar background
	topBar.SetNameAndStatus( "", NULL );
	topBar.SetBackground( PackRGBA( 22, 26, 32, 226 ), PackRGBA( 22, 26, 32, 226 ));
	topBar.iFlags |= QMF_INACTIVE;
	topLine.SetNameAndStatus( "", NULL );
	topLine.SetBackground( PackRGBA( 255, 255, 255, 38 ), PackRGBA( 255, 255, 255, 38 ));
	topLine.iFlags |= QMF_INACTIVE;

	// left icons
	SetupIcon( iconHome, ICON_HOME, L( "GameUI_GameMenu_ResumeGame" ));
	iconHome.colorBase = PackRGBA( 96, 190, 255, 255 ); // "you are here"
	iconHome.onReleased = UI_CloseMenu;
	SetupIcon( iconTv, ICON_TV, L( "StringsList_400" ));
	SET_EVENT( iconTv.onReleased, EngFuncs::ShellExecute( MenuStrings[ IDS_MEDIA_PREVIEWURL ], NULL, false ) );
	SetupIcon( iconGear, ICON_GEAR, L( "StringsList_193" ));
	iconGear.onReleased = UI_Options_Menu;
	SetupIcon( iconPower, ICON_POWER, L( "GameUI_QuitConfirmationText" ));
	iconPower.onReleased = VoidCb( &CMenuMain::QuitDialogCb );

	// centre tabs
	SetupTab( tabTraining, L( "Training" ), L( "StringsList_190" ), QM_DEFAULTFONT, false );
	tabTraining.onReleasedClActive = VoidCb( &CMenuMain::HazardCourseDialogCb );
	tabTraining.onReleased = VoidCb( &CMenuMain::HazardCourseCb );

	SetupTab( tabPlayer, L( "Player" ), L( "Player setup" ), QM_DEFAULTFONT, false );
	tabPlayer.onReleased = UI_PlayerSetup_Menu;

	SetupTab( tabPlay, L( "PLAY" ), L( "StringsList_198" ), QM_BIGFONT, true );
	tabPlay.onReleased = UI_MultiPlayer_Menu;

	SetupTab( tabCreate, L( "Create server" ), L( "Create a new game server" ), QM_DEFAULTFONT, false );
	tabCreate.onReleased = UI_CreateGame_Menu;

	if( bCustomGame )
	{
		SetupTab( tabMore, L( "GameUI_ChangeGame" ), L( "StringsList_530" ), QM_DEFAULTFONT, false );
		tabMore.onReleased = UI_CustomGame_Menu;
	}
	else
	{
		SetupTab( tabMore, L( "Credits" ), L( "Credits" ), QM_DEFAULTFONT, false );
		tabMore.onReleased = UI_Credits_Menu;
	}

	// in-game helpers
	SetupPill( resumeGame, L( "GameUI_GameMenu_ResumeGame" ), L( "StringsList_188" ));
	resumeGame.onReleased = UI_CloseMenu;
	SetupPill( disconnect, L( "GameUI_GameMenu_Disconnect" ), L( "Disconnect from server" ));
	disconnect.onReleased = VoidCb( &CMenuMain::DisconnectDialogCb );
	SetupPill( console, L( "GameUI_Console" ), L( "Show console" ));
	console.SetVisibility( gpGlobals->developer );
	SET_EVENT_MULTI( console.onReleased,
	{
		UI_SetActiveMenu( false );
		EngFuncs::KEY_SetDest( KEY_CONSOLE );
	});

	if ( gMenu.m_gameinfo.gamemode == GAME_SINGLEPLAYER_ONLY )
	{
		tabPlay.SetGrayed( true );
		tabCreate.SetGrayed( true );
	}

	// too short execute string - not a real command
	if( strlen( MenuStrings[IDS_MEDIA_PREVIEWURL] ) <= 3 )
		iconTv.SetGrayed( true );

	// server.dll needs for reading savefiles or startup newgame
	if( !EngFuncs::CheckGameDll( ))
		tabTraining.SetGrayed( true );

	if( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY && !bTrainMap )
		tabTraining.SetGrayed( true );

	if( FBitSet( gMenu.m_gameinfo.flags, GFL_ANIMATED_TITLE ))
	{
		if( !uiStatic.lowmemory && animatedBanner.TryLoad())
			AddItem( animatedBanner );
	}
	else
	{
		AddItem( movieBanner );
	}

	dialog.Link( this );

	AddItem( topBar );
	AddItem( topLine );
	AddItem( iconHome );
	AddItem( iconTv );
	AddItem( iconGear );
	AddItem( iconPower );
	AddItem( tabTraining );
	AddItem( tabPlayer );
	AddItem( tabPlay );
	AddItem( tabCreate );
	AddItem( tabMore );
	AddItem( resumeGame );
	AddItem( disconnect );
	AddItem( console );
}

void CMenuMain::VidInit( bool connected )
{
	const int w = uiStatic.width;

	topBar.SetRect( 0, 0, w, TOPBAR_H );
	topLine.SetRect( 0, TOPBAR_H, w, 2 );

	// icons
	const int isz = 34, ipad = 12, iy = ( TOPBAR_H - isz ) / 2;
	CTopIcon *icons[] = { &iconHome, &iconTv, &iconGear, &iconPower };
	for( int i = 0; i < 4; i++ )
		icons[i]->SetRect( 18 + i * ( isz + ipad ), iy, isz, isz );

	// tabs, centred, PLAY is wider
	const int iconsEnd = 18 + 4 * ( isz + ipad );
	int avail = w - 2 * iconsEnd;
	float unit = avail / 5.4f;
	if( unit > 175 ) unit = 175;
	const int tw = (int)unit, pw = (int)( unit * 1.4f );
	const int total = tw * 4 + pw;
	int x = ( w - total ) / 2;

	const bool compact = unit < 135;
	CTopTab *tabs[] = { &tabTraining, &tabPlayer, &tabPlay, &tabCreate, &tabMore };
	for( int i = 0; i < 5; i++ )
	{
		const int cw = ( i == 2 ) ? pw : tw;
		tabs[i]->SetRect( x, 0, cw, TOPBAR_H );
		if( compact && i != 2 ) tabs[i]->SetCharSize( QM_SMALLFONT );
		x += cw;
	}

	// helpers under the bar (left)
	int py = TOPBAR_H + 18;
	const int pillw = 190, pillh = 34;
	resumeGame.SetVisibility( connected );
	disconnect.SetVisibility( connected && gpGlobals->maxClients > 1 );
	if( connected )
	{
		resumeGame.SetRect( 18, py, pillw, pillh );
		py += pillh + 8;
		if( gpGlobals->maxClients > 1 )
		{
			disconnect.SetRect( 18, py, pillw, pillh );
			py += pillh + 8;
		}
	}
	console.SetRect( 18, py, pillw, pillh );
}

void CMenuMain::_VidInit()
{
	VidInit( CL_IsActive() );
}

void CMenuMain::Think()
{
	if( gpGlobals->developer )
	{
		if( !console.IsVisible( ))
			console.Show();
	}
	else
	{
		if( console.IsVisible( ))
			console.Hide();
	}

	CMenuFramework::Think();
}

ADD_MENU( menu_main, CMenuMain, UI_Main_Menu );


/*
=================
UI_QuitDialog_f

called by the engine when the OS asks to quit (window close button, Cmd+Q on macOS...)
=================
*/
static void UI_QuitDialog_f( void )
{
	// nothing to lose, quit immediately
	if( !CL_IsActive( ))
	{
		EngFuncs::ClientCmd( false, "quit \"menu\"\n" );
		return;
	}

	if( !UI_IsVisible( ))
		UI_Main_Menu();

	menu_main->QuitDialogCb();
}
ADD_COMMAND( menu_quit, UI_QuitDialog_f );
