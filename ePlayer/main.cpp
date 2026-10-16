#include "ePlayer.h"
#include <cstdio>

int main(int argc, char* argv[])
{
	ePlayer& player = ePlayer::Get();

	char* fileName = NULL;
	if (argc < 2)
	{
		printf("no media file\n");
		return 0;
	}

	fileName = argv[1];
	if (!player.Init(fileName))
	{
		player.Quit();
		return 0;
	}

	player.OnPlay();

	player.Quit();
}