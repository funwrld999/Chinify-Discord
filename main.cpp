#include <Windows.h>
#include <psapi.h>  
#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <thread>

#include "resource.h"

#include "winreg.hpp"

#include <tray.hpp>

namespace FUNCTIONS
{
	int
		FindDiscordByMainWindow(
			HWND hwnd
		)

	{

		auto window_pid = DWORD{};

		GetWindowThreadProcessId( hwnd, &window_pid );

		if ( window_pid == 0u )

			return 0;

		auto process_handle = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, window_pid );

		if ( !process_handle )

			return 0;

		const auto DISCORD_PATH_ENDING = std::wstring( L"\\Discord.exe" );

		auto discord_pid = 0;



		CHAR process_path_raw[ MAX_PATH ] = { 0 };


		if ( GetModuleFileNameEx( process_handle, nullptr, process_path_raw, MAX_PATH ) )

		{


			auto process_path =
				std::string( process_path_raw );




			if ( process_path.size() > DISCORD_PATH_ENDING.size() && std::equal( DISCORD_PATH_ENDING.rbegin(), DISCORD_PATH_ENDING.rend(), process_path.rbegin() ) )
			{



				CHAR window_class_name[ 256 ];


				if ( GetClassName( hwnd, window_class_name, RTL_NUMBER_OF( window_class_name ) ) && strcmp( window_class_name, "Chrome_WidgetWin_1" ) == 0 )



					discord_pid = window_pid;
			}
		}




		CloseHandle( process_handle );



