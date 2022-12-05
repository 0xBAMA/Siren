#include "engine.h"

int main ( int argc, char *argv[] ) {
	engine engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
