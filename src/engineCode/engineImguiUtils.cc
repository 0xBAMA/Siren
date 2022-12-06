#include "engine.h"

void engine::QuitConf ( bool *open ) {
	if ( *open ) {
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos( center, 0, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 230, 55 ) );
		ImGui::OpenPopup( "Quit Confirm" );
		if ( ImGui::BeginPopupModal( "Quit Confirm", NULL, ImGuiWindowFlags_NoDecoration ) ) {
			ImGui::Text( "Are you sure you want to quit?" );
			ImGui::Text( "  " );
			ImGui::SameLine();
			// button to cancel -> set this window's bool to false
			if ( ImGui::Button( " Cancel " ) ) {
				*open = false;
			}
			ImGui::SameLine();
			ImGui::Text( "      " );
			ImGui::SameLine();
			// button to quit -> set pquit to true
			if ( ImGui::Button( " Quit " ) ) {
				pQuit = true;
			}
		}
	}
}

void engine::HelpMarker ( const char *desc ) {
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
		ImGui::TextUnformatted( desc );
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void engine::DrawTextEditor () {
	ImGui::Begin( "Editor", NULL, 0 );
	static TextEditor editor;

	static auto language = TextEditor::LanguageDefinition::GLSL();
	editor.SetLanguageDefinition( language );

	auto cursorPosition = editor.GetCursorPosition();
	editor.SetPalette( TextEditor::GetDarkPalette() );

	static const char *fileToEdit = "src/engineCode/shaders/pathtrace.cs.glsl";
	static bool loaded = false;
	if ( !loaded ) {
		std::ifstream t ( fileToEdit );
		editor.SetLanguageDefinition( language );
		if ( t.good() ) {
			editor.SetText( std::string( ( std::istreambuf_iterator< char >( t ) ), std::istreambuf_iterator< char >() ) );
			loaded = true;
		}
		t.close();
	}

	// add dropdown for different shaders? this can be whatever
	ImGui::Text( "%6d/%-6d %6d lines  | %s | %s | %s | %s ", cursorPosition.mLine + 1,
		cursorPosition.mColumn + 1, editor.GetTotalLines(),
		editor.IsOverwrite() ? "Ovr" : "Ins",
		editor.CanUndo() ? "*" : " ",
		editor.GetLanguageDefinitionName(), fileToEdit );
	ImGui::SameLine();
	if ( ImGui::SmallButton( " Hot Recompile " ) ) { // recompile the pathtrace shader out of the editor string
		auto t1 = std::chrono::high_resolution_clock::now();
		computeShader shader( editor.GetText(), computeShader::shaderSource::fromString );
		if ( shader.success ) {
			pathtraceShader = shader.shaderHandle;
			// finish report + report timing
			auto t2 = std::chrono::high_resolution_clock::now();
			shader.report << std::setw( 4 ) << "Done in " << std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count() / 1000.0f << "ms";
			cout << newline << shader.report.str() << newline;
		}
	}
	ImGui::SameLine();
	ImGui::Text( " " );
	ImGui::SameLine();
	if ( ImGui::SmallButton( " Save Shader " ) ) { // overwrite the shader text file
		std::ofstream file( "src/engine_code/shaders/pathtrace.cs.glsl" );
		std::string savetext( editor.GetText() );
		file << savetext;
		file.close();
	}
	ImGui::SameLine();
	ImGui::Text( " " );
	ImGui::SameLine();
	if ( ImGui::SmallButton( " Reload From Disk " ) ) { // reload the file
		std::ifstream t( fileToEdit );
		if ( t.good() ) {
			editor.SetText( std::string( ( std::istreambuf_iterator< char >( t ) ), std::istreambuf_iterator< char >() ) );
			loaded = true;
		}
		t.close();
	}

	editor.Render( "Editor" );
	ImGui::End();
}

// void engine::TonemapControlsWindow () {
// 	ImGui::SetNextWindowSize( { 425, 115 } );
// 	ImGui::Begin( "Tonemapping Controls", NULL, 0 );
// 	const char* tonemapModesList[] = {
// 		"None (Linear)",
// 		"ACES (Narkowicz 2015)",
// 		"Unreal Engine 3",
// 		"Unreal Engine 4",
// 		"Uncharted 2",
// 		"Gran Turismo",
// 		"Modified Gran Turismo",
// 		"Rienhard",
// 		"Modified Rienhard",
// 		"jt",
// 		"robobo1221s",
// 		"robo",
// 		"reinhardRobo",
// 		"jodieRobo",
// 		"jodieRobo2",
// 		"jodieReinhard",
// 		"jodieReinhard2"
// 	};
// 	ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
// 	ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );
// 	ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );
//
// 	ImGui::End();
// }

void engine::ImguiFrameStart () {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame( window );
	ImGui::NewFrame();
}

void engine::ImguiFrameEnd () {
	// get it ready to put on the screen
	ImGui::Render();

	// put imgui data into the framebuffer
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

	// platform windows ( pop out windows )
	ImGuiIO &io = ImGui::GetIO();
	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
	}
}