		return discord_pid;



	}

	std::vector<uint8_t>
		BuildReplacementHandler(
			void* original_handler,
			std::vector<std::pair<wchar_t,
			uint16_t>>&replacement_table )
	{



		auto keyboard_layout = GetKeyboardLayout( 0 );

		auto key_blocker_section = std::vector<uint8_t>{};




		for ( auto& replacement_entry : replacement_table )
		{


			auto original_char = replacement_entry.first;

			auto virtual_key_code = VkKeyScanExW( original_char, keyboard_layout ) & 0xff;





			if ( virtual_key_code < 0 )



				continue;

			auto key_blocker_code = std::vector<uint8_t>{};



			key_blocker_code.resize( 15 );



			key_blocker_code[ 0 ] = 0x41;



			key_blocker_code[ 1 ] = 0x81;

			key_blocker_code[ 2 ] = 0xF8;

			*( uint32_t* )( key_blocker_code.data() + 3 ) = virtual_key_code;


			key_blocker_code[ 7 ] = 0x75;


			key_blocker_code[ 8 ] = 0x06;


			key_blocker_code[ 9 ] = 0xB8;


			*( uint32_t* )( key_blocker_code.data() + 10 ) = 1;



			key_blocker_code[ 14 ] = 0xC3;



			key_blocker_section.insert( key_blocker_section.end(), key_blocker_code.begin(), key_blocker_code.end() );



		}

		auto key_replacer_section = std::vector<uint8_t>{};


		for ( auto& replacement_entry : replacement_table )


		{


			auto original_char = replacement_entry.first;


			auto replacement_char = replacement_entry.second;



			auto key_replacer_code = std::vector<uint8_t>{};


			key_replacer_code.resize( 16 );

			key_replacer_code[ 0 ] = 0x41;


			key_replacer_code[ 1 ] = 0x81;


			key_replacer_code[ 2 ] = 0xF8;


			*( uint32_t* )( key_replacer_code.data() + 3 ) = original_char;


			key_replacer_code[ 7 ] = 0x75;

			key_replacer_code[ 8 ] = 0x07;

			key_replacer_code[ 9 ] = 0x41;



			key_replacer_code[ 10 ] = 0xC7;


			key_replacer_code[ 11 ] = 0xC0;


			*( uint32_t* )( key_replacer_code.data() + 12 ) = replacement_char;



			key_replacer_section.insert( key_replacer_section.end(), key_replacer_code.begin(), key_replacer_code.end() );


		}

		auto call_original_section = std::array<uint8_t, 14>


		{


			0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,


				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF


		};


		*( uint64_t* )( call_original_section.data() + 6 ) = uint64_t( original_handler );



		auto code =

			std::vector<uint8_t>{};



		code.resize( 41 );

		code[ 0 ] = 0x81;

		code[ 1 ] = 0xFA;



		*( uint32_t* )( code.data() + 2 ) = WM_KEYDOWN;


		code[ 6 ] = 0x0F;


		code[ 7 ] = 0x84;


		*( uint32_t* )( code.data() + 8 ) = 29;



		code[ 12 ] = 0x81;


		code[ 13 ] = 0xFA;


		*( uint32_t* )( code.data() + 14 ) = WM_KEYUP;


		code[ 18 ] = 0x0F;



		code[ 19 ] = 0x84;


		*( uint32_t* )( code.data() + 20 ) = 17;



		code[ 24 ] = 0x81;


		code[ 25 ] = 0xFA;


		*( uint32_t* )( code.data() + 26 ) = WM_CHAR;


		code[ 30 ] = 0x0F;


		code[ 31 ] = 0x84;


		*( uint32_t* )( code.data() + 32 ) = uint32_t( 5 + key_blocker_section.size() + call_original_section.size() );




		code[ 36 ] = 0xE9;


		*( uint32_t* )( code.data() + 37 ) = uint32_t( key_blocker_section.size() );


		code.insert( code.end(), key_blocker_section.begin(), key_blocker_section.end() );


		code.insert( code.end(), call_original_section.begin(), call_original_section.end() );


		code.insert( code.end(), key_replacer_section.begin(), key_replacer_section.end() );


		code.insert( code.end(), call_original_section.begin(), call_original_section.end() );



		return code;


	}



	void* GetVtable( HANDLE process_handle, void* inst )


	{



		void* vtable{};

		if ( ReadProcessMemory
		( process_handle, inst, &vtable, sizeof( void* ),
		  NULL )
			 )
			return
			vtable;

		return
			nullptr;
	}

	void*
		GetVirtualFn
		(
			HANDLE process_handle, void* inst, int index )
	{
		auto
			vtable = GetVtable( process_handle, inst );
		if (
			vtable )
		{
			void*
				fn{};
			if (
				ReadProcessMemory( process_handle, ( void* )( uintptr_t( vtable ) + index * sizeof( void* ) ), &fn, sizeof( void* ), NULL ) )
				return
				fn;
		}

		return
			nullptr;
	}

	bool
		ReplaceVirtualFn
		( HANDLE process_handle, void* inst, int index, void* replace_fn )
	{
		auto
			vtable = GetVtable( process_handle, inst );
		if ( !vtable
			 )
			return
			false;

		auto replace_address
			= ( void* )( uintptr_t( vtable ) + index * sizeof( void* ) );

		auto old_protect
			= DWORD{};


		if (
			!VirtualProtectEx( process_handle, replace_address, sizeof( void* ), PAGE_READWRITE, &old_protect ) )
			return

			false;

		if (


			!WriteProcessMemory( process_handle, replace_address, &replace_fn, sizeof( void* ), NULL ) )
			return
			false;

		if (
			!VirtualProtectEx( process_handle, replace_address, sizeof( void* ), old_protect, &old_protect ) )
			return
			false;

		return
			true;
	}
}

struct ChinifyContext_t

{
	HANDLE discord_handle{};



	HWND discord_main_window{};


	LONG_PTR discord_window_userdata{};


	void* original_handler{};


	void* replacement_handler{};



	struct
	{
		std::atomic_bool question_mark{};


		std::atomic_bool exclamation_mark{};


		std::atomic_bool comma{};


		std::atomic_bool semicolon{};


		std::atomic_bool colon{};


		std::atomic_bool square_bracket{};


		std::atomic_bool period{};


		std::atomic_bool dash{};



		std::atomic_bool wave_dash{};




		std::atomic_bool space{};



		std::atomic_bool run_on_system_startup{};



	} config;




	std::atomic_bool unload{};



} ch_ctx{};

BOOL

CALLBACK

EnumWindowsCallback_FindDiscord

( HWND hwnd, LPARAM lParam )
{
	auto discord_pid =

		FUNCTIONS::FindDiscordByMainWindow( hwnd );
	if ( discord_pid
			> 0 )
	{

		ch_ctx.discord_handle
			=
			OpenProcess( PROCESS_ALL_ACCESS, FALSE, discord_pid );
		if (
			!
			ch_ctx.discord_handle )

			return TRUE;

		ch_ctx.discord_main_window
			= hwnd;

		return
			FALSE;
	}



	return
		TRUE;
}

void

Reset()
{
	if
		(
			ch_ctx.discord_handle )
	{
		if
			(
				ch_ctx.replacement_handler && ch_ctx.discord_window_userdata && ch_ctx.original_handler )
		{
			if (

				FUNCTIONS::ReplaceVirtualFn( ch_ctx.discord_handle,


											 ( void* )ch_ctx.discord_window_userdata,

											 4,

											 ch_ctx.original_handler ) )
			{


				Sleep( 200 );


				VirtualFreeEx(

					ch_ctx.discord_handle,


					ch_ctx.replacement_handler,

					0,

					MEM_RELEASE );
			}
		}

		CloseHandle( ch_ctx.discord_handle );
	}

	ch_ctx.discord_handle = NULL;
	ch_ctx.discord_main_window = NULL;
	ch_ctx.discord_window_userdata = NULL;
	ch_ctx.original_handler = nullptr;
	ch_ctx.replacement_handler = nullptr;
}

void Validate()
{
	if (


		ch_ctx.discord_handle )
	{
		if (

			WaitForSingleObject(


				ch_ctx.discord_handle, 0 ) != WAIT_TIMEOUT )
			Reset();
	}

	if (

		ch_ctx.discord_main_window )
	{
		if ( !

			 IsWindow( ch_ctx.discord_main_window )


			 )
			Reset();



	}

	if (


		ch_ctx.discord_window_userdata

		)
	{
		if (


			!ch_ctx.discord_handle ||

			!ch_ctx.discord_main_window )
			Reset();


		else


		{
			if (


				ch_ctx.discord_window_userdata !=

				GetWindowLongPtr( ch_ctx.discord_main_window, GWLP_USERDATA ) )


				Reset();
		}
	}
}

void Update()


{



	Validate();

	if (

		!

		ch_ctx.discord_handle )
	{
		EnumWindows(


			EnumWindowsCallback_FindDiscord,

			0 );

		if ( !

			 ch_ctx.discord_handle )



			return;
	}

	if ( !


		 ch_ctx.discord_window_userdata )
	{
		ch_ctx.


			discord_window_userdata =


			GetWindowLongPtr(


				ch_ctx.discord_main_window, GWLP_USERDATA );

		if ( !


			 ch_ctx.discord_window_userdata )



			return;
	}

	if ( !



		 ch_ctx.original_handler


		 )
	{
		ch_ctx.

			original_handler = FUNCTIONS::


			GetVirtualFn(

				ch_ctx.discord_handle,

				( void* )ch_ctx.discord_window_userdata, 4 );

		if ( !

			 ch_ctx.

			 original_handler )
			return;
	}

	if ( !

		 ch_ctx.

		 replacement_handler )
	{
		auto replacement_table = std::vector<std::pair<wchar_t, uint16_t>>{};
		if ( ch_ctx.config.

			 question_mark )
			replacement_table.emplace_back( '?', 0xFF1F );
		if ( ch_ctx.config.

			 exclamation_mark )
			replacement_table.emplace_back( '!', 0xFF01 );
		if ( ch_ctx.config.



			 comma )
			replacement_table.emplace_back( ',', 0xFF0C );
		if ( ch_ctx.config.


			 semicolon )
			replacement_table.emplace_back( ';', 0xFF1B );
		if ( ch_ctx.config.


			 colon )
			replacement_table.emplace_back( ':', 0xFF1A );
		if ( ch_ctx.config.


			 square_bracket )
			replacement_table.emplace_back( '[', 0x3010 );
		if ( ch_ctx.config.


			 square_bracket )
			replacement_table.emplace_back( ']', 0x3011 );
		if ( ch_ctx.config.


			 period )
			replacement_table.emplace_back( '.', 0x3002 );
		if ( ch_ctx.


			 config.dash )
			replacement_table.emplace_back( '-', 0x2E3A );
		if ( ch_ctx.config.


			 wave_dash )
			replacement_table.emplace_back( '~', 0x301C );
		if ( ch_ctx.


			 config.space )
			replacement_table.emplace_back( ' ', 0x3000 );

		auto replacement_handler_code = FUNCTIONS::BuildReplacementHandler(




			ch_ctx.original_handler,




			replacement_table );

		auto replacement_handler = VirtualAllocEx(


			ch_ctx.discord_handle, NULL,



			replacement_handler_code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );


		if ( replacement_handler

			 )
		{
			if (


				WriteProcessMemory(

					ch_ctx.discord_handle,


					replacement_handler,


					replacement_handler_code.data(), replacement_handler_code.size(), NULL ) )
			{
				if ( FUNCTIONS::


					 ReplaceVirtualFn( ch_ctx.discord_handle,

									   ( void* )ch_ctx.discord_window_userdata,

									   4,

									   replacement_handler ) )


					ch_ctx.replacement_handler = replacement_handler;
			}

			if (

				!ch_ctx.replacement_handler

				)
				VirtualFreeEx(

					ch_ctx.discord_handle,

					replacement_handler,

					0, MEM_RELEASE );
		}

		if (

			!ch_ctx.replacement_handler )


			return;
	}
}

bool

SetAutoStart(

	bool state )
{
	auto key = winreg::RegKey(


		HKEY_CURRENT_USER,


		L"Software\\Microsoft\\Windows\\CurrentVersion\\Run" );
	if (

		state )
	{
		auto path = std::wstring();
		path.resize(

			MAX_PATH );
		GetModuleFileNameW(


			NULL, path.data(), uint32_t( path.size() ) );

		return key.TrySetStringValue(



			L"Chinify", path ).IsOk();
	}
	else
		return key.

		TryDeleteValue(


			L"Chinify" ).IsOk();
}

bool

HasAutoStart()
{
	auto key =

		winreg::RegKey(


			HKEY_CURRENT_USER,


			L"Software\\Microsoft\\Windows\\CurrentVersion\\Run" );
	return key.

		TryGetStringValue(


			L"Chinify" ).IsValid();
}

bool

SetConfigItem(



	const wchar_t* id, bool state )
{
	auto key = winreg::RegKey(


		HKEY_CURRENT_USER,

		L"Software\\Chinify" );
	return key.

		TrySetDwordValue(


			id, state ? 1 : 0 ).IsOk();
}

bool


GetConfigItem(

	const wchar_t* id,


	bool default_value )
{
	auto key = winreg::

		RegKey(


			HKEY_CURRENT_USER,


			L"Software\\Chinify" );
	auto value = key.

		TryGetDwordValue( id );


	if ( !value )


		return default_value;

	return value.GetValue() != 0;
}

void tray_thread()
{
	auto tray = Tray::

		Tray( "Chinify",


			  ( HICON )LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_ICON2 ) ) );

	tray.addEntry( Tray::


				   Toggle( "Question Mark",


						   ch_ctx.config.question_mark, [ ]( bool state ) { ch_ctx.config.question_mark = state; SetConfigItem( L"Question Mark", state ); } ) );
	tray.addEntry( Tray::


				   Toggle( "Exclamation Mark",


						   ch_ctx.config.exclamation_mark, [ ]( bool state ) { ch_ctx.config.exclamation_mark = state; SetConfigItem( L"Exclamation Mark", state ); } ) );
	tray.addEntry( Tray::


				   Toggle( "Comma",


						   ch_ctx.config.comma, [ ]( bool state ) { ch_ctx.config.comma = state; SetConfigItem( L"Comma", state ); } ) );
	tray.addEntry( Tray::


				   Toggle( "Semi-Colon",



						   ch_ctx.config.semicolon, [ ]( bool state ) { ch_ctx.config.semicolon = state; SetConfigItem( L"Semi-Colon", state ); } ) );
	tray.addEntry( Tray::


				   Toggle( "Colon",



						   ch_ctx.config.colon, [ ]( bool state ) { ch_ctx.config.colon = state; SetConfigItem( L"Colon", state ); } ) );
	tray.addEntry( Tray::


				   Toggle(


					   "Square Brackets", ch_ctx.config.square_bracket, [ ]( bool state ) { ch_ctx.config.square_bracket = state; SetConfigItem( L"Square Brackets", state ); } ) );
	tray.addEntry( Tray::


				   Toggle(

					   "Period", ch_ctx.config.period, [ ]( bool state ) { ch_ctx.config.period = state; SetConfigItem( L"Period", state ); } ) );
	tray.addEntry( Tray::


				   Toggle(

					   "Dash", ch_ctx.config.dash, [ ]( bool state ) { ch_ctx.config.dash = state; SetConfigItem( L"Dash", state ); } ) );
	tray.addEntry( Tray::

				   Toggle(


					   "Wave Dash", ch_ctx.config.wave_dash, [ ]( bool state ) { ch_ctx.config.wave_dash = state; SetConfigItem( L"Wave Dash", state ); } ) );
	tray.addEntry( Tray::


				   Toggle(


					   "Space", ch_ctx.config.space, [ ]( bool state ) { ch_ctx.config.space = state; SetConfigItem( L"Space", state ); } ) );
	tray.addEntry( Tray::Separator() );
	tray.addEntry( Tray::

				   Toggle(


					   "Run on system startup", ch_ctx.config.run_on_system_startup, [ ]( bool state ) { SetAutoStart( state ); } ) );
	tray.addEntry( Tray::

				   Button(

					   "Exit", [ & ] { tray.exit(); } ) );

	tray.run();

	ch_ctx.unload = true;
}

int WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
)
{
	ch_ctx.
		config.

		question_mark =


		GetConfigItem( L"Question Mark", true );
	ch_ctx.


		config.


		exclamation_mark =
		GetConfigItem( L"Exclamation Mark", true );


	ch_ctx.config.comma =


		GetConfigItem( L"Comma", false );
	ch_ctx.


		config.


		semicolon = GetConfigItem( L"Semi-Colon", false );

	ch_ctx.

		config.

		colon = GetConfigItem( L"Colon", false );



	ch_ctx.


		config.


		square_bracket =


		GetConfigItem( L"Square Brackets", true );
	ch_ctx.

		config.

		period =

		GetConfigItem( L"Period", false );
	ch_ctx.config


		.dash =


		GetConfigItem( L"Dash", false );



	ch_ctx.config.wave_dash =


		GetConfigItem( L"Wave Dash", false );
	ch_ctx.config.

		space = GetConfigItem( L"Space", false );

	ch_ctx.config.

		run_on_system_startup = HasAutoStart();

	SetAutoStart(


		ch_ctx.

		config.run_on_system_startup );

	std::thread(


		tray_thread )


		.detach();

	while ( !


			ch_ctx.unload )
	{
		Validate()
			;



		Update();



		Sleep(

			2000 );
	}



	Reset();

	return EXIT_SUCCESS;
}